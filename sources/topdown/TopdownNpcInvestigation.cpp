#include "topdown/TopdownNpcInvestigation.h"

#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcRingSlots.h"
#include "TopdownNpcAiCommon.h"
#include "nav/NavMeshQuery.h"

namespace {
    constexpr float kInvestigationJoinRadius = 600.0f;
    constexpr float kInvestigationArriveRadius = 44.0f;
    constexpr float kInvestigationProbePeriodMs = 800.0f;
    constexpr float kInvestigationMinProgressPerProbe = 10.0f;

    TopdownNpcInvestigationContext* FindInvestigationContextByHandle(
            TopdownRuntimeData& runtime,
            int handle)
    {
        for (TopdownNpcInvestigationContext& context : runtime.npcInvestigations) {
            if (context.active && context.handle == handle) {
                return &context;
            }
        }

        return nullptr;
    }

    TopdownNpcInvestigationContext* FindNearbyInvestigationContext(
            TopdownRuntimeData& runtime,
            Vector2 origin)
    {
        const float joinRadiusSqr = kInvestigationJoinRadius * kInvestigationJoinRadius;

        TopdownNpcInvestigationContext* bestContext = nullptr;
        float bestDistSqr = 0.0f;

        for (TopdownNpcInvestigationContext& context : runtime.npcInvestigations) {
            if (!context.active) {
                continue;
            }

            const float distSqr = TopdownLengthSqr(TopdownSub(context.origin, origin));
            if (distSqr > joinRadiusSqr) {
                continue;
            }

            if (bestContext == nullptr || distSqr < bestDistSqr) {
                bestContext = &context;
                bestDistSqr = distSqr;
            }
        }

        return bestContext;
    }

    TopdownNpcInvestigationContext* CreateInvestigationContext(
            TopdownRuntimeData& runtime,
            Vector2 origin,
            float npcRadius,
            int ownerNpcHandle)
    {
        TopdownNpcRingSlotBuildConfig ringConfig;
        ringConfig.candidatePadding = 8.0f;
        ringConfig.maxRings = 3;
        ringConfig.minRadiusStep = 16.0f;
        ringConfig.includeOriginCandidate = false;

        std::vector<Vector2> slotPositions;
        TopdownCollectValidNpcRingSlots(
                runtime,
                origin,
                npcRadius,
                ringConfig,
                slotPositions,
                ownerNpcHandle);

        if (slotPositions.empty()) {
            return nullptr;
        }

        TopdownNpcInvestigationContext newContext;
        newContext.active = true;
        newContext.handle = runtime.nextNpcInvestigationContextHandle++;
        newContext.origin = origin;
        newContext.slots.reserve(slotPositions.size());

        for (const Vector2& slotPos : slotPositions) {
            TopdownNpcInvestigationSlot slot;
            slot.position = slotPos;
            newContext.slots.push_back(slot);
        }

        runtime.npcInvestigations.push_back(newContext);
        return &runtime.npcInvestigations.back();
    }

    void ReleaseNpcInvestigationSlot(
            TopdownRuntimeData& runtime,
            TopdownNpcRuntime& npc)
    {
        TopdownNpcInvestigationContext* context =
                FindInvestigationContextByHandle(runtime, npc.investigationContextHandle);

        if (context != nullptr &&
            npc.investigationSlotIndex >= 0 &&
            npc.investigationSlotIndex < static_cast<int>(context->slots.size())) {
            TopdownNpcInvestigationSlot& slot = context->slots[npc.investigationSlotIndex];
            if (slot.claimedByNpcHandle == npc.handle) {
                slot.claimedByNpcHandle = -1;
            }
        }

        TopdownResetNpcInvestigationState(npc);
    }

