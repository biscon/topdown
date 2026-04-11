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

    const bool newlyAcquiredTarget = !npc.hasPlayerTarget;

    npc.hasPlayerTarget = true;
    npc.lastKnownPlayerPosition = state.topdown.runtime.player.position;
    npc.loseTargetTimerMs = 0.0f;
    npc.awarenessState = TopdownNpcAwarenessState::Alerted;

    if (newlyAcquiredTarget) {
        npc.repathTimerMs = 0.0f;
    }

    if (npc.combatState != TopdownNpcCombatState::Attack) {
        npc.combatState = TopdownNpcCombatState::Chase;
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
            pathPoints.clear();
            pathPoints.push_back(targetPos);
            resolvedEndPos = targetPos;
        } else {
            // Keep existing AI move if we already had one.
            return;
        }
    }

    const float preservedSpeed = npc.move.currentSpeed;

    npc.move = {};
    npc.move.active = !pathPoints.empty();
    npc.move.owner = owner;
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
