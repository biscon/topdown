#include <algorithm>
#include <cmath>
#include "NpcUpdate.h"
#include "TopdownHelpers.h"
#include "NpcRegistry.h"
#include "LevelCollision.h"
#include "resources/AsepriteAsset.h"
#include "topdown/TopdownNpcAi.h"
#include "CharacterRender.h"

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

static Vector2 BuildNpcRuntimeVelocity(const TopdownNpcRuntime& npc)
{
    if (TopdownLengthSqr(npc.knockbackVelocity) > 0.000001f) {
        return npc.knockbackVelocity;
    }

    if (npc.move.active && npc.moving && npc.move.currentSpeed > 0.0f) {
        return TopdownMul(npc.facing, npc.move.currentSpeed);
    }

    return {};
}

static Vector2 BuildNpcOrcaLikeVelocity(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        Vector2 preferredVelocity)
{
    const float preferredSpeed = TopdownLength(preferredVelocity);
    if (preferredSpeed <= 0.000001f) {
        return preferredVelocity;
    }

    const float neighborRadius = 160.0f;
    const float neighborRadiusSqr = neighborRadius * neighborRadius;
    const float predictionTime = 0.45f;
    const float softBuffer = 26.0f;

    const Vector2 preferredDir = TopdownMul(preferredVelocity, 1.0f / preferredSpeed);
    const Vector2 lateralDir{ -preferredDir.y, preferredDir.x };

    Vector2 bestVelocity = preferredVelocity;
    float bestScore = 0.0f;
    bool hasBest = false;

    for (int angleStep = 0; angleStep < 16; ++angleStep) {
        const float angleRadians = (2.0f * PI * static_cast<float>(angleStep)) / 16.0f;
        const float ca = std::cos(angleRadians);
        const float sa = std::sin(angleRadians);

        const Vector2 dir = TopdownNormalizeOrZero(
                TopdownAdd(
                        TopdownMul(preferredDir, ca),
                        TopdownMul(lateralDir, sa)));

        if (TopdownLengthSqr(dir) <= 0.000001f) {
            continue;
        }

        for (int speedBand = 0; speedBand < 3; ++speedBand) {
            float speedScale = 1.0f;
            if (speedBand == 1) {
                speedScale = 0.72f;
            } else if (speedBand == 2) {
                speedScale = 0.38f;
            }

            Vector2 candidateVelocity = TopdownMul(dir, preferredSpeed * speedScale);
            float score = TopdownLengthSqr(TopdownSub(candidateVelocity, preferredVelocity)) * 0.08f;

            for (const TopdownNpcRuntime& otherNpc : state.topdown.runtime.npcs) {
                if (!otherNpc.active || otherNpc.corpse || otherNpc.dead) {
                    continue;
                }

                if (otherNpc.handle == npc.handle) {
                    continue;
                }

                const Vector2 offsetNow = TopdownSub(otherNpc.position, npc.position);
                if (TopdownLengthSqr(offsetNow) > neighborRadiusSqr) {
                    continue;
                }

                const Vector2 otherVelocity = BuildNpcRuntimeVelocity(otherNpc);
                const Vector2 futureSelf =
                        TopdownAdd(npc.position, TopdownMul(candidateVelocity, predictionTime));
                const Vector2 futureOther =
                        TopdownAdd(otherNpc.position, TopdownMul(otherVelocity, predictionTime));

                const float minDist = npc.collisionRadius + otherNpc.collisionRadius;
                const float futureDist = TopdownLength(TopdownSub(futureSelf, futureOther));

                if (futureDist < minDist) {
                    score += (minDist - futureDist) * 9000.0f;
                } else if (futureDist < minDist + softBuffer) {
                    score += (minDist + softBuffer - futureDist) * 380.0f;
                }
            }

            const float forward = TopdownDot(
                    TopdownNormalizeOrZero(candidateVelocity),
                    preferredDir);
            score += (1.0f - forward) * 35.0f;
            score += TopdownLengthSqr(
                    TopdownSub(candidateVelocity, npc.localAvoidanceVelocity)) * 0.02f;

            if (!hasBest || score < bestScore) {
                hasBest = true;
                bestScore = score;
                bestVelocity = candidateVelocity;
            }
        }
    }

    const float stopScore = preferredSpeed * preferredSpeed * 0.3f;
    if (hasBest && bestScore > stopScore) {
        // If every sampled velocity looks like an imminent collision, yield briefly.
        return TopdownMul(preferredVelocity, 0.2f);
    }

    return hasBest ? bestVelocity : preferredVelocity;
}