    int FindNearestUnclaimedSlotIndex(
            const TopdownNpcInvestigationContext& context,
            const TopdownNpcRuntime& npc)
    {
        int bestSlotIndex = -1;
        float bestDistSqr = 0.0f;

        for (int i = 0; i < static_cast<int>(context.slots.size()); ++i) {
            const TopdownNpcInvestigationSlot& slot = context.slots[i];

            if (slot.claimedByNpcHandle >= 0) {
                continue;
            }

            const float distSqr =
                    TopdownLengthSqr(TopdownSub(slot.position, npc.position));

            if (bestSlotIndex < 0 || distSqr < bestDistSqr) {
                bestSlotIndex = i;
                bestDistSqr = distSqr;
            }
        }

        return bestSlotIndex;
    }

    static float ComputePathLength(const std::vector<Vector2>& pathPoints, Vector2 startPos)
    {
        float total = 0.0f;
        Vector2 prev = startPos;

        for (const Vector2& pt : pathPoints) {
            total += TopdownLength(TopdownSub(pt, prev));
            prev = pt;
        }

        return total;
    }

    static int FindBestReachableSlotIndex(
            GameState& state,
            const TopdownNpcInvestigationContext& context,
            const TopdownNpcRuntime& npc)
    {
        int bestSlotIndex = -1;
        float bestPathLength = 0.0f;

        for (int i = 0; i < static_cast<int>(context.slots.size()); ++i) {
            const TopdownNpcInvestigationSlot& slot = context.slots[i];

            if (slot.claimedByNpcHandle >= 0) {
                continue;
            }

            std::vector<Vector2> pathPoints;
            std::vector<int> trianglePath;
            Vector2 resolvedEndPos = slot.position;

            bool builtPath = false;
            if (state.topdown.runtime.nav.valid) {
                builtPath = BuildNavPath(
                        state.topdown.runtime.nav.navMesh,
                        npc.position,
                        slot.position,
                        pathPoints,
                        &trianglePath,
                        &resolvedEndPos);
            }

            if (!builtPath) {
                continue;
            }

            const float pathLength = ComputePathLength(pathPoints, npc.position);

            if (bestSlotIndex < 0 || pathLength < bestPathLength) {
                bestSlotIndex = i;
                bestPathLength = pathLength;
            }
        }

        return bestSlotIndex;
    }

    static int FindRandomReachableSlotIndex(
            GameState& state,
            const TopdownNpcInvestigationContext& context,
            const TopdownNpcRuntime& npc)
    {
        std::vector<int> reachableSlotIndices;
        reachableSlotIndices.reserve(context.slots.size());

        for (int i = 0; i < static_cast<int>(context.slots.size()); ++i) {
            const TopdownNpcInvestigationSlot& slot = context.slots[i];

            if (slot.claimedByNpcHandle >= 0) {
                continue;
            }

            std::vector<Vector2> pathPoints;
            std::vector<int> trianglePath;
            Vector2 resolvedEndPos = slot.position;

            bool builtPath = false;
            if (state.topdown.runtime.nav.valid) {
                builtPath = BuildNavPath(
                        state.topdown.runtime.nav.navMesh,
                        npc.position,
                        slot.position,
                        pathPoints,
                        &trianglePath,
                        &resolvedEndPos);
            }

            if (!builtPath) {
                continue;
            }

            reachableSlotIndices.push_back(i);
        }

        if (reachableSlotIndices.empty()) {
            return -1;
        }

        const int randomIndex =
                GetRandomValue(0, static_cast<int>(reachableSlotIndices.size()) - 1);

        return reachableSlotIndices[randomIndex];
    }

