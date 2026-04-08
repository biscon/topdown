#include "topdown/TopdownNpcAi.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "topdown/TopdownHelpers.h"
#include "topdown/NpcRegistry.h"
#include "topdown/LevelCollision.h"
#include "nav/NavMeshQuery.h"
#include "resources/AsepriteAsset.h"

static constexpr float NPC_INVESTIGATION_CONTEXT_MERGE_DISTANCE = 120.0f;
static constexpr float NPC_INVESTIGATION_CONTEXT_EXPIRE_MS = 10000.0f;
static constexpr float NPC_INVESTIGATION_REFRESH_DISTANCE = 20.0f;
static constexpr float NPC_INVESTIGATION_DEFAULT_RADIUS = 115.0f;
static constexpr float NPC_INVESTIGATION_REASSIGN_COOLDOWN_MS = 400.0f;

static Vector2 BuildInvestigationCandidateOffset(int candidateIndex, float radius)
{
    if (candidateIndex <= 0) {
        return {};
    }

    static constexpr float kAnglesDegrees[] = {
            0.0f, 180.0f, 90.0f, -90.0f, 45.0f, -45.0f, 135.0f, -135.0f
    };

    const int ringIndex = (candidateIndex - 1) / 8;
    const int angleIndex = (candidateIndex - 1) % 8;
    const float ringRadius = radius + static_cast<float>(ringIndex) * (radius * 0.75f);
    const float radians = kAnglesDegrees[angleIndex] * DEG2RAD;

    return Vector2{
            std::cos(radians) * ringRadius,
            std::sin(radians) * ringRadius
    };
}

static TopdownInvestigationContextRuntime* FindInvestigationContextByHandle(
        TopdownRuntimeData& runtime,
        int handle)
{
    if (handle <= 0) {
        return nullptr;
    }

    for (TopdownInvestigationContextRuntime& ctx : runtime.investigationContexts) {
        if (ctx.active && ctx.handle == handle) {
            return &ctx;
        }
    }

    return nullptr;
}

static const TopdownInvestigationContextRuntime* FindInvestigationContextByHandle(
        const TopdownRuntimeData& runtime,
        int handle)
{
    if (handle <= 0) {
        return nullptr;
    }

    for (const TopdownInvestigationContextRuntime& ctx : runtime.investigationContexts) {
        if (ctx.active && ctx.handle == handle) {
            return &ctx;
        }
    }

    return nullptr;
}

static bool TryProjectInvestigationCandidateToNav(
        const TopdownRuntimeData& runtime,
        Vector2 requestedPos,
        Vector2& outProjected)
{
    outProjected = requestedPos;

    if (!runtime.nav.valid) {
        return true;
    }

    int triIndex = -1;
    outProjected = ProjectPointToNavMesh(runtime.nav.navMesh, requestedPos, &triIndex);
    return triIndex >= 0;
}

static bool HasInvestigationSlotWallClearance(
        const TopdownRuntimeData& runtime,
        Vector2 candidate,
        float minClearance)
{
    const float minClearanceSqr = minClearance * minClearance;

    for (const TopdownSegment& seg : runtime.collision.boundarySegments) {
        const Vector2 closest = TopdownClosestPointOnSegment(candidate, seg);
        const float distSqr = TopdownLengthSqr(TopdownSub(candidate, closest));
        if (distSqr < minClearanceSqr) {
            return false;
        }
    }

    for (const TopdownSegment& seg : runtime.collision.visionSegments) {
        const Vector2 closest = TopdownClosestPointOnSegment(candidate, seg);
        const float distSqr = TopdownLengthSqr(TopdownSub(candidate, closest));
        if (distSqr < minClearanceSqr) {
            return false;
        }
    }

    return true;
}

