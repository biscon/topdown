#include "topdown/TopdownNpcPatrol.h"

#include <algorithm>

#include "raylib.h"
#include "nav/NavMeshQuery.h"
#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcAiCommon.h"
#include "topdown/TopdownNpcRingSlots.h"

namespace {
    constexpr float kPatrolArriveRadius = 14.0f;
    static constexpr float kPatrolStuckCheckIntervalMs = 500.0f;
    static constexpr float kPatrolStuckMinProgressSqr = 12.0f * 12.0f;

    static constexpr float kPatrolYieldMinMs = 300.0f;
    static constexpr float kPatrolYieldMaxMs = 900.0f;
    static constexpr float kPatrolRetryDelayMinMs = 50.0f;
    static constexpr float kPatrolRetryDelayMaxMs = 150.0f;

    static constexpr int kPatrolBacktrackAfterStuckCount = 3;
    static constexpr float kPatrolBacktrackChance = 0.5f;

    const TopdownAuthoredSpawn* FindSpawnById(GameState& state, const std::string& spawnId)
    {
        for (const TopdownAuthoredSpawn& spawn : state.topdown.authored.spawns) {
            if (spawn.id == spawnId) {
                return &spawn;
            }
        }

        return nullptr;
    }

    TopdownNpcPatrolContext* FindPatrolContextByHandle(
            TopdownRuntimeData& runtime,
            int handle)
    {
        for (TopdownNpcPatrolContext& context : runtime.npcPatrolContexts) {
            if (context.active && context.handle == handle) {
                return &context;
            }
        }

        return nullptr;
    }

    TopdownNpcPatrolContext* FindPatrolContextByWaypointSpawnId(
            TopdownRuntimeData& runtime,
            const std::string& waypointSpawnId)
    {
        for (TopdownNpcPatrolContext& context : runtime.npcPatrolContexts) {
            if (!context.active) {
                continue;
            }

            if (context.waypointSpawnId == waypointSpawnId) {
                return &context;
            }
        }

        return nullptr;
    }

    TopdownNpcPatrolContext* CreatePatrolContext(
            TopdownRuntimeData& runtime,
            const std::string& waypointSpawnId,
            Vector2 origin,
            float npcRadius,
            int ownerNpcHandle)
    {
        TopdownNpcRingSlotBuildConfig ringConfig;
        ringConfig.candidatePadding = 8.0f;
        ringConfig.maxRings = 2;
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

        TopdownNpcPatrolContext newContext;
        newContext.active = true;
        newContext.handle = runtime.nextNpcPatrolContextHandle++;
        newContext.waypointSpawnId = waypointSpawnId;
        newContext.origin = origin;
        newContext.slots.reserve(slotPositions.size());

        for (const Vector2& slotPos : slotPositions) {
            TopdownNpcPatrolSlot slot;
            slot.position = slotPos;
            newContext.slots.push_back(slot);
        }

        runtime.npcPatrolContexts.push_back(newContext);
        return &runtime.npcPatrolContexts.back();
    }

    void StopPatrolScriptMove(TopdownNpcRuntime& npc)
    {
        if (!npc.move.active || npc.move.owner != TopdownNpcMoveOwner::Patrol) {
            return;
        }

        TopdownStopNpcMovement(npc);
    }

    void ResetPatrolDeadlockWatchdog(TopdownNpcRuntime& npc)
    {
        npc.patrolLastProgressPosition = npc.position;
        npc.patrolStuckTimerMs = 0.0f;
        npc.patrolYieldTimerMs = 0.0f;
        npc.patrolRetryDelayMs = 0.0f;
        npc.patrolStuckCount = 0;
        npc.patrolIsYielding = false;
        npc.patrolIsRetryDelay = false;
    }

    void BeginPatrolProgressTracking(TopdownNpcRuntime& npc)
    {
        npc.patrolLastProgressPosition = npc.position;
        npc.patrolStuckTimerMs = 0.0f;
    }

    void PatrolGoToPreviousWaypoint(TopdownNpcRuntime& npc)
    {
        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
        if (patrol.spawnIds.empty()) {
            patrol.currentPointIndex = 0;
            return;
        }

        if (patrol.currentPointIndex <= 0) {
            patrol.currentPointIndex =
                    patrol.loop
                            ? static_cast<int>(patrol.spawnIds.size()) - 1
                            : 0;
            return;
        }

        patrol.currentPointIndex -= 1;
    }

