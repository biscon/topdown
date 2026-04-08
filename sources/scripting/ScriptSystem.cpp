#include "ScriptSystem.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#include "scripting/ScriptSystemInternal.h"
#include "debug/DebugConsole.h"
#include "scripting/ScriptWaitBridge.h"

// -----------------------------------------------------------------------------
GameState* gameState = nullptr;

// -----------------------------------------------------------------------------

static const char* ScriptWaitTypeToLabel(ScriptWaitType waitType)
{
    switch (waitType) {
        case ScriptWaitType::WalkComplete:   return "WAIT_WALK";
        case ScriptWaitType::SpeechComplete: return "WAIT_SPEECH";
        case ScriptWaitType::DelayMs:        return "WAIT_DELAY";
        case ScriptWaitType::DialogueChoice: return "WAIT_DIALOGUE";
        case ScriptWaitType::None:
        default:                             return "RUNNING";
    }
}

static std::string BuildCoroutineWaitLabel(const ScriptCoroutine& co, const GameState& state)
{
    std::ostringstream out;
    out << ScriptWaitTypeToLabel(co.waitType);

    switch (co.waitType) {
        case ScriptWaitType::DelayMs:
            out << "  " << static_cast<int>(std::max(0.0f, co.remainingMs)) << "ms";
            break;

        case ScriptWaitType::WalkComplete:
            if (state.mode == GameMode::TopDown) {
                out << "  player";
            } else if (co.waitActorIndex >= 0) {
                out << "  actor#" << co.waitActorIndex;
            }
            break;

        case ScriptWaitType::SpeechComplete:
        case ScriptWaitType::DialogueChoice:
        case ScriptWaitType::None:
        default:
            break;
    }

    return out.str();
}

static bool IsAnyForegroundCoroutineRunning(const ScriptData& script)
{
    for (const ScriptCoroutine& co : script.coroutines) {
        if (co.active && co.foreground && !co.stopRequested) {
            return true;
        }
    }
    return false;
}

void ResetScriptCoroutine(ScriptData& script, ScriptCoroutine& co)
{
    if (script.vm != nullptr && co.threadRegistryRef != LUA_NOREF) {
        luaL_unref(script.vm, LUA_REGISTRYINDEX, co.threadRegistryRef);
    }

    co = {};
}

ScriptCoroutine* FindCoroutineByThread(ScriptData& script, lua_State* thread)
{
    for (ScriptCoroutine& co : script.coroutines) {
        if (co.active && co.thread == thread) {
            return &co;
        }
    }
    return nullptr;
}

ScriptCoroutine* FindCoroutineByFunctionName(ScriptData& script, const std::string& functionName)
{
    for (ScriptCoroutine& co : script.coroutines) {
        if (co.active && co.functionName == functionName) {
            return &co;
        }
    }
    return nullptr;
}

const ScriptCoroutine* FindCoroutineByFunctionName(const ScriptData& script, const std::string& functionName)
{
    for (const ScriptCoroutine& co : script.coroutines) {
        if (co.active && co.functionName == functionName) {
            return &co;
        }
    }
    return nullptr;
}

void LogLuaError(lua_State* L, const char* prefix)
{
    const char* msg = lua_tostring(L, -1);
    TraceLog(LOG_ERROR, "%s: %s", prefix, msg != nullptr ? msg : "<no message>");
    lua_pop(L, 1);
}

static std::string LuaValueToString(lua_State* L, int index)
{
    const int type = lua_type(L, index);

    switch (type) {
        case LUA_TNIL:
            return "nil";

        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";

        case LUA_TNUMBER:
        {
            if (lua_isinteger(L, index)) {
                return std::to_string(static_cast<long long>(lua_tointeger(L, index)));
            }

            std::ostringstream out;
            out << lua_tonumber(L, index);
            return out.str();
        }

        case LUA_TSTRING:
        {
            const char* s = lua_tostring(L, index);
            return (s != nullptr) ? std::string(s) : std::string();
        }

        case LUA_TTABLE:
            return "<table>";

        case LUA_TFUNCTION:
            return "<function>";

        case LUA_TTHREAD:
            return "<thread>";

        case LUA_TUSERDATA:
            return "<userdata>";

        case LUA_TLIGHTUSERDATA:
            return "<lightuserdata>";

        default:
            return std::string("<") + lua_typename(L, type) + ">";
    }
}

