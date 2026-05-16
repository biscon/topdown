#include "scripting/ScriptWaitBridge.h"

#include "cutscene/CutsceneMode.h"


bool ScriptIsWalkWaitComplete(GameState& state, const ScriptCoroutine& co)
{
    (void)co;
    if (state.mode == GameMode::TopDown) {
        return !state.topdown.runtime.scriptedMove.active;
    }

    return true;
}

bool ScriptIsSpeechWaitComplete(GameState& state)
{
    if (state.mode == GameMode::TopDown) {
        return true;
    }

    return true;
}

bool ScriptTryConsumeDialogueResult(GameState& state, std::string& outResult)
{
    outResult.clear();

    if (state.mode == GameMode::TopDown) {
        return false;
    }
    return true;
}


bool ScriptIsCutsceneActionComplete(GameState& state)
{
    if (state.mode != GameMode::Cutscene) {
        return true;
    }

    return !CutsceneHasActiveAction(state);
}
