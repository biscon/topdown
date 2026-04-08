#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "lua/lua.hpp"

enum class ScriptWaitType
{
    None,
    WalkComplete,
    SpeechComplete,
    DelayMs,
    DialogueChoice
};

enum class ScriptCallResult
{
    Missing,
    ImmediateFalse,
    ImmediateTrue,
    StartedAsync,
    Busy,
    Error
};

struct ScriptCoroutine
{
    bool active = false;

    lua_State* thread = nullptr;
    int threadRegistryRef = LUA_NOREF;

    ScriptWaitType waitType = ScriptWaitType::None;
    float remainingMs = 0.0f;
    int waitActorIndex = -1;

    bool hasPendingResumeString = false;
    std::string pendingResumeString;

    bool finalReturnSet = false;
    bool finalReturnValue = false;

    std::string functionName;
    bool stopRequested = false;
    bool foreground = false;
};

struct ScriptCoroutineStartRequest
{
    std::string functionName;
};

struct ScriptDebugEntry {
    std::string functionName;
    std::string waitLabel;
};

struct ScriptData {
    lua_State* vm = nullptr;

    std::unordered_map<std::string, bool> flags;
    std::unordered_map<std::string, int> ints;
    std::unordered_map<std::string, std::string> strings;

    std::vector<ScriptCoroutine> coroutines;
    std::vector<ScriptCoroutineStartRequest> pendingStarts;
};
