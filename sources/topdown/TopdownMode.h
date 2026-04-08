#pragma once

#include "data/GameState.h"

bool TopdownLoadLevel(GameState& state, const char* tiledFilePath, int baseAssetScale);
void TopdownUnloadLevel(GameState& state);

void TopdownHandleInput(GameState& state);
void TopdownUpdate(GameState& state, float dt);

void TopdownRenderWorld(GameState& state, RenderTexture2D& worldTarget, RenderTexture2D& tempTarget);
void TopdownRenderUi(GameState& state);
void TopdownRenderDebug(GameState& state);