bool StartPendingWait(lua_State* L, ScriptWaitType waitType, float remainingMs)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return false;
    }

    ScriptData& script = gameState->script;
    ScriptCoroutine* co = FindCoroutineByThread(script, L);
    if (co == nullptr) {
        TraceLog(LOG_ERROR, "Lua wait registered without active coroutine owner");
        lua_pushboolean(L, 0);
        return false;
    }

    co->waitType = waitType;
    co->remainingMs = remainingMs;
    co->waitActorIndex = -1;

    lua_pushboolean(L, 1);
    return true;
}

bool StartPendingActorWalkWait(lua_State* L, int actorIndex)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return false;
    }

    ScriptData& script = gameState->script;
    ScriptCoroutine* co = FindCoroutineByThread(script, L);
    if (co == nullptr) {
        TraceLog(LOG_ERROR, "Lua actor walk wait registered without active coroutine owner");
        lua_pushboolean(L, 0);
        return false;
    }

    co->waitType = ScriptWaitType::WalkComplete;
    co->remainingMs = 0.0f;
    co->waitActorIndex = actorIndex;

    lua_pushboolean(L, 1);
    return true;
}

bool StartPendingDialogueWait(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return false;
    }

    ScriptData& script = gameState->script;
    ScriptCoroutine* co = FindCoroutineByThread(script, L);
    if (co == nullptr) {
        TraceLog(LOG_ERROR, "Lua dialogue wait registered without active coroutine owner");
        lua_pushboolean(L, 0);
        return false;
    }

    co->waitType = ScriptWaitType::DialogueChoice;
    co->remainingMs = 0.0f;
    co->waitActorIndex = -1;
    co->hasPendingResumeString = false;
    co->pendingResumeString.clear();

    lua_pushboolean(L, 1);
    return true;
}

static bool ResumeManagedCoroutine(GameState& state, ScriptCoroutine& co)
{
    if (!co.active || co.thread == nullptr) {
        return false;
    }

    int argCount = 0;

    if (co.hasPendingResumeString) {
        lua_pushstring(co.thread, co.pendingResumeString.c_str());
        argCount = 1;
        co.hasPendingResumeString = false;
        co.pendingResumeString.clear();
    }

    co.waitType = ScriptWaitType::None;
    co.remainingMs = 0.0f;
    co.waitActorIndex = -1;

#if LUA_VERSION_NUM >= 504
    int resultCount = 0;
    const int resumeResult = lua_resume(co.thread, nullptr, argCount, &resultCount);
#else
    const int resumeResult = lua_resume(co.thread, nullptr, argCount);
#endif

    if (resumeResult == LUA_YIELD) {
        return true;
    }

    if (resumeResult == LUA_OK) {
        co.stopRequested = true;
        return true;
    }

    const char* err = lua_tostring(co.thread, -1);
    TraceLog(LOG_ERROR,
             "Lua coroutine resume failed for '%s': %s",
             co.functionName.c_str(),
             err != nullptr ? err : "<unknown>");
    if (err != nullptr) {
        lua_pop(co.thread, 1);
    }

    co.stopRequested = true;
    return false;
}

