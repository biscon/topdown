#include "cutscene/CutsceneMode.h"

#include "cutscene/CutsceneRegistry.h"

namespace
{
    static void ResetCutsceneRuntime(GameState& state)
    {
        state.cutscene.runtime = CutsceneRuntime{};
    }
}

bool CutsceneStart(GameState& state, const std::string& cutsceneId)
{
    const CutsceneDefinition* definition = FindCutsceneDefinitionById(state, cutsceneId);
    if (definition == nullptr) {
        TraceLog(LOG_WARNING, "Cannot start unknown cutscene: %s", cutsceneId.c_str());
        return false;
    }

    ResetCutsceneRuntime(state);

    CutsceneRuntime& runtime = state.cutscene.runtime;
    runtime.active = true;
    runtime.cutsceneId = definition->cutsceneId;
    runtime.baseAssetScale = definition->baseAssetScale;
    runtime.scriptPath = definition->scriptPath;
    runtime.skipLevelId = definition->skipLevelId;
    runtime.skipSpawnId = definition->skipSpawnId;

    state.mode = GameMode::Cutscene;

    return true;
}

void CutsceneStop(GameState& state)
{
    ResetCutsceneRuntime(state);
}

void CutsceneUpdate(GameState& state, float dt)
{
    (void)state;
    (void)dt;
}

void CutsceneHandleInput(GameState& state)
{
    (void)state;
}

void CutsceneRenderUi(GameState& state)
{
    (void)state;
}

bool CutsceneIsActive(const GameState& state)
{
    return state.cutscene.runtime.active;
}
