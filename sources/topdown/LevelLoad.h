#pragma once

#include "data/GameState.h"

bool TopdownLoadLevel(GameState& state, const char* tiledFilePath, int baseAssetScale);
void TopdownUnloadLevel(GameState& state);
