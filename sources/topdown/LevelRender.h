#pragma once

#include "data/GameState.h"

void TopdownRenderWorld(GameState& state, RenderTexture2D& worldTarget, RenderTexture2D& tempTarget);
void TopdownRenderUi(GameState& state);
void TopdownRenderDebug(GameState& state);
void TopdownRenderNpcs(GameState& state);