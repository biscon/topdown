#include "scripting/ScriptWaitBridge.h"


bool ScriptIsWalkWaitComplete(GameState& state, const ScriptCoroutine& co)
{
    (void)co;
    if (state.mode == GameMode::TopDown) {
        return !state.topdown.runtime.scriptedMove.active;
    }
}

bool ScriptIsSpeechWaitComplete(GameState& state)
{
    if (state.mode == GameMode::TopDown) {
        return true;
    }
}

bool ScriptTryConsumeDialogueResult(GameState& state, std::string& outResult)
{
    outResult.clear();

    if (state.mode == GameMode::TopDown) {
        return false;
    }
    return true;
}
