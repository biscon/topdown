#pragma once

#include <string>
#include "data/GameState.h"
#include "scripting/ScriptData.h"

bool ScriptIsWalkWaitComplete(GameState& state, const ScriptCoroutine& co);
bool ScriptIsSpeechWaitComplete(GameState& state);
bool ScriptTryConsumeDialogueResult(GameState& state, std::string& outResult);