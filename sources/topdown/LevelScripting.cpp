#include "topdown/LevelScripting.h"

#include <filesystem>

#include "scripting/ScriptSystem.h"
#include "raylib.h"

bool TopdownLoadLevelScript(GameState& state)
{
    if (state.script.vm == nullptr) {
        TraceLog(LOG_ERROR, "TopdownLoadLevelScript: script VM not initialized");
        return false;
    }

    if (state.topdown.currentLevelScriptFilePath.empty()) {
        TraceLog(LOG_WARNING, "TopdownLoadLevelScript: no script path set for current level");
        return true;
    }

    if (!std::filesystem::exists(state.topdown.currentLevelScriptFilePath)) {
        TraceLog(LOG_INFO,
                 "Topdown level has no script file: %s",
                 state.topdown.currentLevelScriptFilePath.c_str());
        return true;
    }

    if (!ScriptSystemRunFile(state.script, state.topdown.currentLevelScriptFilePath)) {
        TraceLog(LOG_ERROR,
                 "Failed loading topdown level script: %s",
                 state.topdown.currentLevelScriptFilePath.c_str());
        return false;
    }

    TraceLog(LOG_INFO,
             "Loaded topdown level script: %s",
             state.topdown.currentLevelScriptFilePath.c_str());
    return true;
}

bool TopdownRunLevelEnterHook(GameState& state)
{
    return ScriptSystemCallHook(state, "Level_onEnter");
}

bool TopdownRunLevelExitHook(GameState& state)
{
    return ScriptSystemCallHook(state, "Level_onExit");
}
