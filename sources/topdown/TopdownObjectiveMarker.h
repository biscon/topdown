#pragma once

#include "data/GameState.h"

bool LoadTopdownObjectiveMarker(GameState& state);
void UnloadTopdownObjectiveMarker(GameState& state);
void TopdownUpdateObjectiveMarker(GameState& state, float dt);
void TopdownRenderObjectiveMarker(GameState& state);
