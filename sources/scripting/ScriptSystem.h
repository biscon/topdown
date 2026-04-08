#pragma once

#include <string>
#include "ScriptData.h"
#include "data/GameState.h"

void ScriptSystemInit(GameState& state);
void ScriptSystemShutdown(ScriptData& script);
void ScriptSystemUpdate(GameState& state, float dt);

bool ScriptSystemRunFile(ScriptData& script, const std::string& filePath);


// For interaction triggers like Scene_use_furnace / Scene_look_window.
ScriptCallResult ScriptSystemCallTrigger(GameState& state, const std::string& functionName);
ScriptCallResult ScriptSystemCallBoolHook(GameState& state, const std::string& functionName, bool& outValue);
bool ScriptSystemCallHook(GameState& state, const std::string& functionName);


bool ScriptSystemStartFunction(ScriptData& script, const std::string& functionName);
bool ScriptSystemIsAnyCoroutineActive(const ScriptData& script);
bool ScriptSystemIsFunctionRunning(const ScriptData& script, const std::string& functionName);
void ScriptSystemStopFunction(ScriptData& script, const std::string& functionName);
void ScriptSystemStopAll(ScriptData& script);

void ScriptSystemBuildDebugEntries(const ScriptData& script,
                                   const GameState& state,
                                   std::vector<ScriptDebugEntry>& outEntries);
bool ScriptSystemExecuteConsoleLine(
        ScriptData& script,
        const std::string& line,
        std::string& outResult,
        std::string& outError);

void ScriptSystemConsolePrint(const std::string& text);