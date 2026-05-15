#pragma once

#include <string>

#include "data/GameState.h"

bool CutsceneScanRegistry(GameState& state);
const CutsceneDefinition* FindCutsceneDefinitionById(const GameState& state, const std::string& cutsceneId);