ScriptCallResult StartLuaManagedFunction(
        GameState& state,
        const std::string& functionName,
        bool foreground,
        bool* outImmediateBool)
{
    ScriptData& script = state.script;

    if (outImmediateBool != nullptr) {
        *outImmediateBool = false;
    }

    if (script.vm == nullptr) {
        return ScriptCallResult::Error;
    }

    if (foreground && IsAnyForegroundCoroutineRunning(script)) {
        TraceLog(LOG_WARNING, "Script busy, cannot start foreground hook: %s", functionName.c_str());
        return ScriptCallResult::Busy;
    }

    lua_getglobal(script.vm, functionName.c_str());
    if (!lua_isfunction(script.vm, -1)) {
        lua_pop(script.vm, 1);
        return ScriptCallResult::Missing;
    }
    lua_pop(script.vm, 1);

    lua_State* thread = lua_newthread(script.vm);
    const int threadRef = luaL_ref(script.vm, LUA_REGISTRYINDEX);

    lua_getglobal(script.vm, functionName.c_str());
    lua_xmove(script.vm, thread, 1);

    ScriptCoroutine co{};
    co.active = true;
    co.thread = thread;
    co.threadRegistryRef = threadRef;
    co.functionName = functionName;
    co.foreground = foreground;

    // IMPORTANT: register before first resume so delay()/say()/walkTo()
    // can find their owning coroutine during initial execution.
    script.coroutines.push_back(co);
    ScriptCoroutine& storedCo = script.coroutines.back();

#if LUA_VERSION_NUM >= 504
    int resultCount = 0;
    const int resumeResult = lua_resume(thread, nullptr, 0, &resultCount);
#else
    const int resumeResult = lua_resume(thread, nullptr, 0);
#endif

    if (resumeResult == LUA_YIELD) {
        return ScriptCallResult::StartedAsync;
    }

    if (resumeResult == LUA_OK) {
        bool boolResult = false;
        bool hasBoolResult = false;

        if (lua_gettop(thread) >= 1 && lua_isboolean(thread, -1)) {
            boolResult = lua_toboolean(thread, -1) != 0;
            hasBoolResult = true;
        }

        ResetScriptCoroutine(script, storedCo);
        script.coroutines.erase(
                std::remove_if(
                        script.coroutines.begin(),
                        script.coroutines.end(),
                        [](const ScriptCoroutine& c) { return !c.active; }),
                script.coroutines.end());

        if (hasBoolResult) {
            if (outImmediateBool != nullptr) {
                *outImmediateBool = boolResult;
            }
            return boolResult ? ScriptCallResult::ImmediateTrue
                              : ScriptCallResult::ImmediateFalse;
        }

        return ScriptCallResult::ImmediateFalse;
    }

    const char* err = lua_tostring(thread, -1);
    TraceLog(LOG_ERROR,
             "Lua coroutine start failed for '%s': %s",
             functionName.c_str(),
             err != nullptr ? err : "<unknown>");
    if (err != nullptr) {
        lua_pop(thread, 1);
    }

    ResetScriptCoroutine(script, storedCo);
    script.coroutines.erase(
            std::remove_if(
                    script.coroutines.begin(),
                    script.coroutines.end(),
                    [](const ScriptCoroutine& c) { return !c.active; }),
            script.coroutines.end());

    return ScriptCallResult::Error;
}

void ScriptSystemConsolePrint(const std::string& text)
{
    if (gameState == nullptr) {
        return;
    }

    DebugConsoleAddLine(*gameState, text, LIGHTGRAY);
}

