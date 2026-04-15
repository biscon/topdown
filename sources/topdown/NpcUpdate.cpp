#include <algorithm>
#include <cmath>
#include "NpcUpdate.h"
#include "TopdownHelpers.h"
#include "NpcRegistry.h"
#include "LevelCollision.h"
#include "resources/AsepriteAsset.h"
#include "topdown/TopdownNpcAi.h"
#include "CharacterRender.h"
#include "TopdownRvo.h"
#include "LevelDoors.h"
#include "LevelEffects.h"

static float ComputeRemainingNpcPathDistance(const TopdownNpcMoveState& move, Vector2 currentPosition)
{
    if (!move.active || move.currentPoint >= static_cast<int>(move.pathPoints.size())) {
        return 0.0f;
    }

    float total = TopdownLength(TopdownSub(move.pathPoints[move.currentPoint], currentPosition));

    for (int i = move.currentPoint; i + 1 < static_cast<int>(move.pathPoints.size()); ++i) {
        total += TopdownLength(TopdownSub(move.pathPoints[i + 1], move.pathPoints[i]));
    }

    return total;
}

static TopdownNpcRuntime* FindActiveNpcById(GameState& state, const std::string& npcId)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.active && npc.id == npcId) {
            return &npc;
        }
    }
    return nullptr;
}

static Vector2 BuildNpcFallbackPathVelocity(
        GameState& state,
        const TopdownNpcRuntime& npc)
{
    if (!npc.move.active ||
        npc.move.currentPoint < 0 ||
        npc.move.currentPoint >= static_cast<int>(npc.move.pathPoints.size())) {
        return Vector2{};
    }

    const TopdownNpcAssetRuntime* asset = FindTopdownNpcAssetRuntime(state, npc.assetId);
    const float walkSpeed = asset ? asset->walkSpeed : 450.0f;
    const float runSpeed  = asset ? asset->runSpeed  : 700.0f;
    const float maxSpeed = npc.move.running ? runSpeed : walkSpeed;

    const float speed = std::min(std::max(0.0f, npc.move.currentSpeed), maxSpeed);

    // Look a bit ahead down the corridor instead of steering only to the next point.
    const float lookaheadDistance = std::max(
            80.0f,
            npc.collisionRadius * 3.0f);

    const Vector2 steeringTarget =
            TopdownBuildNpcPathSteeringTarget(npc, lookaheadDistance);

    const Vector2 delta = TopdownSub(steeringTarget, npc.position);
    const float dist = TopdownLength(delta);

    if (dist <= std::max(1.0f, npc.move.arrivalRadius)) {
        return Vector2{};
    }

    const Vector2 dir = TopdownNormalizeOrZero(delta);
    return TopdownMul(dir, speed);
}

static void UpdateNpcMovementAndCollision(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (!npc.active || npc.dead) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (npc.hurtStunRemainingMs > 0.0f) {
        npc.currentVelocity = Vector2{};
        return;
    }

    const Vector2 startPos = npc.position;

    Vector2 velocity{};

    if (TopdownRvoHasAgent(state, npc.handle)) {
        velocity = TopdownRvoGetVelocity(state, npc.handle);
    } else {
        velocity = BuildNpcFallbackPathVelocity(state, npc);
    }

    npc.position = TopdownAdd(npc.position, TopdownMul(velocity, dt));

    const auto& segments = state.topdown.runtime.collision.movementSegments;

    for (int iteration = 0; iteration < kCollisionIterations; ++iteration) {
        for (const TopdownSegment& seg : segments) {
            ResolveCircleVsSegment(
                    npc.position,
                    velocity,
                    npc.collisionRadius,
                    seg);
        }

        ResolveNpcVsDoors(state, npc);

        const bool usingRvo =
                TopdownRvoHasAgent(state, npc.handle) &&
                npc.move.active &&
                npc.move.owner == TopdownNpcMoveOwner::Ai;

        if (!usingRvo) {
            Vector2 preferredVsPlayer = TopdownSub(
                    npc.position,
                    state.topdown.runtime.player.position);

            if (TopdownLengthSqr(preferredVsPlayer) <= 0.000001f &&
                TopdownLengthSqr(velocity) > 0.000001f) {
                preferredVsPlayer = Vector2{ -velocity.y, velocity.x };
            }

            ResolveCircleVsCircle(
                    npc.position,
                    velocity,
                    npc.collisionRadius,
                    state.topdown.runtime.player.position,
                    state.topdown.runtime.player.radius,
                    preferredVsPlayer);
        }
    }

    if (dt > 0.0f) {
        npc.currentVelocity = TopdownMul(
                TopdownSub(npc.position, startPos),
                1.0f / dt);
    } else {
        npc.currentVelocity = Vector2{};
    }
}

