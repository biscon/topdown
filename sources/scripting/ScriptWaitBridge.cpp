#include "scripting/ScriptWaitBridge.h"

#include "adventure/AdventureHelpers.h"
#include "adventure/Dialogue.h"

bool ScriptIsWalkWaitComplete(GameState& state, const ScriptCoroutine& co)
{
    (void)co;

    if (state.mode == GameMode::TopDown) {
        return !state.topdown.runtime.scriptedMove.active;
    }

    int actorIndex = co.waitActorIndex;
    if (actorIndex < 0) {
        actorIndex = GetControlledActorIndex(state);
    }

    return
            actorIndex >= 0 &&
            actorIndex < static_cast<int>(state.adventure.actors.size()) &&
            !state.adventure.actors[actorIndex].path.active;
}

bool ScriptIsSpeechWaitComplete(GameState& state)
{
    if (state.mode == GameMode::TopDown) {
        return true;
    }

    return !state.adventure.speechUi.active;
}

bool ScriptTryConsumeDialogueResult(GameState& state, std::string& outResult)
{
    outResult.clear();

    if (state.mode == GameMode::TopDown) {
        return false;
    }

    if (!state.adventure.dialogueUi.resultReady) {
        return false;
    }

    outResult = state.adventure.dialogueUi.selectedOptionId;
    state.adventure.dialogueUi = {};
    return true;
}
