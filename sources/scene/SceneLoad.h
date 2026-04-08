#pragma once

#include "data/GameState.h"

enum class SceneLoadMode {
    Normal,
    FromSave
};

bool LoadSceneById(GameState& state, const char* sceneId, SceneLoadMode loadMode = SceneLoadMode::Normal);
void UnloadCurrentScene(GameState& state);
