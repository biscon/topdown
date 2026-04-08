#pragma once

#include <string>
#include "data/GameState.h"

const ItemDefinitionData* FindItemDefinitionById(const GameState& state, const std::string& itemId);

ActorInventoryData* FindActorInventoryByActorId(GameState& state, const std::string& actorId);
const ActorInventoryData* FindActorInventoryByActorId(const GameState& state, const std::string& actorId);

ActorInventoryData* GetControlledActorInventory(GameState& state);
const ActorInventoryData* GetControlledActorInventory(const GameState& state);

bool ActorHasItem(const GameState& state, const std::string& actorId, const std::string& itemId);
bool ControlledActorHasItem(const GameState& state, const std::string& itemId);

bool GiveItemToActor(GameState& state, const std::string& actorId, const std::string& itemId);
bool RemoveItemFromActor(GameState& state, const std::string& actorId, const std::string& itemId);

bool GiveItem(GameState& state, const std::string& itemId);
bool RemoveItem(GameState& state, const std::string& itemId);

bool SetHeldItemForActor(GameState& state, const std::string& actorId, const std::string& itemId);
bool SetControlledActorHeldItem(GameState& state, const std::string& itemId);
void ClearHeldItemForActor(GameState& state, const std::string& actorId);
void ClearControlledActorHeldItem(GameState& state);

const std::string* GetHeldItemForActor(const GameState& state, const std::string& actorId);
const std::string* GetControlledActorHeldItem(const GameState& state);
void ShowInventoryPickupPopup(GameState& state, const std::string& itemId);