static void PrepareSingleNpcPathMovement(GameState& state, TopdownNpcRuntime& npc, float dt)
{
    TopdownNpcMoveState& move = npc.move;

    if (npc.dead) {
        npc.move = {};
        npc.move.owner = TopdownNpcMoveOwner::None;
        npc.moving = false;
        npc.running = false;
        npc.currentVelocity = Vector2{};
        return;
    }

    if (npc.hurtStunRemainingMs > 0.0f) {
        npc.moving = false;
        npc.running = false;
        npc.currentVelocity = Vector2{};
        return;
    }

    if (!move.active) {
        npc.moving = false;
        npc.running = false;
        npc.currentVelocity = Vector2{};
        return;
    }

    while (move.currentPoint < static_cast<int>(move.pathPoints.size())) {
        const Vector2 target = move.pathPoints[move.currentPoint];
        const Vector2 delta = TopdownSub(target, npc.position);
        const float dist = TopdownLength(delta);

        if (dist > move.arrivalRadius) {
            break;
        }

        npc.position = target;
        move.currentPoint++;
    }

    if (move.currentPoint >= static_cast<int>(move.pathPoints.size())) {
        move = {};
        move.owner = TopdownNpcMoveOwner::None;
        npc.moving = false;
        npc.running = false;
        npc.currentVelocity = Vector2{};
        return;
    }

    const TopdownNpcAssetRuntime* asset = FindTopdownNpcAssetRuntime(state, npc.assetId);
    const float walkSpeed = asset ? asset->walkSpeed : 450.0f;
    const float runSpeed  = asset ? asset->runSpeed  : 700.0f;
    const float maxSpeed = move.running ? runSpeed : walkSpeed;

    float targetSpeed = maxSpeed;

    if (move.owner == TopdownNpcMoveOwner::Script) {
        const float remainingDistance = ComputeRemainingNpcPathDistance(move, npc.position);

        if (remainingDistance < move.stopDistance) {
            const float t = std::clamp(
                    remainingDistance / std::max(move.stopDistance, 0.001f),
                    0.0f,
                    1.0f);
            targetSpeed = maxSpeed * t;
        }
    }

    move.currentSpeed = MoveTowardsFloat(
            move.currentSpeed,
            targetSpeed,
            ((targetSpeed > move.currentSpeed) ? move.acceleration : move.deceleration) * dt);

    const float lookaheadDistance = std::max(
            80.0f,
            npc.collisionRadius * 3.0f);

    const Vector2 target = TopdownBuildNpcPathSteeringTarget(npc, lookaheadDistance);
    const Vector2 delta = TopdownSub(target, npc.position);
    const Vector2 dir = TopdownNormalizeOrZero(delta);

    if (TopdownLengthSqr(dir) > 0.000001f) {
        npc.facing = dir;
        npc.rotationRadians = std::atan2(dir.y, dir.x);
        npc.moving = true;
        npc.running = move.running;
    } else {
        npc.moving = false;
        npc.running = false;
        npc.currentVelocity = Vector2{};
    }
}

static void ApplyNpcAiMovement(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (!npc.move.active || npc.move.owner != TopdownNpcMoveOwner::Ai) {
            continue;
        }

        UpdateNpcMovementAndCollision(state, npc, dt);
    }
}

static void ApplyNpcScriptedMovement(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (!npc.move.active || npc.move.owner != TopdownNpcMoveOwner::Script) {
            continue;
        }

        UpdateNpcMovementAndCollision(state, npc, dt);
    }
}

