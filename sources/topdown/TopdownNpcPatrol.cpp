#include "topdown/TopdownNpcPatrol.h"

#include <algorithm>

#include "raylib.h"
#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownNpcAiCommon.h"

static const TopdownAuthoredSpawn* FindSpawnById(GameState& state, const std::string& spawnId)
{
    for (const TopdownAuthoredSpawn& spawn : state.topdown.authored.spawns) {
        if (spawn.id == spawnId) {
            return &spawn;
        }
    }

    return nullptr;
}

static void StopPatrolScriptMove(TopdownNpcRuntime& npc)
{
    if (!npc.move.active || npc.move.owner != TopdownNpcMoveOwner::Script) {
        return;
    }

    TopdownStopNpcMovement(npc);
}

static void AdvancePatrolPoint(TopdownNpcRuntime& npc)
{
    TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
    patrol.currentPointIndex++;
    patrol.waitTimerMs = 0.0f;

    if (patrol.currentPointIndex < static_cast<int>(patrol.spawnIds.size())) {
        return;
    }

    if (patrol.loop && !patrol.spawnIds.empty()) {
        patrol.currentPointIndex = 0;
        return;
    }

    TopdownClearNpcPatrol(npc);
}

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

    StopPatrolScriptMove(npc);
    return true;
}

void TopdownClearNpcPatrol(TopdownNpcRuntime& npc)
{
    StopPatrolScriptMove(npc);
    npc.scriptBehavior = {};
}

bool TopdownPauseNpcPatrol(TopdownNpcRuntime& npc)
{
    if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute ||
        !npc.scriptBehavior.patrol.active) {
        return false;
    }

    npc.scriptBehavior.patrol.paused = true;
    StopPatrolScriptMove(npc);
    return true;
}

bool TopdownResumeNpcPatrol(TopdownNpcRuntime& npc)
{
    if (npc.scriptBehavior.mode != TopdownNpcScriptBehaviorMode::PatrolRoute ||
        !npc.scriptBehavior.patrol.active) {
        return false;
    }

    npc.scriptBehavior.patrol.paused = false;
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

    if (patrol.spawnIds.empty()) {
        TopdownClearNpcPatrol(npc);
        return;
    }

    if (patrol.currentPointIndex < 0 ||
        patrol.currentPointIndex >= static_cast<int>(patrol.spawnIds.size())) {
        TopdownClearNpcPatrol(npc);
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
        TopdownClearNpcPatrol(npc);
        return;
    }

    if (patrol.waitTimerMs > 0.0f) {
        patrol.waitTimerMs -= dt * 1000.0f;
        if (patrol.waitTimerMs <= 0.0f) {
            AdvancePatrolPoint(npc);
        }
        return;
    }

    if (npc.move.active && npc.move.owner == TopdownNpcMoveOwner::Script) {
        npc.move.running = patrol.running;
        npc.running = patrol.running;
        return;
    }

    if (TopdownHasNpcReachedPoint(npc, targetSpawn->position, 14.0f)) {
        if (patrol.waitDurationMs > 0.0f) {
            patrol.waitTimerMs = patrol.waitDurationMs;
            return;
        }

        AdvancePatrolPoint(npc);
        return;
    }

    TopdownBuildNpcPathToTarget(
            state,
            npc,
            targetSpawn->position,
            TopdownNpcMoveOwner::Script);

    if (npc.move.active && npc.move.owner == TopdownNpcMoveOwner::Script) {
        npc.move.running = patrol.running;
        npc.running = patrol.running;
    }
}

