#include "topdown/TopdownNpcAi.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "topdown/TopdownHelpers.h"
#include "topdown/NpcRegistry.h"
#include "topdown/LevelCollision.h"
#include "nav/NavMeshQuery.h"
#include "resources/AsepriteAsset.h"
#include "LevelDoors.h"
#include "raymath.h"

const char* TopdownNpcAwarenessStateToString(TopdownNpcAwarenessState state)
{
    switch (state) {
        case TopdownNpcAwarenessState::Idle:       return "Idle";
        case TopdownNpcAwarenessState::Suspicious: return "Suspicious";
        case TopdownNpcAwarenessState::Alerted:    return "Alerted";
        default:                                   return "Unknown";
    }
}

const char* TopdownNpcCombatStateToString(TopdownNpcCombatState state)
{
    switch (state) {
        case TopdownNpcCombatState::None:    return "None";
        case TopdownNpcCombatState::Chase:   return "Chase";
        case TopdownNpcCombatState::Investigation: return "Investigation";
        case TopdownNpcCombatState::Attack:  return "Attack";
        case TopdownNpcCombatState::Recover: return "Recover";
        case TopdownNpcCombatState::Search:  return "Search";
        default:                             return "Unknown";
    }
}

void TopdownStopNpcMovement(TopdownNpcRuntime& npc)
{
    npc.move = {};
    npc.move.owner = TopdownNpcMoveOwner::None;
    npc.moving = false;
    npc.running = false;
    npc.currentVelocity = Vector2{};
}

bool TopdownHasNpcReachedPoint(
        const TopdownNpcRuntime& npc,
        Vector2 point,
        float radius)
{
    const float distSqr =
            TopdownLengthSqr(TopdownSub(point, npc.position));
    return distSqr <= radius * radius;
}

void TopdownResetNpcSearchTimers(TopdownNpcRuntime& npc)
{
    npc.searchStateTimeMs = 0.0f;
    npc.searchDurationMs = 0.0f;
    npc.searchBaseFacingRadians = 0.0f;
    npc.searchSweepDegrees = 0.0f;
}

void TopdownResetNpcLostTargetProgress(TopdownNpcRuntime& npc)
{
    npc.lostTargetProgressTimerMs = 0.0f;
    npc.lostTargetLastDistance = 0.0f;
}

void TopdownResetNpcChaseStuckWatchdog(TopdownNpcRuntime& npc)
{
    npc.chaseStuckTimerMs = 0.0f;
    npc.chaseStuckLastPosition = npc.position;
}

bool TopdownHasNpcReachedLastKnownTarget(
        const TopdownNpcRuntime& npc,
        float arriveRadius)
{
    return TopdownHasNpcReachedPoint(
            npc,
            npc.lastKnownPlayerPosition,
            arriveRadius);
}

void TopdownFinishNpcSearchAndForgetTarget(TopdownNpcRuntime& npc)
{
    npc.hasPlayerTarget = false;
    npc.loseTargetTimerMs = 0.0f;
    npc.repathTimerMs = 0.0f;
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
    npc.combatState = TopdownNpcCombatState::None;

    TopdownResetNpcLostTargetProgress(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);
    TopdownResetNpcSearchTimers(npc);
    npc.investigationContextHandle = -1;
    npc.investigationSlotIndex = -1;
    npc.investigationProgressTimerMs = 0.0f;
    npc.investigationLastDistance = 0.0f;
    TopdownStopNpcMovement(npc);
}

void TopdownBeginNpcSearchState(
        TopdownNpcRuntime& npc,
        float durationMs,
        float sweepDegrees)
{
    TopdownStopNpcMovement(npc);
    TopdownResetNpcChaseStuckWatchdog(npc);

    npc.combatState = TopdownNpcCombatState::Search;
    npc.searchStateTimeMs = 0.0f;
    npc.searchDurationMs = durationMs;
    npc.searchSweepDegrees = sweepDegrees;

    Vector2 facing = TopdownNormalizeOrZero(npc.facing);
    if (TopdownLengthSqr(facing) <= 0.000001f) {
        facing = Vector2{
                std::cos(npc.rotationRadians),
                std::sin(npc.rotationRadians)
        };
        facing = TopdownNormalizeOrZero(facing);
    }

    if (TopdownLengthSqr(facing) <= 0.000001f) {
        facing = Vector2{1.0f, 0.0f};
    }

    npc.searchBaseFacingRadians = std::atan2(facing.y, facing.x);
    npc.rotationRadians = npc.searchBaseFacingRadians;
    npc.facing = facing;
}

