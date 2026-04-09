#include "topdown/PlayerUpdate.h"

#include <cmath>
#include <string>
#include <algorithm>

#include "topdown/TopdownHelpers.h"
#include "topdown/PlayerLoad.h"
#include "NpcRegistry.h"
#include "resources/AsepriteAsset.h"
#include "LevelCollision.h"
#include "LevelDoors.h"

static float ClampFloat(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static bool IsScriptedMoveActive(const GameState& state)
{
    return state.topdown.runtime.scriptedMove.active;
}

static SpriteAssetHandle FirstValidHandle(
        SpriteAssetHandle a,
        SpriteAssetHandle b = -1,
        SpriteAssetHandle c = -1)
{
    if (a >= 0) return a;
    if (b >= 0) return b;
    if (c >= 0) return c;
    return -1;
}

static void UpdatePlayerDamageRuntime(GameState& state, float dt)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const float dtMs = dt * 1000.0f;

    if (player.hurtCooldownRemainingMs > 0.0f) {
        player.hurtCooldownRemainingMs -= dtMs;
        if (player.hurtCooldownRemainingMs < 0.0f) {
            player.hurtCooldownRemainingMs = 0.0f;
        }
    }

    if (player.hitSlowdownRemainingMs > 0.0f) {
        player.hitSlowdownRemainingMs -= dtMs;

        if (player.hitSlowdownRemainingMs < 0.0f) {
            player.hitSlowdownRemainingMs = 0.0f;
        }
    }

    if (player.hitSlowdownRemainingMs <= 0.0f) {
        player.hitSlowdownMultiplier = 1.0f;
    }

    if (player.damageFlashRemainingMs > 0.0f) {
        player.damageFlashRemainingMs -= dtMs;
        if (player.damageFlashRemainingMs < 0.0f) {
            player.damageFlashRemainingMs = 0.0f;
        }
    }

    if (player.maxHealth > 0.0f) {
        const float health01 = ClampFloat(player.health / player.maxHealth, 0.0f, 1.0f);
        player.lowHealthEffectWeight = 1.0f - health01;
    } else {
        player.lowHealthEffectWeight = 0.0f;
    }
}

static void UpdatePlayerFeetAnimation(GameState& state)
{
    TopdownCharacterRuntime& runtime = state.topdown.runtime.playerCharacter;

    switch (runtime.locomotion) {
        case TopdownLocomotionType::Idle:
            runtime.currentFeetHandle = FindTopdownPlayerFeetAnimationHandle(state, "idle");
            break;

        case TopdownLocomotionType::Forward:
            runtime.currentFeetHandle = runtime.running
                                        ? FirstValidHandle(
                            FindTopdownPlayerFeetAnimationHandle(state, "run"),
                            FindTopdownPlayerFeetAnimationHandle(state, "walk"),
                            FindTopdownPlayerFeetAnimationHandle(state, "idle"))
                                        : FirstValidHandle(
                            FindTopdownPlayerFeetAnimationHandle(state, "walk"),
                            FindTopdownPlayerFeetAnimationHandle(state, "idle"));
            break;

        case TopdownLocomotionType::Backward:
            runtime.currentFeetHandle = FirstValidHandle(
                    FindTopdownPlayerFeetAnimationHandle(state, "walk"),
                    FindTopdownPlayerFeetAnimationHandle(state, "idle"));
            runtime.running = false;
            break;

        case TopdownLocomotionType::StrafeLeft:
            runtime.currentFeetHandle = FirstValidHandle(
                    FindTopdownPlayerFeetAnimationHandle(state, "strafe_left"),
                    FindTopdownPlayerFeetAnimationHandle(state, "walk"),
                    FindTopdownPlayerFeetAnimationHandle(state, "idle"));
            runtime.running = false;
            break;

        case TopdownLocomotionType::StrafeRight:
            runtime.currentFeetHandle = FirstValidHandle(
                    FindTopdownPlayerFeetAnimationHandle(state, "strafe_right"),
                    FindTopdownPlayerFeetAnimationHandle(state, "walk"),
                    FindTopdownPlayerFeetAnimationHandle(state, "idle"));
            runtime.running = false;
            break;
    }
}

