#include "topdown/TopdownNpcInvestigation.h"

#include <algorithm>

#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcAi.h"
#include "topdown/TopdownNpcRingSlots.h"

namespace {
constexpr float kInvestigationJoinRadius = 180.0f;
constexpr float kInvestigationArriveRadius = 44.0f;
constexpr float kInvestigationProbePeriodMs = 800.0f;
constexpr float kInvestigationMinProgressPerProbe = 20.0f;

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

bool TryClaimNpcInvestigationSlot(
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

    const int slotIndex = FindNearestUnclaimedSlotIndex(*context, npc);
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

} // namespace

void TopdownResetNpcInvestigationState(TopdownNpcRuntime& npc)
{
    npc.investigationContextHandle = -1;
    npc.investigationSlotIndex = -1;
    npc.investigationProgressTimerMs = 0.0f;
    npc.investigationLastDistance = 0.0f;
}

bool TopdownBeginNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;

    TopdownLeaveNpcInvestigationState(state, npc);

    TopdownNpcInvestigationContext* context =
            FindNearbyInvestigationContext(runtime, npc.lastKnownPlayerPosition);

    if (context == nullptr) {
        context = CreateInvestigationContext(
                runtime,
                npc.lastKnownPlayerPosition,
                npc.collisionRadius,
                npc.handle);
    }

    if (context == nullptr) {
        return false;
    }

    npc.investigationContextHandle = context->handle;
    if (!TryClaimNpcInvestigationSlot(runtime, npc)) {
        TopdownResetNpcInvestigationState(npc);
        return false;
    }

    npc.combatState = TopdownNpcCombatState::Investigation;
    npc.investigationProgressTimerMs = 0.0f;

    TopdownNpcInvestigationSlot& slot = context->slots[npc.investigationSlotIndex];
    npc.investigationLastDistance =
            TopdownLength(TopdownSub(slot.position, npc.position));

    TopdownBuildNpcPathToTarget(
            state,
            npc,
            slot.position,
            TopdownNpcMoveOwner::Ai);

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
    TopdownCleanupNpcInvestigationContexts(state);
}

void TopdownUpdateNpcInvestigationState(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    if (npc.combatState != TopdownNpcCombatState::Investigation) {
        return;
    }

    TopdownRuntimeData& runtime = state.topdown.runtime;
    TopdownNpcInvestigationContext* context =
            FindInvestigationContextByHandle(runtime, npc.investigationContextHandle);

    if (context == nullptr ||
        !TryClaimNpcInvestigationSlot(runtime, npc) ||
        npc.investigationSlotIndex < 0 ||
        npc.investigationSlotIndex >= static_cast<int>(context->slots.size())) {
        TopdownLeaveNpcInvestigationState(state, npc);
        TopdownBeginNpcSearchState(npc);
        return;
    }

    TopdownNpcInvestigationSlot& slot = context->slots[npc.investigationSlotIndex];

    const bool detectsPlayer =
            TopdownNpcCanSeePlayer(state, npc) ||
            TopdownNpcCanHearPlayer(state, npc);

    if (detectsPlayer) {
        TopdownLeaveNpcInvestigationState(state, npc);
        TopdownAlertNpcToPlayer(state, npc);
        npc.combatState = TopdownNpcCombatState::Chase;
        return;
    }

    if (TopdownHasNpcReachedPoint(npc, slot.position, kInvestigationArriveRadius)) {
        TopdownLeaveNpcInvestigationState(state, npc);
        TopdownBeginNpcSearchState(npc);
        return;
    }

    if (!npc.move.active || !npc.move.hasFinalTarget) {
        TopdownBuildNpcPathToTarget(
                state,
                npc,
                slot.position,
                TopdownNpcMoveOwner::Ai);

        if (!npc.move.active || !npc.move.hasFinalTarget) {
            TopdownLeaveNpcInvestigationState(state, npc);
            TopdownBeginNpcSearchState(npc);
            return;
        }
    }

    const float dtMs = dt * 1000.0f;
    npc.investigationProgressTimerMs += dtMs;

    const float currentDistance =
            TopdownLength(TopdownSub(slot.position, npc.position));

    if (npc.investigationProgressTimerMs >= kInvestigationProbePeriodMs) {
        const float progress = npc.investigationLastDistance - currentDistance;
        npc.investigationLastDistance = currentDistance;
        npc.investigationProgressTimerMs = 0.0f;

        if (progress < kInvestigationMinProgressPerProbe) {
            TopdownLeaveNpcInvestigationState(state, npc);
            TopdownBeginNpcSearchState(npc);
            return;
        }
    }
}

void TopdownCleanupNpcInvestigationContexts(GameState& state)
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
