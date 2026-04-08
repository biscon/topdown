#pragma once

#include <string>
#include "data/GameState.h"

bool SaveGameToSlot(GameState& state, int slotIndex);
bool LoadGameFromSlot(GameState& state, int slotIndex);

bool DoesSaveSlotExist(int slotIndex);
std::string GetSaveSlotSummary(int slotIndex);