bool TopdownUpdateNpcChaseStuckWatchdog(
        TopdownNpcRuntime& npc,
        float dtMs,
        float probePeriodMs,
        float minDistancePerProbe)
{
    // Periodically sample chase movement progress; callers treat "moved too little" as stuck.
    if (npc.chaseStuckTimerMs <= 0.0f) {
        npc.chaseStuckLastPosition = npc.position;
        npc.chaseStuckTimerMs = dtMs;
        return false;
    }

    npc.chaseStuckTimerMs += dtMs;

    if (npc.chaseStuckTimerMs < probePeriodMs) {
        return false;
    }

    const float movedDistance =
            TopdownLength(
                    TopdownSub(
                            npc.position,
                            npc.chaseStuckLastPosition));

    const bool movedTooLittle = movedDistance < minDistancePerProbe;

    npc.chaseStuckLastPosition = npc.position;
    npc.chaseStuckTimerMs = 0.0f;

    return movedTooLittle;
}

bool TopdownTryBuildNpcChaseTarget(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer,
        Vector2& outChaseTarget)
{
    if (!npc.hasPlayerTarget) {
        return false;
    }

    if (npc.persistentChase) {
        outChaseTarget = state.topdown.runtime.player.position;
        return true;
    }

    if (currentlyDetectsPlayer) {
        outChaseTarget = state.topdown.runtime.player.position;
        return true;
    }

    outChaseTarget = npc.lastKnownPlayerPosition;
    return true;
}

static float SmoothStep01(float t)
{
    t = Clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void TopdownUpdateNpcSearchState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (npc.combatState != TopdownNpcCombatState::Search) {
        return;
    }

    npc.searchStateTimeMs += dt * 1000.0f;

    const float durationMs = std::max(1.0f, npc.searchDurationMs);
    const float totalT = Clamp(npc.searchStateTimeMs / durationMs, 0.0f, 1.0f);

    const float halfSweepRadians =
            (npc.searchSweepDegrees * 0.5f) * DEG2RAD;

    float signedOffsetRadians = 0.0f;

    // Sweep left -> right -> return to center over normalized search duration.
    if (totalT < (1.0f / 3.0f)) {
        const float t = SmoothStep01(totalT / (1.0f / 3.0f));
        signedOffsetRadians = Lerp(0.0f, -halfSweepRadians, t);
    } else if (totalT < (2.0f / 3.0f)) {
        const float t = SmoothStep01((totalT - (1.0f / 3.0f)) / (1.0f / 3.0f));
        signedOffsetRadians = Lerp(-halfSweepRadians, halfSweepRadians, t);
    } else {
        const float t = SmoothStep01((totalT - (2.0f / 3.0f)) / (1.0f / 3.0f));
        signedOffsetRadians = Lerp(halfSweepRadians, 0.0f, t);
    }

    const float newAngle =
            NormalizeAngleRadians(npc.searchBaseFacingRadians + signedOffsetRadians);

    npc.rotationRadians = newAngle;
    npc.facing = Vector2{
            std::cos(newAngle),
            std::sin(newAngle)
    };

    if (TopdownNpcCanSeePlayer(state, npc) ||
        TopdownNpcCanHearPlayer(state, npc)) {
        TopdownAlertNpcToPlayer(state, npc);
        npc.combatState = TopdownNpcCombatState::Chase;
        TopdownResetNpcSearchTimers(npc);
        return;
    }

    if (npc.searchStateTimeMs >= durationMs) {
        npc.rotationRadians = npc.searchBaseFacingRadians;
        npc.facing = Vector2{
                std::cos(npc.searchBaseFacingRadians),
                std::sin(npc.searchBaseFacingRadians)
        };

        TopdownFinishNpcSearchAndForgetTarget(npc);
    }
}


static bool HasDoorBetweenPoints(
        GameState& state,
        Vector2 from,
        Vector2 to)
{
    Vector2 delta = TopdownSub(to, from);
    const float dist = TopdownLength(delta);

    if (dist <= 0.000001f) {
        return false;
    }

    const Vector2 dir = TopdownMul(delta, 1.0f / dist);

    TopdownRuntimeDoor* hitDoor = nullptr;
    Vector2 hitPoint{};
    Vector2 hitNormal{};
    float hitDistance = dist;

    return RaycastClosestDoor(
            state,
            from,
            dir,
            dist,
            hitDoor,
            hitPoint,
            hitNormal,
            hitDistance);
}

