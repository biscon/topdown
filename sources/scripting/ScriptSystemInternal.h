#pragma once

#include <string>
#include "data/GameState.h"
#include "scripting/ScriptData.h"
#include "scripting/ScriptSystem.h"
#include "lua/lua.hpp"

extern GameState* gameState;

ScriptCoroutine* FindCoroutineByThread(ScriptData& script, lua_State* thread);
ScriptCoroutine* FindCoroutineByFunctionName(ScriptData& script, const std::string& functionName);
const ScriptCoroutine* FindCoroutineByFunctionName(const ScriptData& script, const std::string& functionName);

void ResetScriptCoroutine(ScriptData& script, ScriptCoroutine& co);
void LogLuaError(lua_State* L, const char* prefix);

bool StartPendingWait(lua_State* L, ScriptWaitType waitType, float remainingMs);
bool StartPendingActorWalkWait(lua_State* L, int actorIndex);
bool StartPendingDialogueWait(lua_State* L);

ScriptCallResult StartLuaManagedFunction(
        GameState& state,
        const std::string& functionName,
        bool foreground,
        bool* outImmediateBool);

void RegisterLuaAPI(lua_State* L);
void RegisterLuaTalkColorGlobals(lua_State* L);
int Lua_consolePrint(lua_State* L);