static void GenerateInvestigationSlots(
        TopdownRuntimeData& runtime,
        TopdownInvestigationContextRuntime& ctx,
        int desiredCount)
{
    if (desiredCount < 1) {
        desiredCount = 1;
    }

    std::vector<TopdownInvestigationSlotRuntime> slots;
    slots.reserve(static_cast<size_t>(desiredCount));

    const float slotClearance = 22.0f;
    const float minSlotSpacing = 26.0f;
    const float minSlotSpacingSqr = minSlotSpacing * minSlotSpacing;

    int candidateIndex = 0;
    int attempts = 0;
    const int maxAttempts = desiredCount * 12;

    while (static_cast<int>(slots.size()) < desiredCount && attempts < maxAttempts) {
        Vector2 candidatePos = TopdownAdd(
                ctx.anchor,
                BuildInvestigationCandidateOffset(candidateIndex, ctx.radius));

        candidateIndex++;
        attempts++;

        if (!HasInvestigationSlotWallClearance(runtime, candidatePos, slotClearance)) {
            continue;
        }

        Vector2 projected = candidatePos;
        if (!TryProjectInvestigationCandidateToNav(runtime, candidatePos, projected)) {
            continue;
        }

        bool tooCloseToExisting = false;
        for (const TopdownInvestigationSlotRuntime& existing : slots) {
            if (!existing.valid) {
                continue;
            }

            const float distSqr = TopdownLengthSqr(TopdownSub(existing.position, projected));
            if (distSqr < minSlotSpacingSqr) {
                tooCloseToExisting = true;
                break;
            }
        }

        if (tooCloseToExisting) {
            continue;
        }

        TopdownInvestigationSlotRuntime slot;
        slot.position = projected;
        slot.valid = true;
        slot.reservedByNpcHandle = -1;
        slots.push_back(slot);
    }

    if (slots.empty()) {
        TopdownInvestigationSlotRuntime fallbackSlot;
        fallbackSlot.position = ctx.anchor;
        fallbackSlot.valid = true;
        fallbackSlot.reservedByNpcHandle = -1;
        slots.push_back(fallbackSlot);
    }

    ctx.slots = slots;
}

static int FindOrCreateInvestigationContext(
        TopdownRuntimeData& runtime,
        TopdownNpcRuntime& npc,
        Vector2 anchor)
{
    for (TopdownInvestigationContextRuntime& ctx : runtime.investigationContexts) {
        if (!ctx.active) {
            continue;
        }

        if (ctx.aiMode != npc.aiMode || ctx.hostile != npc.hostile) {
            continue;
        }

        const float distSqr = TopdownLengthSqr(TopdownSub(ctx.anchor, anchor));
        if (distSqr > NPC_INVESTIGATION_CONTEXT_MERGE_DISTANCE *
                       NPC_INVESTIGATION_CONTEXT_MERGE_DISTANCE) {
            continue;
        }

        const float anchorMoveSqr = distSqr;
        if (anchorMoveSqr >
            NPC_INVESTIGATION_REFRESH_DISTANCE * NPC_INVESTIGATION_REFRESH_DISTANCE) {
            ctx.anchor = anchor;
            ctx.radius = NPC_INVESTIGATION_DEFAULT_RADIUS;
            GenerateInvestigationSlots(runtime, ctx, static_cast<int>(ctx.slots.size()));
        }

        ctx.elapsedMs = 0.0f;
        ctx.lastRefreshMs = 0.0f;
        return ctx.handle;
    }

    TopdownInvestigationContextRuntime ctx;
    ctx.handle = runtime.nextInvestigationContextHandle++;
    ctx.active = true;
    ctx.aiMode = npc.aiMode;
    ctx.hostile = npc.hostile;
    ctx.anchor = anchor;
    ctx.elapsedMs = 0.0f;
    ctx.lastRefreshMs = 0.0f;
    ctx.radius = NPC_INVESTIGATION_DEFAULT_RADIUS;
    GenerateInvestigationSlots(runtime, ctx, 8);

    runtime.investigationContexts.push_back(ctx);
    return ctx.handle;
}

static bool IsInvestigationSlotReachable(
        const TopdownRuntimeData& runtime,
        const TopdownNpcRuntime& npc,
        Vector2 slotPos)
{
    if (!runtime.nav.valid) {
        return true;
    }

    std::vector<Vector2> path;
    std::vector<int> triangles;
    Vector2 resolvedEnd = slotPos;
    return BuildNavPath(
            runtime.nav.navMesh,
            npc.position,
            slotPos,
            path,
            &triangles,
            &resolvedEnd);
}

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
        default:                             return "Unknown";
    }
}

void TopdownStopNpcMovement(TopdownNpcRuntime& npc)
{
    npc.move = {};
    npc.localAvoidanceVelocity = {};
    npc.moving = false;
    npc.running = false;
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

    npc.hasPlayerTarget = true;
    npc.lastKnownPlayerPosition = state.topdown.runtime.player.position;
    npc.loseTargetTimerMs = 0.0f;
    npc.repathTimerMs = 0.0f;
    npc.investigationReassignCooldownMs = 0.0f;
    npc.lastProgressPosition = npc.position;
    npc.blockedProgressTimerMs = 0.0f;
    npc.awarenessState = TopdownNpcAwarenessState::Alerted;

    npc.investigationContextHandle = FindOrCreateInvestigationContext(
            state.topdown.runtime,
            npc,
            npc.lastKnownPlayerPosition);
    npc.investigationSlotIndex = -1;
    TopdownAssignNpcInvestigationSlot(state, npc);

    if (npc.combatState != TopdownNpcCombatState::Attack) {
        npc.combatState = TopdownNpcCombatState::Chase;
    }
}