void TopdownAlertNpcToPlayer(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    if (!npc.active || npc.dead || npc.corpse) {
        return;
    }

    if (!npc.hostile) {
        return;
    }

    if (npc.aiMode != TopdownNpcAiMode::SeekAndDestroy) {
        return;
    }

    if (npc.combatState == TopdownNpcCombatState::Investigation) {
        TopdownLeaveNpcInvestigationState(state, npc);
    }

    const bool newlyAcquiredTarget = !npc.hasPlayerTarget;
    npc.hasPlayerTarget = true;
    npc.lastKnownPlayerPosition = state.topdown.runtime.player.position;
    npc.loseTargetTimerMs = 0.0f;
    npc.awarenessState = TopdownNpcAwarenessState::Alerted;
    TopdownResetNpcLostTargetProgress(npc);
    if (newlyAcquiredTarget) {
        npc.repathTimerMs = 0.0f;
    }
}

void TopdownAlertNearbyNpcs(
        GameState& state,
        const TopdownNpcRuntime& sourceNpc,
        float radius)
{
    if (radius <= 0.0f) {
        return;
    }

    const float radiusSqr = radius * radius;

    for (TopdownNpcRuntime& otherNpc : state.topdown.runtime.npcs) {
        if (!otherNpc.active || otherNpc.dead || otherNpc.corpse) {
            continue;
        }

        if (&otherNpc == &sourceNpc) {
            continue;
        }

        if (!otherNpc.hostile) {
            continue;
        }

        if (otherNpc.aiMode != TopdownNpcAiMode::SeekAndDestroy) {
            continue;
        }

        const float distSqr =
                TopdownLengthSqr(TopdownSub(otherNpc.position, sourceNpc.position));

        if (distSqr > radiusSqr) {
            continue;
        }

        if (!TopdownNpcHasLineOfSightToNpc(state, sourceNpc, otherNpc)) {
            continue;
        }

        TopdownAlertNpcToPlayer(state, otherNpc);
        if (otherNpc.combatState != TopdownNpcCombatState::Attack) {
            otherNpc.combatState = TopdownNpcCombatState::Chase;
        }
    }
}

void TopdownAlertNpcsByGunshot(
        GameState& state,
        Vector2 shotOrigin)
{
    static constexpr float kAspectRatioCompensation =
            static_cast<float>(INTERNAL_HEIGHT) / static_cast<float>(INTERNAL_WIDTH);

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || npc.dead || npc.corpse) {
            continue;
        }

        if (!npc.hostile) {
            continue;
        }

        if (npc.aiMode != TopdownNpcAiMode::SeekAndDestroy) {
            continue;
        }

        const float range = std::max(0.0f, npc.gunshotHearingRange);
        if (range <= 0.0f) {
            continue;
        }

        const Vector2 delta = TopdownSub(npc.position, shotOrigin);
        const Vector2 weightedDelta{
                delta.x * kAspectRatioCompensation,
                delta.y
        };

        if (TopdownLengthSqr(weightedDelta) > range * range) {
            continue;
        }

        TopdownAlertNpcToPlayer(state, npc);
        if (npc.combatState != TopdownNpcCombatState::Attack) {
            npc.combatState = TopdownNpcCombatState::Chase;
        }
    }
}

bool TopdownIsPlayerAlive(const GameState& state)
{
    return state.topdown.runtime.player.lifeState == TopdownPlayerLifeState::Alive;
}

static bool RaycastPlayerDetailed(
        GameState& state,
        Vector2 origin,
        Vector2 dir,
        float maxDistance,
        float& outDistance,
        Vector2& outHitPoint,
        Vector2& outHitNormal)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    if (!RaycastCircleDetailed(
            origin,
            dir,
            player.position,
            player.radius,
            maxDistance,
            outDistance,
            outHitPoint,
            outHitNormal)) {
        return false;
    }

    Vector2 wallPoint{};
    Vector2 wallNormal{};
    float wallDistance = outDistance;

    if (RaycastClosestSegmentWithNormal(
            origin,
            dir,
            state.topdown.runtime.collision.visionSegments,
            outDistance,
            wallPoint,
            wallNormal,
            wallDistance)) {
        return false;
    }

    wallDistance = outDistance;
    if (RaycastClosestSegmentWithNormal(
            origin,
            dir,
            state.topdown.runtime.collision.boundarySegments,
            outDistance,
            wallPoint,
            wallNormal,
            wallDistance)) {
        return false;
    }

    TopdownRuntimeDoor* hitDoor = nullptr;
    float doorDistance = outDistance;
    if (RaycastClosestDoor(
            state,
            origin,
            dir,
            outDistance,
            hitDoor,
            wallPoint,
            wallNormal,
            doorDistance)) {
        return false;
    }

    return true;
}

