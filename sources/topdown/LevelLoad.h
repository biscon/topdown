#pragma once

#include "data/GameState.h"
#include "topdown/TopdownData.h"

bool TopdownLoadLevel(GameState& state, const char* tiledFilePath, int baseAssetScale);
void TopdownUnloadLevel(GameState& state);
void TopdownRebuildWallOcclusionPolygons(TopdownData& topdown);