    void PatrolReissueMoveToCurrentWaypoint(
            GameState& state,
            TopdownNpcRuntime& npc,
            Vector2 patrolTarget,
            const TopdownNpcPatrolState& patrol)
    {
        TopdownBuildNpcPathToTarget(
                state,
                npc,
                patrolTarget,
                TopdownNpcMoveOwner::Patrol);

        if (npc.move.active && npc.move.owner == TopdownNpcMoveOwner::Patrol) {
            npc.move.running = patrol.running;
            npc.running = patrol.running;
            BeginPatrolProgressTracking(npc);
        }
    }

    void EnterPatrolYield(TopdownNpcRuntime& npc)
    {
        npc.patrolIsYielding = true;
        npc.patrolIsRetryDelay = false;
        npc.patrolYieldTimerMs = RandomRangeFloat(kPatrolYieldMinMs, kPatrolYieldMaxMs);
        TopdownStopNpcMovement(npc);
    }

    void ResetPatrolContextMembership(TopdownNpcRuntime& npc)
    {
        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
        patrol.contextHandle = -1;
        patrol.slotIndex = -1;
    }

    void ReleaseNpcPatrolSlot(
            TopdownRuntimeData& runtime,
            TopdownNpcRuntime& npc)
    {
        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;

        TopdownNpcPatrolContext* context =
                FindPatrolContextByHandle(runtime, patrol.contextHandle);

        if (context != nullptr &&
            patrol.slotIndex >= 0 &&
            patrol.slotIndex < static_cast<int>(context->slots.size())) {
            TopdownNpcPatrolSlot& slot = context->slots[patrol.slotIndex];
            if (slot.claimedByNpcHandle == npc.handle) {
                slot.claimedByNpcHandle = -1;
            }
        }

        ResetPatrolContextMembership(npc);
    }

