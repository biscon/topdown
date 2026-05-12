#pragma once

#include <string>

#include "data/GameState.h"

bool TopdownPrepareLoadScreenOverlay(
        GameState& state,
        const std::string& texturePath,
        int baseAssetScale,
        bool presentImmediately);
void TopdownClearLoadScreenOverlay(GameState& state);
void TopdownUpdateLoadScreenOverlay(GameState& state, float dt);
bool TopdownLoadScreenBlocksLevelUpdate(const GameState& state);
bool TopdownLoadScreenConsumeInput(GameState& state);
void TopdownRenderLoadScreenOverlay(GameState& state);
