#pragma once

#include "data/GameState.h"
#include "topdown/TopdownData.h"

void TopdownUpdateProps(GameState& state, float dt);
void TopdownRenderProps(GameState& state, TopdownEffectPlacement placement);
TopdownRuntimeProp* FindProp(GameState& state, const std::string& id);
const TopdownRuntimeProp* FindProp(const GameState& state, const std::string& id);
