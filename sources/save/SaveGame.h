#pragma once

#include <string>
#include "data/GameState.h"

bool SaveGameToSlot(GameState& state, int slotIndex);
bool LoadGameFromSlot(GameState& state, int slotIndex);
bool CanSaveGame(const GameState& state, std::string* outReason = nullptr);

bool DoesSaveSlotExist(int slotIndex);
std::string GetSaveSlotSummary(int slotIndex);