static void UpdatePlayerUpperAnimation(GameState& state)
{
    TopdownCharacterRuntime& runtime = state.topdown.runtime.playerCharacter;
    TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    const std::string& equipmentSetId = runtime.equippedSetId;
    if (equipmentSetId.empty()) {
        runtime.currentUpperHandle = -1;
        return;
    }

    if (attack.active) {
        const SpriteAssetHandle attackHandle =
                FindTopdownPlayerEquipmentAttackAnimationHandle(
                        state,
                        equipmentSetId,
                        attack.attackType);

        if (attackHandle >= 0) {
            runtime.currentUpperHandle = attackHandle;
            return;
        }
    }

    const bool moving =
            runtime.locomotion != TopdownLocomotionType::Idle;

    if (moving) {
        runtime.currentUpperHandle = FirstValidHandle(
                FindTopdownPlayerEquipmentAnimationHandle(state, equipmentSetId, "move"),
                FindTopdownPlayerEquipmentAnimationHandle(state, equipmentSetId, "idle"));
    } else {
        runtime.currentUpperHandle = FirstValidHandle(
                FindTopdownPlayerEquipmentAnimationHandle(state, equipmentSetId, "idle"),
                FindTopdownPlayerEquipmentAnimationHandle(state, equipmentSetId, "move"));
    }
}

void TopdownUpdatePlayerAnimation(GameState& state, float dt)
{
    TopdownCharacterRuntime& runtime = state.topdown.runtime.playerCharacter;
    const TopdownCharacterAssetData& asset = state.topdown.playerCharacterAsset;
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    if (!runtime.active || !asset.loaded) {
        return;
    }

    runtime.feetAnimationTimeMs += dt * 1000.0f;
    runtime.upperAnimationTimeMs += dt * 1000.0f;

    if (IsScriptedMoveActive(state)) {
        if (TopdownLengthSqr(player.facing) > 0.0001f) {
            const float scriptedAngle = std::atan2(player.facing.y, player.facing.x);

            runtime.desiredAimRadians = scriptedAngle;
            runtime.bodyFacingRadians = MoveTowardsAngle(
                    runtime.bodyFacingRadians,
                    scriptedAngle,
                    runtime.turnSpeedRadians * dt * 1.5f);

            runtime.feetRotationRadians = runtime.bodyFacingRadians;
            runtime.upperRotationRadians = runtime.bodyFacingRadians;
            runtime.aimFrozen = true;
        }
    } else {
        const Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld{
                mouseScreen.x + state.topdown.runtime.camera.position.x,
                mouseScreen.y + state.topdown.runtime.camera.position.y
        };

        const Vector2 aimDelta = TopdownSub(mouseWorld, player.position);
        const float aimDistSqr = TopdownLengthSqr(aimDelta);
        const float enterDistSqr = runtime.minAimDistanceEnter * runtime.minAimDistanceEnter;
        const float exitDistSqr = runtime.minAimDistanceExit * runtime.minAimDistanceExit;

        if (runtime.aimFrozen) {
            if (aimDistSqr >= exitDistSqr) {
                runtime.aimFrozen = false;
            }
        } else {
            if (aimDistSqr <= enterDistSqr) {
                runtime.aimFrozen = true;
            }
        }

        if (!runtime.aimFrozen && aimDistSqr > 0.0001f) {
            runtime.desiredAimRadians = std::atan2(aimDelta.y, aimDelta.x);
        }

        runtime.bodyFacingRadians = MoveTowardsAngle(
                runtime.bodyFacingRadians,
                runtime.desiredAimRadians,
                runtime.turnSpeedRadians * dt);

        runtime.feetRotationRadians = runtime.bodyFacingRadians;

        float upperDelta = NormalizeAngleRadians(runtime.desiredAimRadians - runtime.bodyFacingRadians);
        upperDelta = ClampFloat(
                upperDelta,
                -runtime.maxUpperBodyTwistRadians,
                runtime.maxUpperBodyTwistRadians);

        runtime.upperRotationRadians = runtime.bodyFacingRadians + upperDelta;
    }

    runtime.running = false;
    runtime.locomotion = TopdownLocomotionType::Idle;

    const float localForward = player.moveInputForward;
    const float localRight = player.moveInputRight;

    if (std::fabs(localRight) > std::fabs(localForward) && std::fabs(localRight) > 0.25f) {
        runtime.locomotion =
                (localRight < 0.0f)
                ? TopdownLocomotionType::StrafeLeft
                : TopdownLocomotionType::StrafeRight;
    } else if (localForward < -0.1f) {
        runtime.locomotion = TopdownLocomotionType::Backward;
    } else if (localForward > 0.1f) {
        runtime.locomotion = TopdownLocomotionType::Forward;
        runtime.running = player.wantsRun;
    } else if (TopdownLengthSqr(player.velocity) > 0.0001f) {
        runtime.locomotion = TopdownLocomotionType::Forward;
    }

    UpdatePlayerFeetAnimation(state);
    UpdatePlayerUpperAnimation(state);
}

