#pragma once

#include "data/GameState.h"

bool TopdownScanLevelRegistry(GameState& state);
const TopdownLevelRegistryEntry* FindTopdownLevelRegistryEntryById(const GameState& state, const char* levelId);

bool TopdownLoadLevelById(GameState& state, const char* levelId);
bool TopdownLoadLevelById(GameState& state, const char* levelId, const char* spawnId);

bool TopdownReloadCurrentLevel(GameState& state);
bool TopdownHasActiveOrResumableLevel(const GameState& state);