    bool TryClaimNpcInvestigationSlot(
            GameState& state,
            TopdownRuntimeData& runtime,
            TopdownNpcRuntime& npc)
    {
        TopdownNpcInvestigationContext* context =
                FindInvestigationContextByHandle(runtime, npc.investigationContextHandle);

        if (context == nullptr) {
            return false;
        }

        if (npc.investigationSlotIndex >= 0 &&
            npc.investigationSlotIndex < static_cast<int>(context->slots.size())) {
            TopdownNpcInvestigationSlot& slot = context->slots[npc.investigationSlotIndex];
            if (slot.claimedByNpcHandle == npc.handle) {
                return true;
            }
        }

        const int slotIndex = FindRandomReachableSlotIndex(state, *context, npc);
        //const int slotIndex = FindBestReachableSlotIndex(state, *context, npc);
        //const int slotIndex = FindNearestUnclaimedSlotIndex(*context, npc);
        if (slotIndex < 0) {
            return false;
        }

        context->slots[slotIndex].claimedByNpcHandle = npc.handle;
        npc.investigationSlotIndex = slotIndex;
        return true;
    }

    bool NpcIsParticipatingInContext(
            const TopdownRuntimeData& runtime,
            int contextHandle)
    {
        for (const TopdownNpcRuntime& npc : runtime.npcs) {
            if (!npc.active || npc.dead || npc.corpse) {
                continue;
            }

            if (npc.combatState != TopdownNpcCombatState::Investigation) {
                continue;
            }

            if (npc.investigationContextHandle == contextHandle) {
                return true;
            }
        }

        return false;
    }

    void CleanupInactiveInvestigationClaims(TopdownRuntimeData& runtime)
    {
        for (TopdownNpcInvestigationContext& context : runtime.npcInvestigations) {
            if (!context.active) {
                continue;
            }

            for (int slotIndex = 0; slotIndex < static_cast<int>(context.slots.size()); ++slotIndex) {
                TopdownNpcInvestigationSlot& slot = context.slots[slotIndex];
                if (slot.claimedByNpcHandle < 0) {
                    continue;
                }

                bool stillValidClaim = false;
                for (const TopdownNpcRuntime& npc : runtime.npcs) {
                    if (!npc.active || npc.dead || npc.corpse) {
                        continue;
                    }

                    if (npc.handle != slot.claimedByNpcHandle) {
                        continue;
                    }

                    if (npc.combatState != TopdownNpcCombatState::Investigation) {
                        break;
                    }

                    if (npc.investigationContextHandle != context.handle) {
                        break;
                    }

                    if (npc.investigationSlotIndex != slotIndex) {
                        break;
                    }

                    stillValidClaim = true;
                    break;
                }

                if (!stillValidClaim) {
                    slot.claimedByNpcHandle = -1;
                }
            }
        }
    }

    static bool UpdateNpcInvestigationStuckWatchdog(
            TopdownNpcRuntime& npc,
            float dtMs,
            float probePeriodMs,
            float minDistancePerProbe)
    {
        if (npc.investigationProgressTimerMs <= 0.0f) {
            npc.investigationLastPosition = npc.position;
            npc.investigationProgressTimerMs = dtMs;
            return false;
        }

        npc.investigationProgressTimerMs += dtMs;

        if (npc.investigationProgressTimerMs < probePeriodMs) {
            return false;
        }

        const float movedDistance =
                TopdownLength(
                        TopdownSub(
                                npc.position,
                                npc.investigationLastPosition));

        const bool movedTooLittle = movedDistance < minDistancePerProbe;

        npc.investigationLastPosition = npc.position;
        npc.investigationProgressTimerMs = 0.0f;

        return movedTooLittle;
    }

} // namespace

void TopdownResetNpcInvestigationState(TopdownNpcRuntime& npc)
{
    npc.investigationContextHandle = -1;
    npc.investigationSlotIndex = -1;
    npc.investigationProgressTimerMs = 0.0f;
    npc.investigationLastPosition = Vector2{};
}

