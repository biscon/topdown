#pragma once

#include "data/GameState.h"

void AdventureQueueLoadScene(GameState& state, const char* sceneId, const char* spawnId = nullptr);
void AdventureProcessPendingLoads(GameState& state);
void AdventureUpdate(GameState& state, float dt);