static void PrepareNpcAiMovement(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (!npc.move.active || npc.move.owner != TopdownNpcMoveOwner::Ai) {
            continue;
        }

        PrepareSingleNpcPathMovement(state, npc, dt);
    }
}

static void PrepareNpcScriptedMovement(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (!npc.move.active || npc.move.owner != TopdownNpcMoveOwner::Script) {
            continue;
        }

        PrepareSingleNpcPathMovement(state, npc, dt);
    }
}

static void TopdownUpdateNpcStatusTimers(GameState& state, float dt)
{
    const float dtMs = dt * 1000.0f;

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (npc.hurtStunRemainingMs > 0.0f) {
            npc.hurtStunRemainingMs -= dtMs;
            if (npc.hurtStunRemainingMs < 0.0f) {
                npc.hurtStunRemainingMs = 0.0f;
            }
        }

        if (npc.painSoundCooldownMs > 0.0f) {
            npc.painSoundCooldownMs -= dtMs;
            if (npc.painSoundCooldownMs < 0.0f) {
                npc.painSoundCooldownMs = 0.0f;
            }
        }
    }
}

static void UpdateNpcKnockbackAndCollision(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (!npc.active) {
        npc.currentVelocity = Vector2{};
        return;
    }

    if (npc.corpse) {
        npc.knockbackVelocity = Vector2{};
        npc.currentVelocity = Vector2{};
        return;
    }

    if (TopdownLengthSqr(npc.knockbackVelocity) <= 0.000001f) {
        npc.knockbackVelocity = Vector2{};
        npc.currentVelocity = Vector2{};
        return;
    }

    const Vector2 startPos = npc.position;

    Vector2 velocity = npc.knockbackVelocity;
    npc.position = TopdownAdd(npc.position, TopdownMul(velocity, dt));

    const auto& segments = state.topdown.runtime.collision.movementSegments;

    for (int iteration = 0; iteration < kCollisionIterations; ++iteration) {
        for (const TopdownSegment& seg : segments) {
            ResolveCircleVsSegment(
                    npc.position,
                    velocity,
                    npc.collisionRadius,
                    seg);
        }

        ResolveNpcKnockbackVsDoors(state, npc);

        Vector2 preferredVsPlayer = TopdownSub(
                npc.position,
                state.topdown.runtime.player.position);

        if (TopdownLengthSqr(preferredVsPlayer) <= 0.000001f &&
            TopdownLengthSqr(velocity) > 0.000001f) {
            preferredVsPlayer = Vector2{ -velocity.y, velocity.x };
        }

        ResolveCircleVsCircle(
                npc.position,
                velocity,
                npc.collisionRadius,
                state.topdown.runtime.player.position,
                state.topdown.runtime.player.radius,
                preferredVsPlayer);

        for (const TopdownNpcRuntime& otherNpc : state.topdown.runtime.npcs) {
            if (!otherNpc.active || !otherNpc.visible || otherNpc.corpse) {
                continue;
            }

            if (&otherNpc == &npc) {
                continue;
            }

            Vector2 preferredVsNpc = TopdownSub(npc.position, otherNpc.position);

            if (TopdownLengthSqr(preferredVsNpc) <= 0.000001f &&
                TopdownLengthSqr(velocity) > 0.000001f) {
                preferredVsNpc = Vector2{ -velocity.y, velocity.x };
            }

            ResolveCircleVsCircle(
                    npc.position,
                    velocity,
                    npc.collisionRadius,
                    otherNpc.position,
                    otherNpc.collisionRadius,
                    preferredVsNpc);
        }
    }

    if (dt > 0.0f) {
        npc.currentVelocity = TopdownMul(
                TopdownSub(npc.position, startPos),
                1.0f / dt);
    } else {
        npc.currentVelocity = Vector2{};
    }

    npc.knockbackVelocity = MoveTowardsVector(
            velocity,
            Vector2{},
            npc.knockbackDeceleration * dt);

    if (TopdownLengthSqr(npc.knockbackVelocity) <= 0.000001f) {
        npc.knockbackVelocity = Vector2{};
    }
}

static void UpdateNpcKnockbacks(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        UpdateNpcKnockbackAndCollision(state, npc, dt);
    }
}

