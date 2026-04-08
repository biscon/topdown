#pragma once

#include <string>
#include "data/GameState.h"

bool EnsureActorDefinitionLoaded(GameState& state, const std::string& actorId, int* outDefIndex = nullptr);