    int FindRandomReachablePatrolSlotIndex(
            GameState& state,
            const TopdownNpcPatrolContext& context,
            const TopdownNpcRuntime& npc)
    {
        std::vector<int> reachableSlotIndices;
        reachableSlotIndices.reserve(context.slots.size());

        for (int i = 0; i < static_cast<int>(context.slots.size()); ++i) {
            const TopdownNpcPatrolSlot& slot = context.slots[i];

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

    bool TryClaimNpcPatrolSlot(
            GameState& state,
            TopdownRuntimeData& runtime,
            TopdownNpcRuntime& npc)
    {
        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
        TopdownNpcPatrolContext* context =
                FindPatrolContextByHandle(runtime, patrol.contextHandle);

        if (context == nullptr) {
            return false;
        }

        if (patrol.slotIndex >= 0 &&
            patrol.slotIndex < static_cast<int>(context->slots.size())) {
            TopdownNpcPatrolSlot& slot = context->slots[patrol.slotIndex];
            if (slot.claimedByNpcHandle == npc.handle) {
                return true;
            }
        }

        const int slotIndex = FindRandomReachablePatrolSlotIndex(state, *context, npc);
        if (slotIndex < 0) {
            patrol.slotIndex = -1;
            return false;
        }

        context->slots[slotIndex].claimedByNpcHandle = npc.handle;
        patrol.slotIndex = slotIndex;
        return true;
    }

    bool EnsurePatrolSlotForWaypoint(
            GameState& state,
            TopdownNpcRuntime& npc,
            const TopdownAuthoredSpawn& targetSpawn)
    {
        TopdownRuntimeData& runtime = state.topdown.runtime;
        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;

        TopdownNpcPatrolContext* currentContext =
                FindPatrolContextByHandle(runtime, patrol.contextHandle);

        if (currentContext != nullptr &&
            currentContext->waypointSpawnId != targetSpawn.id) {
            ReleaseNpcPatrolSlot(runtime, npc);
            currentContext = nullptr;
        }

        if (currentContext == nullptr) {
            TopdownNpcPatrolContext* context =
                    FindPatrolContextByWaypointSpawnId(runtime, targetSpawn.id);

            if (context == nullptr) {
                context = CreatePatrolContext(
                        runtime,
                        targetSpawn.id,
                        targetSpawn.position,
                        npc.collisionRadius,
                        npc.handle);
            }

            if (context == nullptr) {
                return false;
            }

            patrol.contextHandle = context->handle;
            patrol.slotIndex = -1;
        }

        return TryClaimNpcPatrolSlot(state, runtime, npc);
    }

    bool TryGetPatrolTargetPoint(
            TopdownRuntimeData& runtime,
            const TopdownNpcRuntime& npc,
            Vector2& outTargetPoint)
    {
        const TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;

        TopdownNpcPatrolContext* context =
                FindPatrolContextByHandle(runtime, patrol.contextHandle);

        if (context == nullptr ||
            patrol.slotIndex < 0 ||
            patrol.slotIndex >= static_cast<int>(context->slots.size())) {
            return false;
        }

        const TopdownNpcPatrolSlot& slot = context->slots[patrol.slotIndex];
        if (slot.claimedByNpcHandle != npc.handle) {
            return false;
        }

        outTargetPoint = slot.position;
        return true;
    }

    bool NpcIsUsingPatrolContext(
            const TopdownRuntimeData& runtime,
            int contextHandle)
    {
        for (const TopdownNpcRuntime& npc : runtime.npcs) {
            if (!npc.active || npc.dead || npc.corpse) {
                continue;
            }

            if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute) {
                continue;
            }

            const TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
            if (!patrol.active || patrol.paused) {
                continue;
            }

            if (patrol.contextHandle == contextHandle) {
                return true;
            }
        }

        return false;
    }

    void CleanupInactivePatrolClaims(TopdownRuntimeData& runtime)
    {
        for (TopdownNpcPatrolContext& context : runtime.npcPatrolContexts) {
            if (!context.active) {
                continue;
            }

            for (int slotIndex = 0; slotIndex < static_cast<int>(context.slots.size()); ++slotIndex) {
                TopdownNpcPatrolSlot& slot = context.slots[slotIndex];
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

                    if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute) {
                        break;
                    }

                    const TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
                    if (!patrol.active || patrol.paused) {
                        break;
                    }

                    if (patrol.contextHandle != context.handle) {
                        break;
                    }

                    if (patrol.slotIndex != slotIndex) {
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

    void AdvancePatrolPoint(
            GameState& state,
            TopdownNpcRuntime& npc)
    {
        TopdownRuntimeData& runtime = state.topdown.runtime;
        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;

        ReleaseNpcPatrolSlot(runtime, npc);

        patrol.currentPointIndex++;
        patrol.waitTimerMs = 0.0f;

        if (patrol.currentPointIndex < static_cast<int>(patrol.spawnIds.size())) {
            return;
        }

        if (patrol.loop && !patrol.spawnIds.empty()) {
            patrol.currentPointIndex = 0;
            return;
        }

        TopdownClearNpcPatrol(state, npc);
    }
} // namespace

bool TopdownAssignNpcPatrolRoute(
        GameState& state,
        TopdownNpcRuntime& npc,
        const std::vector<std::string>& spawnIds,
        const TopdownNpcPatrolRouteOptions& options)
{
    if (spawnIds.empty()) {
        TraceLog(LOG_WARNING, "NPC '%s': patrol route requires at least one waypoint", npc.id.c_str());
        return false;
    }

    for (const std::string& spawnId : spawnIds) {
        if (spawnId.empty()) {
            TraceLog(LOG_WARNING, "NPC '%s': patrol route contains an empty spawn id", npc.id.c_str());
            return false;
        }

        if (FindSpawnById(state, spawnId) == nullptr) {
            TraceLog(
                    LOG_WARNING,
                    "NPC '%s': patrol waypoint spawn '%s' not found",
                    npc.id.c_str(),
                    spawnId.c_str());
            return false;
        }
    }

    ReleaseNpcPatrolSlot(state.topdown.runtime, npc);

    TopdownNpcScriptBehaviorState& behavior = npc.scriptBehavior;
    behavior = {};
    behavior.mode = TopdownNpcScriptBehaviorMode::PatrolRoute;
    behavior.patrol.active = true;
    behavior.patrol.loop = options.loop;
    behavior.patrol.running = options.running;
    behavior.patrol.spawnIds = spawnIds;
    behavior.patrol.waitDurationMs = std::max(0.0f, options.waitMs);
    behavior.patrol.waitTimerMs = 0.0f;
    behavior.patrol.currentPointIndex = 0;
    behavior.patrol.interrupted = false;

    ResetPatrolDeadlockWatchdog(npc);
    StopPatrolScriptMove(npc);
    return true;
}

void TopdownClearNpcPatrol(GameState& state, TopdownNpcRuntime& npc)
{
    ReleaseNpcPatrolSlot(state.topdown.runtime, npc);
    StopPatrolScriptMove(npc);
    ResetPatrolDeadlockWatchdog(npc);
    npc.scriptBehavior = {};
}

bool TopdownPauseNpcPatrol(GameState& state, TopdownNpcRuntime& npc)
{
    if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute ||
        !npc.scriptBehavior.patrol.active) {
        return false;
    }

    ReleaseNpcPatrolSlot(state.topdown.runtime, npc);
    npc.scriptBehavior.patrol.paused = true;
    StopPatrolScriptMove(npc);
    ResetPatrolDeadlockWatchdog(npc);
    return true;
}

bool TopdownInterruptNpcPatrol(GameState& state, TopdownNpcRuntime& npc)
{
    if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute ||
        !npc.scriptBehavior.patrol.active) {
        return false;
    }

    TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
    patrol.interrupted = true;
    patrol.waitTimerMs = 0.0f;
    ReleaseNpcPatrolSlot(state.topdown.runtime, npc);
    StopPatrolScriptMove(npc);
    ResetPatrolDeadlockWatchdog(npc);
    return true;
}

bool TopdownResumeNpcPatrol(TopdownNpcRuntime& npc)
{
    if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute ||
        !npc.scriptBehavior.patrol.active) {
        return false;
    }

    npc.scriptBehavior.patrol.paused = false;
    npc.scriptBehavior.patrol.interrupted = false;
    ResetPatrolDeadlockWatchdog(npc);
    return true;
}

void TopdownUpdateNpcPatrol(
        GameState& state,
        TopdownNpcRuntime& npc,
        float dt)
{
    TopdownNpcScriptBehaviorState& behavior = npc.scriptBehavior;
    if (behavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute) {
        return;
    }

    TopdownNpcPatrolState& patrol = behavior.patrol;
    if (!patrol.active || patrol.paused) {
        return;
    }

    if (patrol.interrupted) {
        patrol.interrupted = false;
    }

    if (patrol.spawnIds.empty()) {
        TopdownClearNpcPatrol(state, npc);
        return;
    }

    if (patrol.currentPointIndex < 0 ||
        patrol.currentPointIndex >= static_cast<int>(patrol.spawnIds.size())) {
        TopdownClearNpcPatrol(state, npc);
        return;
    }

    const TopdownAuthoredSpawn* targetSpawn =
            FindSpawnById(state, patrol.spawnIds[patrol.currentPointIndex]);

    if (targetSpawn == nullptr) {
        TraceLog(
                LOG_WARNING,
                "NPC '%s': patrol waypoint spawn '%s' disappeared during update",
                npc.id.c_str(),
                patrol.spawnIds[patrol.currentPointIndex].c_str());
        TopdownClearNpcPatrol(state, npc);
        return;
    }

    const bool hasSlotTarget = EnsurePatrolSlotForWaypoint(state, npc, *targetSpawn);
    Vector2 patrolTarget = targetSpawn->position;
    if (hasSlotTarget) {
        Vector2 slotTarget{};
        if (TryGetPatrolTargetPoint(state.topdown.runtime, npc, slotTarget)) {
            patrolTarget = slotTarget;
        }
    }

    if (patrol.waitTimerMs > 0.0f) {
        patrol.waitTimerMs -= dt * 1000.0f;
        if (patrol.waitTimerMs <= 0.0f) {
            AdvancePatrolPoint(state, npc);
            BeginPatrolProgressTracking(npc);
        }
        return;
    }

    const bool hasActivePatrolMove =
            npc.move.active &&
            npc.move.owner == TopdownNpcMoveOwner::Patrol;

    const float dtMs = dt * 1000.0f;
    if (npc.patrolIsYielding) {
        npc.patrolYieldTimerMs -= dtMs;
        if (npc.patrolYieldTimerMs > 0.0f) {
            return;
        }

        npc.patrolIsYielding = false;
        npc.patrolYieldTimerMs = 0.0f;
        npc.patrolIsRetryDelay = true;
        npc.patrolRetryDelayMs =
                RandomRangeFloat(kPatrolRetryDelayMinMs, kPatrolRetryDelayMaxMs);
        return;
    }

    if (npc.patrolIsRetryDelay) {
        npc.patrolRetryDelayMs -= dtMs;
        if (npc.patrolRetryDelayMs > 0.0f) {
            return;
        }

        if (npc.patrolStuckCount >= kPatrolBacktrackAfterStuckCount) {
            const float backtrackRoll = RandomRangeFloat(0.0f, 1.0f);
            if (backtrackRoll < kPatrolBacktrackChance) {
                PatrolGoToPreviousWaypoint(npc);
                ResetPatrolDeadlockWatchdog(npc);
                BeginPatrolProgressTracking(npc);
                npc.patrolStuckCount = 0;
                patrol.waitTimerMs = 0.0f;
                ReleaseNpcPatrolSlot(state.topdown.runtime, npc);
                if (patrol.currentPointIndex < 0 ||
                    patrol.currentPointIndex >= static_cast<int>(patrol.spawnIds.size())) {
                    TopdownClearNpcPatrol(state, npc);
                    return;
                }

                const TopdownAuthoredSpawn* backtrackSpawn =
                        FindSpawnById(state, patrol.spawnIds[patrol.currentPointIndex]);
                if (backtrackSpawn == nullptr) {
                    TopdownClearNpcPatrol(state, npc);
                    return;
                }

                Vector2 backtrackTarget = backtrackSpawn->position;
                if (EnsurePatrolSlotForWaypoint(state, npc, *backtrackSpawn)) {
                    Vector2 slotTarget{};
                    if (TryGetPatrolTargetPoint(state.topdown.runtime, npc, slotTarget)) {
                        backtrackTarget = slotTarget;
                    }
                }

                npc.patrolIsRetryDelay = false;
                npc.patrolRetryDelayMs = 0.0f;
                BeginPatrolProgressTracking(npc);
                PatrolReissueMoveToCurrentWaypoint(state, npc, backtrackTarget, patrol);
                return;
            }
        }

        const TopdownAuthoredSpawn* retrySpawn =
                FindSpawnById(state, patrol.spawnIds[patrol.currentPointIndex]);
        if (retrySpawn == nullptr) {
            TopdownClearNpcPatrol(state, npc);
            return;
        }

        Vector2 retryTarget = retrySpawn->position;
        if (EnsurePatrolSlotForWaypoint(state, npc, *retrySpawn)) {
            Vector2 slotTarget{};
            if (TryGetPatrolTargetPoint(state.topdown.runtime, npc, slotTarget)) {
                retryTarget = slotTarget;
            }
        }

        npc.patrolIsRetryDelay = false;
        npc.patrolRetryDelayMs = 0.0f;
        BeginPatrolProgressTracking(npc);
        PatrolReissueMoveToCurrentWaypoint(state, npc, retryTarget, patrol);
        return;
    }

    if (hasActivePatrolMove) {
        const bool patrolMoveMatchesTarget =
                npc.move.hasFinalTarget &&
                TopdownLengthSqr(TopdownSub(npc.move.finalTarget, patrolTarget)) <= 1.0f;

        if (patrolMoveMatchesTarget) {
            npc.move.running = patrol.running;
            npc.running = patrol.running;

            npc.patrolStuckTimerMs += dtMs;
            if (npc.patrolStuckTimerMs >= kPatrolStuckCheckIntervalMs) {
                const float distSqr =
                        TopdownLengthSqr(TopdownSub(npc.position, npc.patrolLastProgressPosition));
                if (distSqr < kPatrolStuckMinProgressSqr) {
                    npc.patrolStuckCount++;
                    EnterPatrolYield(npc);
                } else {
                    npc.patrolStuckCount = 0;
                }

                npc.patrolLastProgressPosition = npc.position;
                npc.patrolStuckTimerMs = 0.0f;
            }
            return;
        }

        TopdownStopNpcMovement(npc);
        BeginPatrolProgressTracking(npc);
    }

    if (TopdownHasNpcReachedPoint(npc, patrolTarget, kPatrolArriveRadius)) {
        if (patrol.waitDurationMs > 0.0f) {
            patrol.waitTimerMs = patrol.waitDurationMs;
            BeginPatrolProgressTracking(npc);
            return;
        }

        AdvancePatrolPoint(state, npc);
        BeginPatrolProgressTracking(npc);
        return;
    }

    PatrolReissueMoveToCurrentWaypoint(state, npc, patrolTarget, patrol);
}

void TopdownPruneNpcPatrolContexts(GameState& state)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;

    CleanupInactivePatrolClaims(runtime);

    for (TopdownNpcPatrolContext& context : runtime.npcPatrolContexts) {
        if (!context.active) {
            continue;
        }

        if (!NpcIsUsingPatrolContext(runtime, context.handle)) {
            context.active = false;
            context.waypointSpawnId.clear();
            context.slots.clear();
        }
    }
}