static float ComputeRemainingPathDistance(const TopdownScriptMoveState& move, Vector2 currentPosition)
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

static void UpdatePlayerMovementAndCollision(GameState& state, float dt)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;

    player.position = TopdownAdd(player.position, TopdownMul(player.velocity, dt));

    const auto& segments = state.topdown.runtime.collision.movementSegments;

    for (int iteration = 0; iteration < kCollisionIterations; ++iteration) {
        for (const TopdownSegment& seg : segments) {
            ResolveCircleVsSegment(
                    player.position,
                    player.velocity,
                    player.radius,
                    seg);
        }

        ResolvePlayerVsDoors(state);

        for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
            if (!npc.active || !npc.visible || npc.corpse) {
                continue;
            }

            Vector2 preferredSeparation = TopdownSub(player.position, npc.position);

            if (TopdownLengthSqr(preferredSeparation) <= 0.000001f &&
                TopdownLengthSqr(player.velocity) > 0.000001f) {
                preferredSeparation = Vector2{ -player.velocity.y, player.velocity.x };
            }

            ResolveCircleVsCircle(
                    player.position,
                    player.velocity,
                    player.radius,
                    npc.position,
                    npc.collisionRadius,
                    preferredSeparation);
        }
    }
}

static void UpdateScriptedPlayerMovement(GameState& state, float dt)
{
    TopdownScriptMoveState& move = state.topdown.runtime.scriptedMove;
    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    if (!move.active) {
        return;
    }

    if (move.currentPoint >= static_cast<int>(move.pathPoints.size())) {
        move = {};
        player.velocity = Vector2{};
        player.desiredVelocity = Vector2{};
        player.wantsRun = false;
        return;
    }

    const float maxSpeed = move.running ? player.runSpeed : player.walkSpeed;
    const float remainingDistance = ComputeRemainingPathDistance(move, player.position);

    float targetSpeed = maxSpeed;
    if (remainingDistance < move.stopDistance) {
        const float t = std::clamp(remainingDistance / std::max(move.stopDistance, 0.001f), 0.0f, 1.0f);
        targetSpeed = maxSpeed * t;
    }

    move.currentSpeed = MoveTowardsFloat(
            move.currentSpeed,
            targetSpeed,
            ((targetSpeed > move.currentSpeed) ? move.acceleration : move.deceleration) * dt);

    while (move.currentPoint < static_cast<int>(move.pathPoints.size())) {
        const Vector2 target = move.pathPoints[move.currentPoint];
        const Vector2 delta = TopdownSub(target, player.position);
        const float dist = TopdownLength(delta);

        if (dist <= move.arrivalRadius) {
            player.position = target;
            move.currentPoint++;
            continue;
        }

        const Vector2 dir = TopdownNormalizeOrZero(delta);

        player.velocity = TopdownMul(dir, move.currentSpeed);
        player.desiredVelocity = player.velocity;
        player.facing = dir;
        player.wantsRun = move.running;
        player.moveInputForward = 1.0f;
        player.moveInputRight = 0.0f;

        const float desiredAngle = std::atan2(dir.y, dir.x);
        character.bodyFacingRadians = MoveTowardsAngle(
                character.bodyFacingRadians,
                desiredAngle,
                character.turnSpeedRadians * dt * 1.5f);
        character.feetRotationRadians = character.bodyFacingRadians;
        character.upperRotationRadians = character.bodyFacingRadians;
        character.desiredAimRadians = character.bodyFacingRadians;
        character.aimFrozen = true;

        UpdatePlayerMovementAndCollision(state, dt);

        return;
    }

    move = {};
    player.velocity = Vector2{};
    player.desiredVelocity = Vector2{};
    player.wantsRun = false;
    player.moveInputForward = 0.0f;
    player.moveInputRight = 0.0f;
}