static bool HasNpcLineOfSightToPlayer(
        GameState& state,
        const TopdownNpcRuntime& npc,
        float maxDistance)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const Vector2 toPlayer = TopdownSub(player.position, npc.position);
    const float dist = TopdownLength(toPlayer);

    if (dist <= 0.000001f) {
        return true;
    }

    if (dist > maxDistance) {
        return false;
    }

    const Vector2 dir = TopdownMul(toPlayer, 1.0f / dist);

    float hitDistance = 0.0f;
    Vector2 hitPoint{};
    Vector2 hitNormal{};
    return RaycastPlayerDetailed(state, npc.position, dir, maxDistance, hitDistance, hitPoint, hitNormal);
}

static bool IsPlayerInsideNpcVisionCone(
        const TopdownNpcRuntime& npc,
        const TopdownPlayerRuntime& player)
{
    Vector2 toPlayer = TopdownSub(player.position, npc.position);
    const float dist = TopdownLength(toPlayer);

    if (dist <= 0.000001f) {
        return true;
    }

    const Vector2 dirToPlayer = TopdownMul(toPlayer, 1.0f / dist);

    Vector2 facing = TopdownNormalizeOrZero(npc.facing);
    if (TopdownLengthSqr(facing) <= 0.000001f) {
        facing = Vector2{
                std::cos(npc.rotationRadians),
                std::sin(npc.rotationRadians)
        };
        facing = TopdownNormalizeOrZero(facing);
    }

    if (TopdownLengthSqr(facing) <= 0.000001f) {
        facing = Vector2{1.0f, 0.0f};
    }

    const float cosThreshold =
            std::cos(npc.visionHalfAngleDegrees * DEG2RAD);

    return TopdownDot(facing, dirToPlayer) >= cosThreshold;
}

static bool HasWallOcclusionBetweenPoints(
        GameState& state,
        Vector2 from,
        Vector2 to)
{
    Vector2 delta = TopdownSub(to, from);
    const float dist = TopdownLength(delta);

    if (dist <= 0.000001f) {
        return false;
    }

    const Vector2 dir = TopdownMul(delta, 1.0f / dist);

    Vector2 hitPoint{};
    Vector2 hitNormal{};
    float hitDistance = dist;

    if (RaycastClosestSegmentWithNormal(
            from,
            dir,
            state.topdown.runtime.collision.visionSegments,
            dist,
            hitPoint,
            hitNormal,
            hitDistance)) {
        return true;
    }

    hitDistance = dist;
    if (RaycastClosestSegmentWithNormal(
            from,
            dir,
            state.topdown.runtime.collision.boundarySegments,
            dist,
            hitPoint,
            hitNormal,
            hitDistance)) {
        return true;
    }

    return false;
}