bool ScriptSystemExecuteConsoleLine(
        ScriptData& script,
        const std::string& line,
        std::string& outResult,
        std::string& outError)
{
    outResult.clear();
    outError.clear();

    if (script.vm == nullptr) {
        outError = "Lua VM not initialized.";
        return false;
    }

    const std::string returnChunk = "return " + line;
    int loadResult = luaL_loadbuffer(
            script.vm,
            returnChunk.c_str(),
            returnChunk.size(),
            "console");

    if (loadResult == LUA_OK) {
        const int callResult = lua_pcall(script.vm, 0, LUA_MULTRET, 0);
        if (callResult != LUA_OK) {
            const char* err = lua_tostring(script.vm, -1);
            outError = (err != nullptr) ? err : "<unknown Lua error>";
            lua_pop(script.vm, 1);
            return false;
        }

        const int resultCount = lua_gettop(script.vm);
        if (resultCount > 0) {
            std::ostringstream out;
            for (int i = 1; i <= resultCount; ++i) {
                if (i > 1) {
                    out << "    ";
                }
                out << LuaValueToString(script.vm, i);
            }
            outResult = out.str();
        }

        lua_settop(script.vm, 0);
        return true;
    }

    lua_settop(script.vm, 0);

    loadResult = luaL_loadbuffer(
            script.vm,
            line.c_str(),
            line.size(),
            "console");

    if (loadResult != LUA_OK) {
        const char* err = lua_tostring(script.vm, -1);
        outError = (err != nullptr) ? err : "<compile error>";
        lua_pop(script.vm, 1);
        return false;
    }

    const int callResult = lua_pcall(script.vm, 0, LUA_MULTRET, 0);
    if (callResult != LUA_OK) {
        const char* err = lua_tostring(script.vm, -1);
        outError = (err != nullptr) ? err : "<runtime error>";
        lua_pop(script.vm, 1);
        return false;
    }

    const int resultCount = lua_gettop(script.vm);
    if (resultCount > 0) {
        std::ostringstream out;
        for (int i = 1; i <= resultCount; ++i) {
            if (i > 1) {
                out << "    ";
            }
            out << LuaValueToString(script.vm, i);
        }
        outResult = out.str();
    }

    lua_settop(script.vm, 0);
    return true;
}

static bool RegisterLuaHelpers(lua_State* L)
{
    const char* source = R"(
Adv = Adv or {}

function Adv.appendIf(t, condition, value)
    if condition then
        table.insert(t, value)
    end
    return t
end

function Adv.hiddenOptions(map)
    local out = {}

    if map == nil then
        return out
    end

    for optionId, shouldHide in pairs(map) do
        if shouldHide then
            table.insert(out, optionId)
        end
    end

    return out
end

function Adv.runConversation(choiceSetId, handlers, hiddenOptions)
    while true do
        local choice = dialogue(choiceSetId, hiddenOptions)
        if choice == nil then
            return nil
        end

        local handler = nil
        if handlers ~= nil then
            handler = handlers[choice]
        end

        if handler == nil then
            return choice
        end

        local result = handler(choice)

        if result == "exit" or result == "break" then
            return choice
        end
    end
end

function Adv.runConversationDynamic(choiceSetId, handlers, hiddenOptionsFn)
    while true do
        local hidden = nil

        if hiddenOptionsFn ~= nil then
            hidden = hiddenOptionsFn()
        end

        local choice = dialogue(choiceSetId, hidden)
        if choice == nil then
            return nil
        end

        local handler = nil
        if handlers ~= nil then
            handler = handlers[choice]
        end

        if handler == nil then
            return choice
        end

        local result = handler(choice)

        if result == "exit" or result == "break" then
            return choice
        end
    end
end
)";

    const int loadResult = luaL_loadbuffer(L, source, std::strlen(source), "builtin_helpers");
    if (loadResult != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        TraceLog(LOG_ERROR,
                 "Failed to compile built-in Lua helpers: %s",
                 err != nullptr ? err : "<unknown>");
        if (err != nullptr) {
            lua_pop(L, 1);
        }
        return false;
    }

    const int callResult = lua_pcall(L, 0, 0, 0);
    if (callResult != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        TraceLog(LOG_ERROR,
                 "Failed to register built-in Lua helpers: %s",
                 err != nullptr ? err : "<unknown>");
        if (err != nullptr) {
            lua_pop(L, 1);
        }
        return false;
    }

    return true;
}