static void UpdatePlayerMovementIntent(GameState& state)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    float inputForward = 0.0f;
    float inputRight = 0.0f;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        inputForward += 1.0f;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        inputForward -= 1.0f;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        inputRight += 1.0f;
    }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        inputRight -= 1.0f;
    }

    Vector2 localInput{inputRight, inputForward};
    localInput = TopdownNormalizeOrZero(localInput);

    player.moveInputRight = localInput.x;
    player.moveInputForward = localInput.y;
    player.wantsRun = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (TopdownLengthSqr(localInput) <= 0.000001f) {
        player.desiredVelocity = Vector2{};
        return;
    }

    Vector2 forward{
            std::cos(character.bodyFacingRadians),
            std::sin(character.bodyFacingRadians)
    };
    forward = TopdownNormalizeOrZero(forward);

    if (TopdownLengthSqr(forward) <= 0.000001f) {
        forward = Vector2{1.0f, 0.0f};
    }

    Vector2 right{ -forward.y, forward.x };

    Vector2 moveDir = TopdownAdd(
            TopdownMul(forward, localInput.y),
            TopdownMul(right, localInput.x));
    moveDir = TopdownNormalizeOrZero(moveDir);

    float desiredSpeed = player.walkSpeed;

    if (localInput.y < -0.1f) {
        desiredSpeed = player.backwardSpeed;
    } else if (std::fabs(localInput.x) > 0.1f && std::fabs(localInput.y) < 0.25f) {
        desiredSpeed = player.strafeSpeed;
    } else if (player.wantsRun) {
        desiredSpeed = player.runSpeed;
    } else {
        desiredSpeed = player.walkSpeed;
    }

    if (player.hitSlowdownRemainingMs > 0.0f) {
        desiredSpeed *= ClampFloat(player.hitSlowdownMultiplier, 0.0f, 1.0f);
    }

    player.desiredVelocity = TopdownMul(moveDir, desiredSpeed);

    if (TopdownLengthSqr(moveDir) > 0.000001f) {
        player.facing = moveDir;
    }
}

static void UpdatePlayerVelocity(TopdownPlayerRuntime& player, float dt)
{
    const float desiredSpeedSqr = TopdownLengthSqr(player.desiredVelocity);
    const float rate = (desiredSpeedSqr > 0.000001f) ? player.acceleration : player.deceleration;
    const float maxDelta = rate * dt;

    player.velocity = MoveTowardsVector(player.velocity, player.desiredVelocity, maxDelta);
}

void TopdownUpdatePlayerLogic(GameState& state, float dt)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;

    UpdatePlayerDamageRuntime(state, dt);

    if (state.topdown.runtime.scriptedMove.active) {
        UpdateScriptedPlayerMovement(state, dt);
    } else {
        const bool inputBlocked =
                !state.topdown.runtime.controlsEnabled ||
                state.debug.console.open;

        if (!inputBlocked) {
            UpdatePlayerMovementIntent(state);
        } else {
            player.moveInputForward = 0.0f;
            player.moveInputRight = 0.0f;
            player.wantsRun = false;
            player.desiredVelocity = Vector2{};
        }

        UpdatePlayerVelocity(player, dt);
        UpdatePlayerMovementAndCollision(state, dt);
    }
}