void TopdownClearNpcInvestigationSlot(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;
    TopdownInvestigationContextRuntime* ctx =
            FindInvestigationContextByHandle(runtime, npc.investigationContextHandle);

    if (ctx != nullptr &&
        npc.investigationSlotIndex >= 0 &&
        npc.investigationSlotIndex < static_cast<int>(ctx->slots.size())) {
        TopdownInvestigationSlotRuntime& slot = ctx->slots[npc.investigationSlotIndex];
        if (slot.reservedByNpcHandle == npc.handle) {
            slot.reservedByNpcHandle = -1;
        }
    }

    npc.investigationContextHandle = -1;
    npc.investigationSlotIndex = -1;
    npc.investigationReassignCooldownMs = 0.0f;
    npc.lastProgressPosition = npc.position;
    npc.blockedProgressTimerMs = 0.0f;
}

void TopdownAssignNpcInvestigationSlot(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;
    TopdownInvestigationContextRuntime* ctx =
            FindInvestigationContextByHandle(runtime, npc.investigationContextHandle);

    if (ctx == nullptr || !ctx->active || ctx->slots.empty()) {
        npc.investigationSlotIndex = -1;
        return;
    }

    if (npc.investigationSlotIndex >= 0 &&
        npc.investigationSlotIndex < static_cast<int>(ctx->slots.size())) {
        TopdownInvestigationSlotRuntime& currentSlot = ctx->slots[npc.investigationSlotIndex];
        if (currentSlot.valid &&
            currentSlot.reservedByNpcHandle == npc.handle &&
            IsInvestigationSlotReachable(runtime, npc, currentSlot.position)) {
            return;
        }

        if (currentSlot.reservedByNpcHandle == npc.handle) {
            currentSlot.reservedByNpcHandle = -1;
        }
        npc.investigationSlotIndex = -1;
    }

    float bestScore = 0.0f;
    int bestSlotIndex = -1;

    for (int i = 0; i < static_cast<int>(ctx->slots.size()); ++i) {
        TopdownInvestigationSlotRuntime& slot = ctx->slots[i];

        if (!slot.valid) {
            continue;
        }

        if (slot.reservedByNpcHandle != -1 &&
            slot.reservedByNpcHandle != npc.handle) {
            continue;
        }

        if (!IsInvestigationSlotReachable(runtime, npc, slot.position)) {
            continue;
        }

        const float distToAnchor =
                TopdownLengthSqr(TopdownSub(slot.position, ctx->anchor));
        const float distToNpc =
                TopdownLengthSqr(TopdownSub(slot.position, npc.position));
        float congestionPenalty = 0.0f;

        for (const TopdownNpcRuntime& otherNpc : runtime.npcs) {
            if (!otherNpc.active || otherNpc.dead || otherNpc.corpse) {
                continue;
            }

            if (otherNpc.handle == npc.handle) {
                continue;
            }

            if (!otherNpc.hasPlayerTarget) {
                continue;
            }

            const float distToOther =
                    TopdownLengthSqr(TopdownSub(slot.position, otherNpc.position));
            if (distToOther < 80.0f * 80.0f) {
                congestionPenalty += (80.0f * 80.0f - distToOther) * 0.45f;
            }
        }

        // Favor central, close, reachable slots with stable assignments.
        float score = 0.0f;
        score -= distToAnchor * 0.35f;
        score -= distToNpc * 0.65f;
        score -= congestionPenalty;

        if (slot.reservedByNpcHandle == npc.handle) {
            score += 50000.0f;
        }

        if (bestSlotIndex < 0 || score > bestScore) {
            bestScore = score;
            bestSlotIndex = i;
        }
    }

    if (bestSlotIndex < 0) {
        npc.investigationSlotIndex = -1;
        return;
    }

    TopdownInvestigationSlotRuntime& slot = ctx->slots[bestSlotIndex];
    slot.reservedByNpcHandle = npc.handle;
    npc.investigationSlotIndex = bestSlotIndex;
}

