#include "topdown/LevelUpdate.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/LevelCamera.h"
#include "topdown/PlayerUpdate.h"
#include "topdown/TopdownCombat.h"
#include "raylib.h"
#include "NpcRegistry.h"
#include "NpcUpdate.h"
#include "menu/Menu.h"
#include "LevelLoad.h"
#include "utils/ScopeTimer.h"
#include "scripting/ScriptSystem.h"
#include "LevelDoors.h"
#include "LevelEffects.h"
#include "LevelWindows.h"
#include "LevelProps.h"
#include "ui/NarrationPopups.h"

static bool IsPointInsideTrigger(
        const TopdownAuthoredTrigger& trigger,
        Vector2 point)
{
    if (trigger.usePolygon) {
        return TopdownPointInPolygon(point, trigger.polygon);
    }

    return CheckCollisionPointRec(point, trigger.worldRect);
}

static void QueueTriggerCall(
        TopdownRuntimeTrigger& trigger,
        int authoredIndex,
        TopdownCharacterHandle instigatorHandle,
        bool instigatorIsPlayer,
        float delayMs)
{
    TopdownRuntimeTriggerPendingCall call{};
    call.active = true;
    call.authoredIndex = authoredIndex;
    call.instigatorHandle = instigatorHandle;
    call.instigatorIsPlayer = instigatorIsPlayer;
    call.remainingMs = delayMs;
    trigger.pendingCalls.push_back(call);
}

static void UpdateSingleTriggerPendingCalls(
        GameState& state,
        TopdownRuntimeTrigger& runtimeTrigger,
        float dt)
{
    const TopdownAuthoredTrigger& authored =
            state.topdown.authored.triggers[runtimeTrigger.authoredIndex];

    std::vector<TopdownRuntimeTriggerPendingCall> retained;
    retained.reserve(runtimeTrigger.pendingCalls.size());

    for (TopdownRuntimeTriggerPendingCall& pending : runtimeTrigger.pendingCalls) {
        if (!pending.active) {
            continue;
        }

        pending.remainingMs -= dt * 1000.0f;
        if (pending.remainingMs > 0.0f) {
            retained.push_back(pending);
            continue;
        }

        if (!runtimeTrigger.enabled || authored.script.empty()) {
            continue;
        }

        const ScriptCallResult result =
                ScriptSystemCallTrigger(state, authored.script);
        if (result == ScriptCallResult::Error) {
            TraceLog(LOG_WARNING,
                     "Topdown trigger '%s' script '%s' failed",
                     authored.id.c_str(),
                     authored.script.c_str());
        }
    }

    runtimeTrigger.pendingCalls.swap(retained);
}

static bool TriggerAffectsPlayer(TopdownTriggerAffects affects)
{
    return affects == TopdownTriggerAffects::Player ||
           affects == TopdownTriggerAffects::All;
}

static bool TriggerAffectsNpc(TopdownTriggerAffects affects)
{
    return affects == TopdownTriggerAffects::Npc ||
           affects == TopdownTriggerAffects::All;
}