static void ConfigureLuaRequire(GameState& state) {
    lua_State* L = state.script.vm;

// Add your asset script paths
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");

    std::string currentPath = lua_tostring(L, -1);
    lua_pop(L, 1);

    // Add paths
    std::string newPath =
            currentPath +
            ";" + ASSETS_PATH + "scripts/?.lua" +
            ";" + ASSETS_PATH + "scripts/?/init.lua" +
            ";" + ASSETS_PATH + "scenes/?.lua" +
            ";" + ASSETS_PATH + "scenes/?/?.lua";

    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");

    lua_pop(L, 1); // pop package
}

void ScriptSystemInit(GameState& state)
{
    gameState = &state;

    if (state.script.vm != nullptr) {
        ScriptSystemShutdown(state.script);
    }

    state.script.vm = luaL_newstate();

    if (state.script.vm == nullptr) {
        TraceLog(LOG_ERROR, "Failed to create Lua state");
        return;
    }

    state.script.coroutines.clear();
    state.script.pendingStarts.clear();

    luaL_openlibs(state.script.vm);
    ConfigureLuaRequire(state);

    RegisterLuaAPI(state.script.vm);
    RegisterLuaTalkColorGlobals(state.script.vm);
    if (!RegisterLuaHelpers(state.script.vm)) {
        TraceLog(LOG_ERROR, "Failed to install built-in Lua helpers");
    }
}

void ScriptSystemShutdown(ScriptData& script)
{
    if (script.vm) {
        for (ScriptCoroutine& co : script.coroutines) {
            ResetScriptCoroutine(script, co);
        }

        lua_close(script.vm);
        script.vm = nullptr;
    }

    script.coroutines.clear();
    script.pendingStarts.clear();
}
// -----------------------------------------------------------------------------

void ScriptSystemBuildDebugEntries(const ScriptData& script,
                                   const GameState& state,
                                   std::vector<ScriptDebugEntry>& outEntries)
{
    outEntries.clear();

    for (const ScriptCoroutine& co : script.coroutines) {
        if (!co.active || co.stopRequested) {
            continue;
        }

        ScriptDebugEntry entry;
        entry.functionName = co.functionName;
        entry.waitLabel = BuildCoroutineWaitLabel(co, state);
        outEntries.push_back(entry);
    }
}

ScriptCallResult ScriptSystemCallBoolHook(GameState& state, const std::string& functionName, bool& outValue)
{
    outValue = false;

    bool immediateBool = false;
    const ScriptCallResult result =
            StartLuaManagedFunction(state, functionName, true, &immediateBool);

    switch (result) {
        case ScriptCallResult::ImmediateTrue:
            outValue = true;
            return result;

        case ScriptCallResult::ImmediateFalse:
            outValue = false;
            return result;

        case ScriptCallResult::StartedAsync:
        case ScriptCallResult::Busy:
        case ScriptCallResult::Missing:
        case ScriptCallResult::Error:
        default:
            return result;
    }
}

bool ScriptSystemRunFile(ScriptData& script, const std::string& filePath)
{
    if (script.vm == nullptr) {
        return false;
    }

    std::ifstream in(filePath);
    if (!in.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open Lua script file: %s", filePath.c_str());
        return false;
    }

    std::stringstream buf;
    buf << in.rdbuf();
    const std::string source = buf.str();

    const int loadResult = luaL_loadbuffer(
            script.vm,
            source.c_str(),
            source.size(),
            filePath.c_str());

    if (loadResult != LUA_OK) {
        LogLuaError(script.vm, "Lua compile error");
        return false;
    }

    const int callResult = lua_pcall(script.vm, 0, 0, 0);
    if (callResult != LUA_OK) {
        LogLuaError(script.vm, "Lua runtime error");
        return false;
    }

    return true;
}

bool ScriptSystemStartFunction(ScriptData& script, const std::string& functionName)
{
    if (script.vm == nullptr) {
        return false;
    }

    script.pendingStarts.push_back({ functionName });
    return true;
}

