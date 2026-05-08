#pragma once

#include "data/GameState.h"

void MenuInit(GameState* gameState);
void MenuRefreshRoot();
void MenuUpdate(float dt);
void MenuRenderUi(GameState& state);
void MenuRenderOverlay();
void MenuHandleInput(GameState& state);