#pragma once

#include "data/GameState.h"

constexpr float kCorpseFadeDurationMs = 3000.0f;

void TopdownRenderPlayerCharacter(GameState& state);
void TopdownRenderNpcs(GameState& state);