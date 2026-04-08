#include "AdventureActionSystem.h"
#include "input/Input.h"
#include "nav/NavMeshQuery.h"
#include "scripting/ScriptSystem.h"
#include "scene/SceneHelpers.h"
#include "adventure/AdventureHelpers.h"
#include "render/RenderHelpers.h"


static int FindClickedActorIndex(const GameState& state, Vector2 clickScreen)
{
    const int controlledActorIndex = GetControlledActorIndex(state);

    int bestActorIndex = -1;
    float bestSortY = -1000000.0f;

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        if (i == controlledActorIndex) {
            continue;
        }

        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        const Rectangle actorRect = GetActorInteractionRect(state, actor);
        if (!CheckCollisionPointRec(clickScreen, actorRect)) {
            continue;
        }

        if (bestActorIndex < 0 || actor.feetPos.y > bestSortY) {
            bestActorIndex = i;
            bestSortY = actor.feetPos.y;
        }
    }

    return bestActorIndex;
}

static void QueuePathToTarget(
        GameState& state,
        Vector2 clickWorld,
        Vector2 walkTarget,
        bool fastMove,
        PendingInteractionType interactionType,
        int interactionIndex)
{
    ActorInstance* player = GetControlledActor(state);
    if (player == nullptr) {
        return;
    }
    state.adventure.pendingInteraction = {};

    const NavMeshData& navMesh = state.adventure.currentScene.navMesh;

    state.adventure.lastClickWorldPos = clickWorld;
    state.adventure.hasLastClickWorldPos = true;

    std::vector<Vector2> pathPoints;
    std::vector<int> trianglePath;
    Vector2 resolvedEnd{};

    const bool ok = BuildNavPath(
            navMesh,
            player->feetPos,
            walkTarget,
            pathPoints,
            &trianglePath,
            &resolvedEnd);

    if (ok) {
        player->path.points = pathPoints;
        player->path.currentPoint = 0;
        player->path.active = !player->path.points.empty();
        player->path.fastMove = fastMove;

        player->stoppedTimeMs = 0.0f;
        player->inIdleState = false;
        player->animationTimeMs = 0.0f;

        state.adventure.lastResolvedTargetPos = resolvedEnd;
        state.adventure.hasLastResolvedTargetPos = true;
        state.adventure.debugTrianglePath = trianglePath;

        state.adventure.pendingInteraction.type = interactionType;
        state.adventure.pendingInteraction.targetIndex = interactionIndex;
        state.adventure.pendingInteraction.active = (interactionType != PendingInteractionType::None);
    } else {
        player->path = {};
        state.adventure.hasLastResolvedTargetPos = false;
        state.adventure.debugTrianglePath.clear();
        state.adventure.pendingInteraction = {};
    }
}

