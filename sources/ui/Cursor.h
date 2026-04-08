#pragma once

#include "data/GameState.h"

void InitCursor(GameState& state);
void ShutdownCursor(GameState& state);

void UpdateCursor(GameState& state);
void RenderCursor(const GameState& state, float scale);