static void SetNpcAutomaticLoopFromLocomotion(
        TopdownNpcRuntime& npc,
        const TopdownNpcAssetRuntime& asset)
{
    if (npc.moving) {
        if (npc.running) {
            if (TopdownNpcClipRefIsValid(asset.runClip)) {
                TopdownSetNpcAutomaticLoopAnimation(npc, asset.runClip);
            } else if (TopdownNpcClipRefIsValid(asset.walkClip)) {
                TopdownSetNpcAutomaticLoopAnimation(npc, asset.walkClip);
            } else if (TopdownNpcClipRefIsValid(asset.idleClip)) {
                TopdownSetNpcAutomaticLoopAnimation(npc, asset.idleClip);
            } else {
                npc.automaticLoopClip = {};
                npc.automaticLoopTimeMs = 0.0f;
            }
        } else {
            if (TopdownNpcClipRefIsValid(asset.walkClip)) {
                TopdownSetNpcAutomaticLoopAnimation(npc, asset.walkClip);
            } else if (TopdownNpcClipRefIsValid(asset.idleClip)) {
                TopdownSetNpcAutomaticLoopAnimation(npc, asset.idleClip);
            } else {
                npc.automaticLoopClip = {};
                npc.automaticLoopTimeMs = 0.0f;
            }
        }
    } else {
        if (TopdownNpcClipRefIsValid(asset.idleClip)) {
            TopdownSetNpcAutomaticLoopAnimation(npc, asset.idleClip);
        } else {
            npc.automaticLoopClip = {};
            npc.automaticLoopTimeMs = 0.0f;
        }
    }
}

static void TopdownUpdateNpcAnimationPlayback(GameState& state, float dtMs)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            npc.currentVelocity = Vector2{};
            continue;
        }

        const TopdownNpcAssetRuntime* asset =
                FindTopdownNpcAssetRuntime(state, npc.assetId);

        if (asset == nullptr || !asset->loaded) {
            npc.automaticLoopClip = {};
            TopdownClearNpcScriptLoopAnimation(npc);
            TopdownClearNpcOneShotAnimation(npc);
            npc.currentVelocity = Vector2{};
            continue;
        }

        if (!npc.dead &&
            npc.animationMode == TopdownNpcAnimationMode::AutomaticLocomotion &&
            npc.hurtStunRemainingMs <= 0.0f &&
            !npc.oneShotActive &&
            TopdownLengthSqr(npc.knockbackVelocity) <= 0.000001f) {
            SetNpcAutomaticLoopFromLocomotion(npc, *asset);
        }

        if (TopdownNpcClipRefIsValid(npc.automaticLoopClip)) {
            npc.automaticLoopTimeMs += dtMs;
        }

        if (npc.animationMode == TopdownNpcAnimationMode::ScriptLoop &&
            TopdownNpcClipRefIsValid(npc.scriptLoopClip)) {
            npc.scriptLoopTimeMs += dtMs;
        }

        if (npc.oneShotActive && TopdownNpcClipRefIsValid(npc.oneShotClip)) {
            npc.oneShotTimeMs += dtMs;

            const SpriteAssetResource* sprite =
                    FindSpriteAssetResource(state.resources, npc.oneShotClip.spriteHandle);

            if (sprite == nullptr ||
                !sprite->loaded ||
                npc.oneShotClip.clipIndex < 0 ||
                npc.oneShotClip.clipIndex >= static_cast<int>(sprite->clips.size())) {
                TopdownClearNpcOneShotAnimation(npc);
                npc.currentVelocity = Vector2{};
            } else {
                const SpriteClip& clip = sprite->clips[npc.oneShotClip.clipIndex];
                const float durationMs = GetOneShotClipDurationMs(*sprite, clip);

                if (npc.dead && npc.oneShotTimeMs >= durationMs) {
                    npc.oneShotTimeMs = durationMs;
                } else if (!npc.dead && npc.oneShotTimeMs >= durationMs) {
                    TopdownClearNpcOneShotAnimation(npc);
                }
            }
        }
    }
}

