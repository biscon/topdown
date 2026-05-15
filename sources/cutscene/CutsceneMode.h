#pragma once

#include <string>

#include "data/GameState.h"

bool CutsceneStart(GameState& state, const std::string& cutsceneId);
void CutsceneStop(GameState& state);
void CutsceneUpdate(GameState& state, float dt);
void CutsceneHandleInput(GameState& state);
void CutsceneRenderUi(GameState& state);
bool CutsceneIsActive(const GameState& state);