bool TopdownBeginNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;

    TopdownLeaveNpcInvestigationState(state, npc);

    TopdownNpcInvestigationContext* context =
            FindNearbyInvestigationContext(runtime, npc.investigationPosition);

    if (context == nullptr) {
        TraceLog(LOG_INFO, "Creating new investigation context");
        context = CreateInvestigationContext(
                runtime,
                npc.investigationPosition,
                npc.collisionRadius,
                npc.handle);
    } else {
        TraceLog(LOG_INFO, "Reusing existing investigation context");
    }

    if (context == nullptr) {
        return false;
    }

    npc.investigationContextHandle = context->handle;
    if (!TryClaimNpcInvestigationSlot(state, runtime, npc)) {
        TopdownResetNpcInvestigationState(npc);
        return false;
    }

    npc.combatState = TopdownNpcCombatState::Investigation;
    npc.investigationProgressTimerMs = 0.0f;
    npc.investigationLastPosition = npc.position;

    TopdownNpcInvestigationSlot& slot = context->slots[npc.investigationSlotIndex];

    TopdownStopNpcMovement(npc);

    TopdownBuildNpcPathToTarget(
            state,
            npc,
            slot.position,
            TopdownNpcMoveOwner::Ai);

    const bool validPath =
            npc.move.active &&
            npc.move.hasFinalTarget &&
            TopdownLengthSqr(TopdownSub(npc.move.finalTarget, slot.position)) <= 1.0f;

    if (!validPath) {
        TraceLog(LOG_ERROR, "Investigation computed invalid path, abort.");
        TopdownLeaveNpcInvestigationState(state, npc);
        return false;
    }

    if (!npc.move.active || !npc.move.hasFinalTarget) {
        TopdownLeaveNpcInvestigationState(state, npc);
        return false;
    }

    return true;
}

void TopdownLeaveNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    ReleaseNpcInvestigationSlot(state.topdown.runtime, npc);
    TopdownPruneNpcInvestigationContexts(state);
}

TopdownNpcInvestigationUpdateResult TopdownUpdateNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (npc.combatState != TopdownNpcCombatState::Investigation) {
        return TopdownNpcInvestigationUpdateResult::Failed;
    }

    TopdownRuntimeData& runtime = state.topdown.runtime;
    TopdownNpcInvestigationContext* context =
            FindInvestigationContextByHandle(runtime, npc.investigationContextHandle);

    if (context == nullptr ||
        !TryClaimNpcInvestigationSlot(state, runtime, npc) ||
        npc.investigationSlotIndex < 0 ||
        npc.investigationSlotIndex >= static_cast<int>(context->slots.size())) {
        TopdownLeaveNpcInvestigationState(state, npc);
        return TopdownNpcInvestigationUpdateResult::Failed;
    }

    TopdownNpcInvestigationSlot& slot = context->slots[npc.investigationSlotIndex];

    if (TopdownHasNpcReachedPoint(npc, slot.position, kInvestigationArriveRadius)) {
        TopdownLeaveNpcInvestigationState(state, npc);
        return TopdownNpcInvestigationUpdateResult::Arrived;
    }

    if (!npc.move.active || !npc.move.hasFinalTarget) {
        TopdownBuildNpcPathToTarget(
                state,
                npc,
                slot.position,
                TopdownNpcMoveOwner::Ai);

        if (!npc.move.active || !npc.move.hasFinalTarget) {
            TopdownLeaveNpcInvestigationState(state, npc);
            return TopdownNpcInvestigationUpdateResult::Failed;
        }
    }

    const float dtMs = dt * 1000.0f;

    if (UpdateNpcInvestigationStuckWatchdog(
            npc,
            dtMs,
            kInvestigationProbePeriodMs,
            kInvestigationMinProgressPerProbe)) {
        TopdownLeaveNpcInvestigationState(state, npc);
        return TopdownNpcInvestigationUpdateResult::Failed;
    }

    return TopdownNpcInvestigationUpdateResult::Running;
}

void TopdownPruneNpcInvestigationContexts(GameState& state)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;

    CleanupInactiveInvestigationClaims(runtime);

    for (TopdownNpcInvestigationContext& context : runtime.npcInvestigations) {
        if (!context.active) {
            continue;
        }

        if (!NpcIsParticipatingInContext(runtime, context.handle)) {
            context.active = false;
            context.slots.clear();
        }
    }
}