static void TopdownUpdateNpcDeathAndCorpseLifecycle(GameState& state, float dtMs)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (npc.dead && !npc.corpse && npc.oneShotActive && TopdownNpcClipRefIsValid(npc.oneShotClip)) {
            const TopdownNpcClipRef& clipRef = npc.oneShotClip;
            const SpriteAssetResource* sprite =
                    FindSpriteAssetResource(state.resources, clipRef.spriteHandle);
            if (sprite != nullptr &&
                sprite->loaded &&
                clipRef.clipIndex >= 0 &&
                clipRef.clipIndex < static_cast<int>(sprite->clips.size())) {
                const float durationMs = GetOneShotClipDurationMs(*sprite, sprite->clips[clipRef.clipIndex]);
                if (npc.oneShotTimeMs >= durationMs) {
                    npc.corpse = true;
                    npc.knockbackVelocity = Vector2{};
                    npc.move = {};
                    npc.move.owner = TopdownNpcMoveOwner::None;
                    npc.moving = false;
                    npc.running = false;
                    npc.currentVelocity = Vector2{};

                    SpawnBloodPoolEmitter(
                            state,
                            npc.position,
                            RandomRangeFloat(80.0f, 105.0f),
                            RandomRangeFloat(2000.0f, 2500.0f));
                }
            }
        }

        if (npc.corpse && npc.corpseExpirationMs >= 0.0f) {
            npc.corpseElapsedMs += dtMs;

            const float totalLifetimeMs =
                    npc.corpseExpirationMs + kCorpseFadeDurationMs;

            if (npc.corpseElapsedMs >= totalLifetimeMs) {
                npc.active = false;
                npc.visible = false;
                npc.dead = true;
                npc.corpse = false;
                npc.move = {};
                npc.move.owner = TopdownNpcMoveOwner::None;
                npc.moving = false;
                npc.running = false;
                npc.knockbackVelocity = Vector2{};
                npc.currentVelocity = Vector2{};
                TopdownClearNpcOneShotAnimation(npc);
                TopdownClearNpcScriptLoopAnimation(npc);
                npc.automaticLoopClip = {};
                npc.automaticLoopTimeMs = 0.0f;
                TopdownRvoRequestRebuild(state);
            }
        }
    }
}

void TopdownUpdateNpcAnimation(GameState& state, float dt)
{
    const float dtMs = dt * 1000.0f;
    TopdownUpdateNpcAnimationPlayback(state, dtMs);
    TopdownUpdateNpcDeathAndCorpseLifecycle(state, dtMs);
}

void StartNpcKnockback(
        TopdownNpcRuntime& npc,
        Vector2 dir,
        float knockbackDistance)
{
    dir = TopdownNormalizeOrZero(dir);
    if (TopdownLengthSqr(dir) <= 0.000001f) {
        npc.knockbackVelocity = Vector2{};
        npc.currentVelocity = Vector2{};
        return;
    }

    if (knockbackDistance <= 0.0f) {
        npc.knockbackVelocity = Vector2{};
        npc.currentVelocity = Vector2{};
        return;
    }

    const float a = std::max(1.0f, npc.knockbackDeceleration);
    const float initialSpeed = std::sqrt(2.0f * a * knockbackDistance);

    npc.knockbackVelocity = TopdownMul(dir, initialSpeed);
}

void TopdownUpdateNpcLogic(GameState& state, float dt)
{
    // Phase 1: gameplay timers/state that should not live inside animation playback.
    TopdownUpdateNpcStatusTimers(state, dt);

    // Phase 2: AI chooses goals/state and writes desired movement.
    TopdownUpdateNpcAi(state, dt);

    // Phase 3: path-following prep sets facing/running before RVO consumes preferred motion.
    PrepareNpcAiMovement(state, dt);
    PrepareNpcScriptedMovement(state, dt);

    // Phase 4: crowd sim resolves agent motion for AI-owned movers.
    TopdownRvoEnsureReady(state);
    TopdownRvoSync(state);
    TopdownRvoStep(state, dt);

    // Phase 5: apply movement and world collision for both AI and scripted owners.
    ApplyNpcAiMovement(state, dt);
    ApplyNpcScriptedMovement(state, dt);

    // Phase 6: knockback is applied after normal movement so interruption/override is explicit.
    UpdateNpcKnockbacks(state, dt);
}