static void RotateNpcTowardDirection(TopdownNpcRuntime& npc, Vector2 direction, float dt)
{
    if (TopdownLengthSqr(direction) <= 0.000001f) {
        return;
    }

    const Vector2 desiredDir = TopdownNormalizeOrZero(direction);
    if (TopdownLengthSqr(desiredDir) <= 0.000001f) {
        return;
    }

    const float desiredAngle = std::atan2(desiredDir.y, desiredDir.x);
    const float maxTurnRate = 9.0f;
    npc.rotationRadians = MoveTowardsAngle(
            npc.rotationRadians,
            desiredAngle,
            maxTurnRate * dt);

    npc.facing = Vector2{
            std::cos(npc.rotationRadians),
            std::sin(npc.rotationRadians)
    };
    npc.facing = TopdownNormalizeOrZero(npc.facing);
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

static void UpdateNpcMovementAndCollision(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt,
        Vector2 desiredDirection)
{
    if (!npc.active || npc.dead) {
        return;
    }

    if (npc.hurtStunRemainingMs > 0.0f) {
        return;
    }

    Vector2 velocity{};

    if (npc.move.active && npc.moving) {
        Vector2 steerDir = TopdownNormalizeOrZero(desiredDirection);
        if (TopdownLengthSqr(steerDir) <= 0.000001f) {
            steerDir = npc.facing;
        }
        if (TopdownLengthSqr(steerDir) <= 0.000001f) {
            steerDir = Vector2{1.0f, 0.0f};
        }

        const Vector2 preferredVelocity = TopdownMul(steerDir, npc.move.currentSpeed);
        const Vector2 selectedVelocity =
                BuildNpcOrcaLikeVelocity(state, npc, preferredVelocity);

        velocity = TopdownAdd(
                TopdownMul(selectedVelocity, 0.78f),
                TopdownMul(npc.localAvoidanceVelocity, 0.22f));

        const float maxSpeed = std::max(1.0f, npc.move.currentSpeed);
        const float speed = TopdownLength(velocity);
        if (speed > maxSpeed) {
            velocity = TopdownMul(velocity, maxSpeed / speed);
        }

    } else {
        npc.localAvoidanceVelocity = {};
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

        Vector2 preferredVsPlayer = TopdownSub(npc.position, state.topdown.runtime.player.position);

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

    if (TopdownLengthSqr(velocity) <= 0.000001f) {
        npc.localAvoidanceVelocity = {};
    } else {
        npc.localAvoidanceVelocity = velocity;
    }
}

static void UpdateSingleNpcScriptedMovement(GameState& state, TopdownNpcRuntime& npc, float dt)
{
    TopdownNpcMoveState& move = npc.move;

    if (npc.dead) {
        npc.move = {};
        npc.moving = false;
        npc.running = false;
        return;
    }

    if (npc.hurtStunRemainingMs > 0.0f) {
        npc.moving = false;
        npc.running = false;
        return;
    }

    if (!move.active) {
        npc.moving = false;
        npc.running = false;
        return;
    }

    if (move.currentPoint >= static_cast<int>(move.pathPoints.size())) {
        move = {};
        npc.moving = false;
        npc.running = false;
        return;
    }

    const TopdownNpcAssetRuntime* asset = FindTopdownNpcAssetRuntime(state, npc.assetId);

    const float walkSpeed = asset ? asset->walkSpeed : 450.0f;
    const float runSpeed  = asset ? asset->runSpeed  : 700.0f;
    const float maxSpeed = move.running ? runSpeed : walkSpeed;

    const float remainingDistance = ComputeRemainingNpcPathDistance(move, npc.position);

    float targetSpeed = maxSpeed;
    if (remainingDistance < move.stopDistance) {
        const float t = std::clamp(
                remainingDistance / std::max(move.stopDistance, 0.001f),
                0.0f,
                1.0f);
        targetSpeed = maxSpeed * t;
    }

    move.currentSpeed = MoveTowardsFloat(
            move.currentSpeed,
            targetSpeed,
            ((targetSpeed > move.currentSpeed) ? move.acceleration : move.deceleration) * dt);

    while (move.currentPoint < static_cast<int>(move.pathPoints.size())) {
        const Vector2 target = move.pathPoints[move.currentPoint];
        const Vector2 delta = TopdownSub(target, npc.position);
        const float dist = TopdownLength(delta);

        if (dist <= move.arrivalRadius) {
            npc.position = target;
            move.currentPoint++;
            continue;
        }

        const Vector2 dir = TopdownNormalizeOrZero(delta);
        RotateNpcTowardDirection(npc, dir, dt);
        npc.moving = true;
        npc.running = move.running;

        UpdateNpcMovementAndCollision(state, npc, dt, dir);
        return;
    }

    move = {};
    npc.moving = false;
    npc.running = false;
}

static void UpdateNpcScriptedMovement(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        UpdateSingleNpcScriptedMovement(state, npc, dt);
    }
}

static void UpdateNpcAudio(GameState& state, float dt)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        if (npc.painSoundCooldownMs > 0.0f) {
            npc.painSoundCooldownMs -= dt * 1000.0f;
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
        return;
    }

    if (npc.corpse) {
        npc.knockbackVelocity = Vector2{};
        return;
    }

    if (TopdownLengthSqr(npc.knockbackVelocity) <= 0.000001f) {
        npc.knockbackVelocity = Vector2{};
        return;
    }

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

        Vector2 preferredVsPlayer = TopdownSub(npc.position, state.topdown.runtime.player.position);

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

void TopdownUpdateNpcAnimation(GameState& state, float dt)
{
    //static constexpr float kCorpseFadeDurationMs = 2000.0f;
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

        const TopdownNpcAssetRuntime* asset =
                FindTopdownNpcAssetRuntime(state, npc.assetId);

        if (asset == nullptr || !asset->loaded) {
            npc.automaticLoopClip = {};
            TopdownClearNpcScriptLoopAnimation(npc);
            TopdownClearNpcOneShotAnimation(npc);
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
            } else {
                const SpriteClip& clip = sprite->clips[npc.oneShotClip.clipIndex];
                const float durationMs = GetOneShotClipDurationMs(*sprite, clip);

                if (npc.dead && npc.oneShotTimeMs >= durationMs) {
                    npc.oneShotTimeMs = durationMs;
                    npc.corpse = true;
                    npc.knockbackVelocity = Vector2{};
                    npc.move = {};
                    npc.moving = false;
                    npc.running = false;
                } else if (!npc.dead && npc.oneShotTimeMs >= durationMs) {
                    TopdownClearNpcOneShotAnimation(npc);
                }
            }
        }

        if (npc.corpse && npc.active && npc.corpseExpirationMs >= 0.0f) {
            npc.corpseElapsedMs += dtMs;

            const float totalLifetimeMs =
                    npc.corpseExpirationMs + kCorpseFadeDurationMs;

            if (npc.corpseElapsedMs >= totalLifetimeMs) {
                npc.active = false;
                npc.visible = false;
                npc.dead = true;
                npc.corpse = false;
                npc.move = {};
                npc.moving = false;
                npc.running = false;
                npc.knockbackVelocity = Vector2{};
                TopdownClearNpcOneShotAnimation(npc);
                TopdownClearNpcScriptLoopAnimation(npc);
                npc.automaticLoopClip = {};
                npc.automaticLoopTimeMs = 0.0f;
            }
        }
    }
}

void StartNpcKnockback(
        TopdownNpcRuntime& npc,
        Vector2 dir,
        float knockbackDistance)
{
    dir = TopdownNormalizeOrZero(dir);
    if (TopdownLengthSqr(dir) <= 0.000001f) {
        return;
    }

    if (knockbackDistance <= 0.0f) {
        npc.knockbackVelocity = Vector2{};
        return;
    }

    const float a = std::max(1.0f, npc.knockbackDeceleration);
    const float initialSpeed = std::sqrt(2.0f * a * knockbackDistance);

    npc.knockbackVelocity = TopdownMul(dir, initialSpeed);
}


void TopdownUpdateNpcLogic(GameState& state, float dt)
{
    TopdownUpdateNpcAi(state, dt);
    UpdateNpcScriptedMovement(state, dt);
    UpdateNpcKnockbacks(state, dt);
    UpdateNpcAudio(state, dt);
}
