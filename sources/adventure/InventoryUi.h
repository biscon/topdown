#pragma once

#include "data/GameState.h"

void UpdateInventoryUi(GameState& state, float dt);
Rectangle GetInventoryPanelRect(const GameState& state);
void RenderHeldInventoryItemCursor(const GameState& state);
void UpdateInventoryPickupPopup(GameState& state, float dt);
void UpdateInventoryHoverUi(GameState& state);