bool TopdownGetNpcInvestigationDestination(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        Vector2& outDestination)
{
    const TopdownInvestigationContextRuntime* ctx =
            FindInvestigationContextByHandle(
                    state.topdown.runtime,
                    npc.investigationContextHandle);

    if (ctx == nullptr || !ctx->active) {
        return false;
    }

    if (npc.investigationSlotIndex < 0 ||
        npc.investigationSlotIndex >= static_cast<int>(ctx->slots.size())) {
        return false;
    }

    const TopdownInvestigationSlotRuntime& slot = ctx->slots[npc.investigationSlotIndex];
    if (!slot.valid || slot.reservedByNpcHandle != npc.handle) {
        return false;
    }

    outDestination = slot.position;
    return true;
}

void TopdownUpdateInvestigationContexts(GameState& state, float dtMs)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;

    for (TopdownNpcRuntime& npc : runtime.npcs) {
        if (!npc.active || npc.dead || npc.corpse || !npc.hasPlayerTarget) {
            TopdownClearNpcInvestigationSlot(state, npc);
            continue;
        }

        if (npc.investigationReassignCooldownMs > 0.0f) {
            npc.investigationReassignCooldownMs -= dtMs;
            if (npc.investigationReassignCooldownMs < 0.0f) {
                npc.investigationReassignCooldownMs = 0.0f;
            }
        }

        if (npc.investigationContextHandle <= 0) {
            npc.investigationContextHandle = FindOrCreateInvestigationContext(
                    runtime,
                    npc,
                    npc.lastKnownPlayerPosition);
            npc.investigationSlotIndex = -1;
        }

        if (npc.investigationReassignCooldownMs <= 0.0f) {
            TopdownAssignNpcInvestigationSlot(state, npc);
            npc.investigationReassignCooldownMs = NPC_INVESTIGATION_REASSIGN_COOLDOWN_MS;
        }
    }

    for (TopdownInvestigationContextRuntime& ctx : runtime.investigationContexts) {
        if (!ctx.active) {
            continue;
        }

        ctx.elapsedMs += dtMs;
        ctx.lastRefreshMs += dtMs;

        for (TopdownInvestigationSlotRuntime& slot : ctx.slots) {
            if (!slot.valid) {
                continue;
            }

            if (slot.reservedByNpcHandle == -1) {
                continue;
            }

            bool ownerStillValid = false;
            for (const TopdownNpcRuntime& npc : runtime.npcs) {
                if (!npc.active || npc.dead || npc.corpse) {
                    continue;
                }

                if (npc.handle == slot.reservedByNpcHandle &&
                    npc.investigationContextHandle == ctx.handle &&
                    npc.hasPlayerTarget) {
                    ownerStillValid = true;
                    break;
                }
            }

            if (!ownerStillValid) {
                slot.reservedByNpcHandle = -1;
            }
        }
    }

    runtime.investigationContexts.erase(
            std::remove_if(
                    runtime.investigationContexts.begin(),
                    runtime.investigationContexts.end(),
                    [&](const TopdownInvestigationContextRuntime& ctx) {
                        if (!ctx.active) {
                            return true;
                        }

                        if (ctx.elapsedMs < NPC_INVESTIGATION_CONTEXT_EXPIRE_MS) {
                            return false;
                        }

                        for (const TopdownNpcRuntime& npc : runtime.npcs) {
                            if (!npc.active || npc.dead || npc.corpse) {
                                continue;
                            }
                            if (npc.investigationContextHandle == ctx.handle &&
                                npc.hasPlayerTarget) {
                                return false;
                            }
                        }

                        return true;
                    }),
            runtime.investigationContexts.end());
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
    const float distSqr = TopdownLengthSqr(TopdownSub(player.position, npc.position));
    const float range = std::max(0.0f, npc.hearingRange);

    if (distSqr > range * range) {
        return false;
    }

    return HasNpcLineOfSightToPlayer(state, npc, range);
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
        Vector2 targetPos)
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
        pathPoints.clear();
        pathPoints.push_back(targetPos);
    }

    const float preservedSpeed = npc.move.currentSpeed;

    npc.move = {};
    npc.move.active = !pathPoints.empty();
    npc.move.running = true;
    npc.move.pathPoints = pathPoints;
    npc.move.currentPoint = 0;
    npc.move.currentSpeed = preservedSpeed;
    npc.move.acceleration = 1800.0f;
    npc.move.deceleration = 2200.0f;
    npc.move.arrivalRadius = 12.0f;
    npc.move.stopDistance = 100.0f;

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