bool ScriptSystemIsAnyCoroutineActive(const ScriptData& script)
{
    for (const ScriptCoroutine& co : script.coroutines) {
        if (co.active) {
            return true;
        }
    }
    return false;
}

bool ScriptSystemIsFunctionRunning(const ScriptData& script, const std::string& functionName)
{
    return FindCoroutineByFunctionName(script, functionName) != nullptr;
}

void ScriptSystemStopFunction(ScriptData& script, const std::string& functionName)
{
    ScriptCoroutine* co = FindCoroutineByFunctionName(script, functionName);
    if (co != nullptr) {
        co->stopRequested = true;
    }
}

void ScriptSystemStopAll(ScriptData& script)
{
    for (ScriptCoroutine& co : script.coroutines) {
        if (co.active) {
            co.stopRequested = true;
        }
    }
}

// -----------------------------------------------------------------------------

bool ScriptSystemCallHook(GameState& state, const std::string& functionName)
{
    bool ignoredValue = false;
    const ScriptCallResult result = ScriptSystemCallBoolHook(state, functionName, ignoredValue);

    switch (result) {
        case ScriptCallResult::Missing:
            return false;

        case ScriptCallResult::ImmediateFalse:
        case ScriptCallResult::ImmediateTrue:
        case ScriptCallResult::StartedAsync:
        case ScriptCallResult::Busy:
            return true;

        case ScriptCallResult::Error:
        default:
            return false;
    }
}

ScriptCallResult ScriptSystemCallTrigger(GameState& state, const std::string& functionName)
{
    bool ignoredValue = false;
    return ScriptSystemCallBoolHook(state, functionName, ignoredValue);
}

// -----------------------------------------------------------------------------
void ScriptSystemUpdate(GameState& state, float dt)
{
    ScriptData& script = state.script;

    if (script.vm == nullptr) {
        return;
    }

    // Expose current frame delta to Lua in seconds.
    lua_pushnumber(script.vm, static_cast<lua_Number>(dt));
    lua_setglobal(script.vm, "FrameDelta");

    if (!script.pendingStarts.empty()) {
        std::vector<ScriptCoroutineStartRequest> starts;
        starts.swap(script.pendingStarts);

        for (const ScriptCoroutineStartRequest& req : starts) {
            bool ignoredBool = false;
            StartLuaManagedFunction(state, req.functionName, false, &ignoredBool);
        }
    }

    bool dialogueResultConsumed = false;
    std::string sharedDialogueResult;

    for (ScriptCoroutine& co : script.coroutines) {
        if (!co.active || co.stopRequested) {
            continue;
        }

        bool shouldResume = false;

        switch (co.waitType) {
            case ScriptWaitType::WalkComplete:
                shouldResume = ScriptIsWalkWaitComplete(state, co);
                break;

            case ScriptWaitType::SpeechComplete:
                shouldResume = ScriptIsSpeechWaitComplete(state);
                break;

            case ScriptWaitType::DelayMs:
                co.remainingMs -= dt * 1000.0f;
                shouldResume = (co.remainingMs <= 0.0f);
                break;

            case ScriptWaitType::DialogueChoice:
                if (!dialogueResultConsumed &&
                    ScriptTryConsumeDialogueResult(state, sharedDialogueResult)) {
                    co.hasPendingResumeString = true;
                    co.pendingResumeString = sharedDialogueResult;
                    shouldResume = true;
                    dialogueResultConsumed = true;
                }
                break;

            case ScriptWaitType::None:
                break;
        }

        if (shouldResume) {
            ResumeManagedCoroutine(state, co);
        }
    }

    for (ScriptCoroutine& co : script.coroutines) {
        if (co.stopRequested && co.active) {
            ResetScriptCoroutine(script, co);
        }
    }

    script.coroutines.erase(
            std::remove_if(
                    script.coroutines.begin(),
                    script.coroutines.end(),
                    [](const ScriptCoroutine& co) { return !co.active; }),
            script.coroutines.end());
}
