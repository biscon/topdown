#pragma once

#include "data/GameState.h"

bool TopdownLoadLevelScript(GameState& state);
bool TopdownRunLevelEnterHook(GameState& state);
bool TopdownRunLevelExitHook(GameState& state);