static void UpdateTriggers(GameState& state, float dt)
{
    for (TopdownRuntimeTrigger& runtimeTrigger : state.topdown.runtime.triggers) {
        if (runtimeTrigger.authoredIndex < 0 ||
            runtimeTrigger.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
            continue;
        }

        const TopdownAuthoredTrigger& authored =
                state.topdown.authored.triggers[runtimeTrigger.authoredIndex];

        if (!runtimeTrigger.enabled) {
            runtimeTrigger.playerInside = false;
            runtimeTrigger.npcHandlesInside.clear();
            runtimeTrigger.pendingCalls.clear();
            continue;
        }

        const bool allowNewEntries = runtimeTrigger.repeat || !runtimeTrigger.fired;
        bool enteredThisFrame = false;

        if (TriggerAffectsPlayer(authored.affects)) {
            const bool playerInsideNow =
                    IsPointInsideTrigger(authored, state.topdown.runtime.player.position);

            if (allowNewEntries && playerInsideNow && !runtimeTrigger.playerInside) {
                enteredThisFrame = true;
                QueueTriggerCall(
                        runtimeTrigger,
                        runtimeTrigger.authoredIndex,
                        0,
                        true,
                        authored.delayMs);
            }

            runtimeTrigger.playerInside = playerInsideNow;
        } else {
            runtimeTrigger.playerInside = false;
        }

        if (TriggerAffectsNpc(authored.affects)) {
            std::vector<TopdownCharacterHandle> npcInsideNow;
            npcInsideNow.reserve(state.topdown.runtime.npcs.size());

            for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
                if (!npc.active || !npc.visible || npc.dead) {
                    continue;
                }

                if (!IsPointInsideTrigger(authored, npc.position)) {
                    continue;
                }

                npcInsideNow.push_back(npc.handle);

                const bool wasInside = std::find(
                        runtimeTrigger.npcHandlesInside.begin(),
                        runtimeTrigger.npcHandlesInside.end(),
                        npc.handle) != runtimeTrigger.npcHandlesInside.end();

                if (allowNewEntries && !wasInside) {
                    enteredThisFrame = true;
                    QueueTriggerCall(
                            runtimeTrigger,
                            runtimeTrigger.authoredIndex,
                            npc.handle,
                            false,
                            authored.delayMs);
                }
            }

            runtimeTrigger.npcHandlesInside.swap(npcInsideNow);
        } else {
            runtimeTrigger.npcHandlesInside.clear();
        }

        if (enteredThisFrame) {
            runtimeTrigger.fired = true;
        }

        UpdateSingleTriggerPendingCalls(state, runtimeTrigger, dt);
    }
}

static void PruneWorldEvents(GameState& state) {
    const float nowMs = state.topdown.runtime.timeMs;
    auto& events = state.topdown.runtime.worldEvents;
    events.erase(
            std::remove_if(
                    events.begin(),
                    events.end(),
                    [nowMs](const TopdownWorldEvent& evt)
                    {
                        return (nowMs - evt.createdAtMs) > evt.ttlMs;
                    }),
            events.end());
}

void TopdownUpdate(GameState& state, float dt)
{
    if (!state.topdown.runtime.levelActive) {
        return;
    }

    TopdownPlayerRuntime& player = state.topdown.runtime.player;
    TopdownRuntimeData& runtime = state.topdown.runtime;

    // Tick global timer
    runtime.timeMs += dt * 1000.0f;

    // Prune stale events
    PruneWorldEvents(state);

    // --- trigger game over ---
    if (!runtime.gameOverActive &&
        player.lifeState == TopdownPlayerLifeState::Dead &&
        player.health <= 0.0f)
    {
        runtime.gameOverActive = true;
        runtime.gameOverElapsedMs = 0.0f;
        runtime.returnToMenuRequested = false;

        runtime.controlsEnabled = false;

        TraceLog(LOG_INFO, "Game Over triggered");
    }

    TopdownUpdateDoors(state, dt);

    TopdownUpdatePlayerLogic(state, dt);
    TopdownUpdateNpcLogic(state, dt);

    ApplyDoorMotionPushToPlayer(state, dt);
    ApplyDoorMotionPushToNpcs(state, dt);

    TopdownUpdateCombat(state, dt);

    TopdownUpdateLevelEffects(state, dt);
    TopdownUpdateWindows(state, dt);
    TopdownUpdateProps(state, dt);
    UpdateTriggers(state, dt);
    TopdownUpdateNarrationPopups(state, dt);

    TopdownUpdatePlayerAnimation(state, dt);
    TopdownUpdateNpcAnimation(state, dt);
    TopdownUpdateCamera(state, dt);
    {
        //ScopeTimer t("RebuildWallOcclusionPolygons");
        TopdownRebuildWallOcclusionPolygons(state.topdown, false);
    }

    // --- update game over ---
    if (runtime.gameOverActive) {
        runtime.gameOverElapsedMs += dt * 1000.0f;

        if (runtime.returnToMenuRequested) {
            TraceLog(LOG_INFO, "Returning to menu from game over");

            TopdownUnloadLevel(state);

            runtime = {}; // wipe runtime clean (important)

            state.topdown.currentLevelId.clear();
            state.topdown.currentLevelSaveName.clear();

            state.mode = GameMode::Menu;

            MenuInit(&state); // resets to "Start New Game" state

            return;
        }
    }
}