void QueueAdventureActionsFromInput(GameState& state)
{
    if (!state.adventure.controlsEnabled || state.adventure.fadeInputBlocked) {
        for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
            if (ev.mouse.button == MOUSE_LEFT_BUTTON ||
                ev.mouse.button == MOUSE_RIGHT_BUTTON) {
                ConsumeEvent(ev);
            }
        }
        return;
    }

    if (!HasControlledActor(state) || !state.adventure.currentScene.loaded) {
        return;
    }

    const SceneData& scene = state.adventure.currentScene;

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
        const Vector2 clickScreen = ev.mouse.pos;
        const Vector2 clickWorld{
                clickScreen.x + state.adventure.camera.position.x,
                clickScreen.y + state.adventure.camera.position.y
        };

        bool consumed = false;

        if (ev.mouse.button == MOUSE_RIGHT_BUTTON) {
            const int actorIndex = FindClickedActorIndex(state, clickScreen);
            if (actorIndex >= 0) {
                state.adventure.actionQueue.push({
                                                         AdventureActionType::LookActor,
                                                         LookActorAction{actorIndex, clickWorld}
                                                 });
                consumed = true;
            }

            if (!consumed) {
                for (int i = static_cast<int>(scene.exits.size()) - 1; i >= 0; --i) {
                    if (PointInPolygon(clickWorld, scene.exits[i].shape)) {
                        state.adventure.actionQueue.push({
                                                                 AdventureActionType::LookExit,
                                                                 LookExitAction{i, clickWorld}
                                                         });
                        consumed = true;
                        break;
                    }
                }
            }

            if (!consumed) {
                for (int i = static_cast<int>(scene.hotspots.size()) - 1; i >= 0; --i) {
                    if (PointInPolygon(clickWorld, scene.hotspots[i].shape)) {
                        state.adventure.actionQueue.push({
                                                                 AdventureActionType::LookHotspot,
                                                                 LookHotspotAction{i, clickWorld}
                                                         });
                        consumed = true;
                        break;
                    }
                }
            }

            if (consumed) {
                ConsumeEvent(ev);
            }

            continue;
        }

        if (ev.mouse.button != MOUSE_LEFT_BUTTON) {
            continue;
        }

        const int actorIndex = FindClickedActorIndex(state, clickScreen);
        if (actorIndex >= 0) {
            state.adventure.actionQueue.push({
                                                     AdventureActionType::UseActor,
                                                     UseActorAction{actorIndex, clickWorld, ev.mouse.doubleClick}
                                             });
            consumed = true;
        }

        for (int i = static_cast<int>(scene.exits.size()) - 1; !consumed && i >= 0; --i) {
            if (PointInPolygon(clickWorld, scene.exits[i].shape)) {
                state.adventure.actionQueue.push({
                                                         AdventureActionType::UseExit,
                                                         UseExitAction{i, clickWorld, ev.mouse.doubleClick}
                                                 });
                consumed = true;
                break;
            }
        }

        if (!consumed) {
            for (int i = static_cast<int>(scene.hotspots.size()) - 1; i >= 0; --i) {
                if (PointInPolygon(clickWorld, scene.hotspots[i].shape)) {
                    state.adventure.actionQueue.push({
                                                             AdventureActionType::UseHotspot,
                                                             UseHotspotAction{i, clickWorld, ev.mouse.doubleClick}
                                                     });
                    consumed = true;
                    break;
                }
            }
        }

        if (!consumed) {
            state.adventure.actionQueue.clear();
            state.adventure.pendingInteraction = {};
            state.adventure.actionQueue.push({
                                                     AdventureActionType::WalkToPoint,
                                                     WalkToPointAction{clickWorld, clickWorld, ev.mouse.doubleClick}
                                             });
        }

        ConsumeEvent(ev);
    }
}