bool TopdownNpcCanSeePlayer(
        GameState& state,
        const TopdownNpcRuntime& npc)
{
    if (!TopdownIsPlayerAlive(state)) {
        return false;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const float distSqr = TopdownLengthSqr(TopdownSub(player.position, npc.position));
    const float range = std::max(0.0f, npc.visionRange);

    if (distSqr > range * range) {
        return false;
    }

    if (!IsPlayerInsideNpcVisionCone(npc, player)) {
        return false;
    }

    return HasNpcLineOfSightToPlayer(state, npc, range);
}

bool TopdownNpcCanHearPlayer(
        GameState& state,
        const TopdownNpcRuntime& npc)
{
    if (!TopdownIsPlayerAlive(state)) {
        return false;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    if (HasWallOcclusionBetweenPoints(state, npc.position, player.position)) {
        return false;
    }

    float range = std::max(0.0f, npc.hearingRange);

    /* Reducing hearing through doors is less fun
    if (HasDoorBetweenPoints(state, npc.position, player.position)) {
        range *= 0.75f;
    }
    */

    const float distSqr = TopdownLengthSqr(TopdownSub(player.position, npc.position));
    return distSqr <= range * range;
}

void TopdownUpdateNpcPerception(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dtMs)
{
    const bool seesPlayer = TopdownNpcCanSeePlayer(state, npc);
    const bool hearsPlayer = TopdownNpcCanHearPlayer(state, npc);

    if (seesPlayer || hearsPlayer) {
        TopdownAlertNpcToPlayer(state, npc);

        const float nearbyAlertRadius =
                std::max(180.0f, npc.hearingRange);

        TopdownAlertNearbyNpcs(state, npc, nearbyAlertRadius);
        return;
    }

    if (npc.hasPlayerTarget) {
        npc.loseTargetTimerMs += dtMs;
        npc.awarenessState = TopdownNpcAwarenessState::Suspicious;

        if (npc.loseTargetTimerMs >= npc.loseTargetTimeoutMs) {
            if (TopdownHasNpcReachedLastKnownTarget(npc)) {
                if (!npc.persistentChase &&
                    TopdownBeginNpcInvestigationState(state, npc)) {
                    return;
                }
                TopdownBeginNpcSearchState(npc);
                return;
            }

            const float currentDistance =
                    TopdownLength(
                            TopdownSub(
                                    npc.lastKnownPlayerPosition,
                                    npc.position));

            if (npc.lostTargetProgressTimerMs <= 0.0f) {
                npc.lostTargetLastDistance = currentDistance;
                npc.lostTargetProgressTimerMs = dtMs;
            } else {
                npc.lostTargetProgressTimerMs += dtMs;
            }

            // Probe chase progress every 800 ms to avoid infinite pursuit on bad pathing.
            if (npc.lostTargetProgressTimerMs >= 800.0f) {
                const float progress =
                        npc.lostTargetLastDistance - currentDistance;

                // If we barely closed distance since the last probe, transition into search.
                const bool madeTooLittleProgress = progress < 20.0f;
                if (madeTooLittleProgress) {
                    if (!npc.persistentChase &&
                        TopdownBeginNpcInvestigationState(state, npc)) {
                        return;
                    }
                    TopdownBeginNpcSearchState(npc);
                    return;
                }

                npc.lostTargetLastDistance = currentDistance;
                npc.lostTargetProgressTimerMs = 0.0f;
            }
        } else {
            TopdownResetNpcLostTargetProgress(npc);
        }

        return;
    }

    TopdownResetNpcLostTargetProgress(npc);
    npc.awarenessState = TopdownNpcAwarenessState::Idle;
}

void TopdownUpdateNpcPersistentChaseState(
        GameState& state,
        TopdownNpcRuntime& npc,
        bool currentlyDetectsPlayer)
{
    if (!npc.hasPlayerTarget) {
        return;
    }

    npc.awarenessState = TopdownNpcAwarenessState::Alerted;
    npc.loseTargetTimerMs = 0.0f;
    npc.lastKnownPlayerPosition = state.topdown.runtime.player.position;
    TopdownResetNpcLostTargetProgress(npc);

    if (currentlyDetectsPlayer) {
        TopdownResetNpcChaseStuckWatchdog(npc);
    }
}

bool TopdownIsPlayerWithinNpcAttackRange(
        const TopdownNpcRuntime& npc,
        const TopdownPlayerRuntime& player)
{
    const Vector2 delta = TopdownSub(player.position, npc.position);
    const float centerDist = TopdownLength(delta);
    const float edgeDist = centerDist - player.radius - npc.collisionRadius;
    return edgeDist <= npc.attackRange;
}

bool TopdownNpcHasLineOfSightToNpc(
        GameState& state,
        const TopdownNpcRuntime& fromNpc,
        const TopdownNpcRuntime& toNpc)
{
    const Vector2 delta = TopdownSub(toNpc.position, fromNpc.position);
    const float dist = TopdownLength(delta);

    if (dist <= 0.000001f) {
        return true;
    }

    const Vector2 dir = TopdownMul(delta, 1.0f / dist);

    Vector2 hitPoint{};
    Vector2 hitNormal{};
    float hitDistance = dist;

    if (RaycastClosestSegmentWithNormal(
            fromNpc.position,
            dir,
            state.topdown.runtime.collision.visionSegments,
            dist,
            hitPoint,
            hitNormal,
            hitDistance)) {
        return false;
    }

    hitDistance = dist;
    if (RaycastClosestSegmentWithNormal(
            fromNpc.position,
            dir,
            state.topdown.runtime.collision.boundarySegments,
            dist,
            hitPoint,
            hitNormal,
            hitDistance)) {
        return false;
    }

    TopdownRuntimeDoor* hitDoor = nullptr;
    hitDistance = dist;
    if (RaycastClosestDoor(
            state,
            fromNpc.position,
            dir,
            dist,
            hitDoor,
            hitPoint,
            hitNormal,
            hitDistance)) {
        return false;
    }

    return true;
}

float TopdownGetNpcClipDurationMs(
        const GameState& state,
        const TopdownNpcClipRef& clipRef)
{
    if (!TopdownNpcClipRefIsValid(clipRef)) {
        return 0.0f;
    }

    const SpriteAssetResource* sprite =
            FindSpriteAssetResource(state.resources, clipRef.spriteHandle);

    if (sprite == nullptr || !sprite->loaded) {
        return 0.0f;
    }

    if (clipRef.clipIndex < 0 || clipRef.clipIndex >= static_cast<int>(sprite->clips.size())) {
        return 0.0f;
    }

    return GetOneShotClipDurationMs(*sprite, sprite->clips[clipRef.clipIndex]);
}

void TopdownBuildNpcPathToTarget(
        GameState& state,
        TopdownNpcRuntime& npc,
        Vector2 targetPos,
        TopdownNpcMoveOwner owner)
{
    npc.repathTimerMs = 0.0f;

    std::vector<Vector2> pathPoints;
    std::vector<int> trianglePath;
    Vector2 resolvedEndPos = targetPos;

    bool builtPath = false;

    if (state.topdown.runtime.nav.valid) {
        builtPath = BuildNavPath(
                state.topdown.runtime.nav.navMesh,
                npc.position,
                targetPos,
                pathPoints,
                &trianglePath,
                &resolvedEndPos);
    }

    if (!builtPath) {
        if (owner == TopdownNpcMoveOwner::Script) {
            // Scripted move targets may intentionally request direct movement when nav pathing fails.
            pathPoints.clear();
            pathPoints.push_back(targetPos);
            resolvedEndPos = targetPos;
        } else {
            // AI keeps the existing path if repath fails to avoid snapping to a full stop.
            return;
        }
    }

    // Preserve current speed across repaths so chase movement does not jitter stop/start.
    const float preservedSpeed = npc.move.currentSpeed;

    npc.move = {};
    npc.move.active = !pathPoints.empty();
    npc.move.owner = owner;
    // AI/script path requests default to running for responsive melee pursuit.
    npc.move.running = true;
    npc.move.pathPoints = pathPoints;
    npc.move.currentPoint = 0;
    npc.move.finalTarget = resolvedEndPos;
    npc.move.hasFinalTarget = true;
    npc.move.currentSpeed = preservedSpeed;
    npc.move.acceleration = 1800.0f;
    npc.move.deceleration = 2200.0f;
    npc.move.arrivalRadius = 12.0f;
    npc.move.stopDistance = (owner == TopdownNpcMoveOwner::Script) ? 100.0f : 12.0f;
    npc.move.debugTrianglePath = trianglePath;

    npc.moving = npc.move.active;
    npc.running = npc.move.active;
}

void TopdownApplyDamageToPlayer(
        GameState& state,
        float damage,
        Vector2 attackerPos)
{
    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCharacterAssetData& playerAsset = state.topdown.playerCharacterAsset;

    if (state.topdown.runtime.godMode) {
        return;
    }

    if (!TopdownIsPlayerAlive(state)) {
        return;
    }

    if (player.hurtCooldownRemainingMs > 0.0f) {
        return;
    }

    if (damage <= 0.0f) {
        return;
    }

    player.health -= damage;
    if (player.health < 0.0f) {
        player.health = 0.0f;
    }

    player.hurtCooldownRemainingMs = std::max(0.0f, playerAsset.hurtCooldownMs);
    player.hitSlowdownRemainingMs = std::max(player.hitSlowdownRemainingMs, playerAsset.meleeHitSlowdownMs);
    player.hitSlowdownMultiplier = std::min(player.hitSlowdownMultiplier, playerAsset.meleeHitSlowdownMultiplier);
    player.damageFlashRemainingMs = std::max(player.damageFlashRemainingMs, 260.0f);

    if (player.health <= 0.0f) {
        player.lifeState = TopdownPlayerLifeState::Dead;
        state.topdown.runtime.controlsEnabled = false;
        player.velocity = Vector2{};
        player.desiredVelocity = Vector2{};
        player.moveInputForward = 0.0f;
        player.moveInputRight = 0.0f;
        player.wantsRun = false;
        state.topdown.runtime.playerAttack.triggerHeld = false;
        state.topdown.runtime.playerAttack.pendingPrimaryAttack = false;
        state.topdown.runtime.playerAttack.pendingSecondaryAttack = false;
    }

    (void)attackerPos;
}