void ProcessAdventureActions(GameState& state)
{
    AdventureAction action;
    while (state.adventure.actionQueue.pop(action)) {
        switch (action.type) {
            case AdventureActionType::WalkToPoint:
            {
                const auto& a = std::get<WalkToPointAction>(action.payload);
                QueuePathToTarget(
                        state,
                        a.clickWorld,
                        a.walkTarget,
                        a.fastMove,
                        PendingInteractionType::None,
                        -1);
                break;
            }

            case AdventureActionType::UseHotspot:
            {
                const auto& a = std::get<UseHotspotAction>(action.payload);
                if (a.hotspotIndex < 0 || a.hotspotIndex >= static_cast<int>(state.adventure.currentScene.hotspots.size())) {
                    break;
                }

                const SceneHotspot& hotspot = state.adventure.currentScene.hotspots[a.hotspotIndex];

                const std::string clickMethod = BuildSceneMethodName("click_", hotspot.id);
                const ScriptCallResult clickResult = ScriptSystemCallTrigger(state, clickMethod);
                if (clickResult == ScriptCallResult::ImmediateTrue ||
                    clickResult == ScriptCallResult::StartedAsync ||
                    clickResult == ScriptCallResult::Busy)
                {
                    break;
                }

                QueuePathToTarget(
                        state,
                        a.clickWorld,
                        hotspot.walkTo,
                        a.fastMove,
                        PendingInteractionType::UseHotspot,
                        a.hotspotIndex);
                break;
            }

            case AdventureActionType::LookHotspot:
            {
                const auto& a = std::get<LookHotspotAction>(action.payload);
                if (a.hotspotIndex < 0 || a.hotspotIndex >= static_cast<int>(state.adventure.currentScene.hotspots.size())) {
                    break;
                }

                const SceneHotspot& hotspot = state.adventure.currentScene.hotspots[a.hotspotIndex];

                const std::string clickMethod = BuildSceneMethodName("click_", hotspot.id);
                const ScriptCallResult clickResult = ScriptSystemCallTrigger(state, clickMethod);
                if (clickResult == ScriptCallResult::ImmediateTrue ||
                    clickResult == ScriptCallResult::StartedAsync ||
                    clickResult == ScriptCallResult::Busy)
                {
                    break;
                }

                QueuePathToTarget(
                        state,
                        a.clickWorld,
                        hotspot.walkTo,
                        false,
                        PendingInteractionType::LookHotspot,
                        a.hotspotIndex);
                break;
            }

            case AdventureActionType::UseExit:
            {
                const auto& a = std::get<UseExitAction>(action.payload);
                if (a.exitIndex < 0 || a.exitIndex >= static_cast<int>(state.adventure.currentScene.exits.size())) {
                    break;
                }

                const SceneExit& exitObj = state.adventure.currentScene.exits[a.exitIndex];

                const std::string clickMethod = BuildSceneMethodName("click_", exitObj.id);
                const ScriptCallResult clickResult = ScriptSystemCallTrigger(state, clickMethod);
                if (clickResult == ScriptCallResult::ImmediateTrue ||
                    clickResult == ScriptCallResult::StartedAsync ||
                    clickResult == ScriptCallResult::Busy)
                {
                    break;
                }

                QueuePathToTarget(
                        state,
                        a.clickWorld,
                        exitObj.walkTo,
                        a.fastMove,
                        PendingInteractionType::UseExit,
                        a.exitIndex);
                break;
            }

            case AdventureActionType::LookExit:
            {
                const auto& a = std::get<LookExitAction>(action.payload);
                if (a.exitIndex < 0 || a.exitIndex >= static_cast<int>(state.adventure.currentScene.exits.size())) {
                    break;
                }

                const SceneExit& exitObj = state.adventure.currentScene.exits[a.exitIndex];

                const std::string clickMethod = BuildSceneMethodName("click_", exitObj.id);
                const ScriptCallResult clickResult = ScriptSystemCallTrigger(state, clickMethod);
                if (clickResult == ScriptCallResult::ImmediateTrue ||
                    clickResult == ScriptCallResult::StartedAsync ||
                    clickResult == ScriptCallResult::Busy)
                {
                    break;
                }

                QueuePathToTarget(
                        state,
                        a.clickWorld,
                        exitObj.walkTo,
                        false,
                        PendingInteractionType::LookExit,
                        a.exitIndex);
                break;
            }

            case AdventureActionType::UseActor:
            {
                const auto& a = std::get<UseActorAction>(action.payload);
                if (a.actorIndex < 0 || a.actorIndex >= static_cast<int>(state.adventure.actors.size())) {
                    break;
                }

                const ActorInstance& actor = state.adventure.actors[a.actorIndex];
                if (!actor.activeInScene || !actor.visible) {
                    break;
                }

                const std::string method = BuildSceneMethodName("use_actor_", actor.actorId);
                const ScriptCallResult scriptResult = ScriptSystemCallTrigger(state, method);
                if (scriptResult == ScriptCallResult::ImmediateTrue ||
                    scriptResult == ScriptCallResult::StartedAsync ||
                    scriptResult == ScriptCallResult::Busy)
                {
                    break;
                }

                break;
            }

            case AdventureActionType::LookActor:
            {
                const auto& a = std::get<LookActorAction>(action.payload);
                if (a.actorIndex < 0 || a.actorIndex >= static_cast<int>(state.adventure.actors.size())) {
                    break;
                }

                const ActorInstance& actor = state.adventure.actors[a.actorIndex];
                if (!actor.activeInScene || !actor.visible) {
                    break;
                }

                const std::string method = BuildSceneMethodName("look_actor_", actor.actorId);
                const ScriptCallResult scriptResult = ScriptSystemCallTrigger(state, method);
                if (scriptResult == ScriptCallResult::ImmediateTrue ||
                    scriptResult == ScriptCallResult::StartedAsync ||
                    scriptResult == ScriptCallResult::Busy)
                {
                    break;
                }

                break;
            }
        }
    }
}

