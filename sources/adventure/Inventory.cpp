#include "adventure/Inventory.h"

#include <algorithm>
#include "adventure/AdventureHelpers.h"
#include "audio/Audio.h"

const ItemDefinitionData* FindItemDefinitionById(const GameState& state, const std::string& itemId)
{
    for (const ItemDefinitionData& item : state.adventure.itemDefinitions) {
        if (item.itemId == itemId) {
            return &item;
        }
    }
    return nullptr;
}

ActorInventoryData* FindActorInventoryByActorId(GameState& state, const std::string& actorId)
{
    for (ActorInventoryData& inv : state.adventure.actorInventories) {
        if (inv.actorId == actorId) {
            return &inv;
        }
    }
    return nullptr;
}

const ActorInventoryData* FindActorInventoryByActorId(const GameState& state, const std::string& actorId)
{
    for (const ActorInventoryData& inv : state.adventure.actorInventories) {
        if (inv.actorId == actorId) {
            return &inv;
        }
    }
    return nullptr;
}

ActorInventoryData* GetControlledActorInventory(GameState& state)
{
    const ActorInstance* actor = GetControlledActor(state);
    if (actor == nullptr || actor->actorId.empty()) {
        return nullptr;
    }

    return FindActorInventoryByActorId(state, actor->actorId);
}

const ActorInventoryData* GetControlledActorInventory(const GameState& state)
{
    const ActorInstance* actor = GetControlledActor(state);
    if (actor == nullptr || actor->actorId.empty()) {
        return nullptr;
    }

    return FindActorInventoryByActorId(state, actor->actorId);
}

bool ActorHasItem(const GameState& state, const std::string& actorId, const std::string& itemId)
{
    const ActorInventoryData* inv = FindActorInventoryByActorId(state, actorId);
    if (inv == nullptr) {
        return false;
    }

    return std::find(inv->itemIds.begin(), inv->itemIds.end(), itemId) != inv->itemIds.end();
}

bool ControlledActorHasItem(const GameState& state, const std::string& itemId)
{
    const ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr) {
        return false;
    }

    return std::find(inv->itemIds.begin(), inv->itemIds.end(), itemId) != inv->itemIds.end();
}

bool GiveItemToActor(GameState& state, const std::string& actorId, const std::string& itemId)
{
    if (actorId.empty() || itemId.empty()) {
        return false;
    }

    if (FindItemDefinitionById(state, itemId) == nullptr) {
        return false;
    }

    ActorInventoryData* inv = FindActorInventoryByActorId(state, actorId);
    if (inv == nullptr) {
        return false;
    }

    if (std::find(inv->itemIds.begin(), inv->itemIds.end(), itemId) != inv->itemIds.end()) {
        return true;
    }

    inv->itemIds.push_back(itemId);
    ShowInventoryPickupPopup(state, itemId);
    PlaySoundById(state, "item_added");
    return true;
}

bool RemoveItemFromActor(GameState& state, const std::string& actorId, const std::string& itemId)
{
    if (actorId.empty() || itemId.empty()) {
        return false;
    }

    ActorInventoryData* inv = FindActorInventoryByActorId(state, actorId);
    if (inv == nullptr) {
        return false;
    }

    auto it = std::find(inv->itemIds.begin(), inv->itemIds.end(), itemId);
    if (it == inv->itemIds.end()) {
        return false;
    }

    inv->itemIds.erase(it);

    if (inv->heldItemId == itemId) {
        inv->heldItemId.clear();
    }

    if (inv->pageStartIndex < 0) {
        inv->pageStartIndex = 0;
    }
    if (inv->pageStartIndex > static_cast<int>(inv->itemIds.size())) {
        inv->pageStartIndex = static_cast<int>(inv->itemIds.size());
    }

    return true;
}

bool GiveItem(GameState& state, const std::string& itemId)
{
    const ActorInstance* actor = GetControlledActor(state);
    if (actor == nullptr || actor->actorId.empty()) {
        return false;
    }

    return GiveItemToActor(state, actor->actorId, itemId);
}

bool RemoveItem(GameState& state, const std::string& itemId)
{
    const ActorInstance* actor = GetControlledActor(state);
    if (actor == nullptr || actor->actorId.empty()) {
        return false;
    }

    return RemoveItemFromActor(state, actor->actorId, itemId);
}

bool SetHeldItemForActor(GameState& state, const std::string& actorId, const std::string& itemId)
{
    if (actorId.empty()) {
        return false;
    }

    ActorInventoryData* inv = FindActorInventoryByActorId(state, actorId);
    if (inv == nullptr) {
        return false;
    }

    if (!itemId.empty() &&
        std::find(inv->itemIds.begin(), inv->itemIds.end(), itemId) == inv->itemIds.end()) {
        return false;
    }

    inv->heldItemId = itemId;
    return true;
}

bool SetControlledActorHeldItem(GameState& state, const std::string& itemId)
{
    const ActorInstance* actor = GetControlledActor(state);
    if (actor == nullptr || actor->actorId.empty()) {
        return false;
    }

    return SetHeldItemForActor(state, actor->actorId, itemId);
}

void ClearHeldItemForActor(GameState& state, const std::string& actorId)
{
    ActorInventoryData* inv = FindActorInventoryByActorId(state, actorId);
    if (inv != nullptr) {
        inv->heldItemId.clear();
    }
}

void ClearControlledActorHeldItem(GameState& state)
{
    ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv != nullptr) {
        inv->heldItemId.clear();
    }
}

const std::string* GetHeldItemForActor(const GameState& state, const std::string& actorId)
{
    const ActorInventoryData* inv = FindActorInventoryByActorId(state, actorId);
    if (inv == nullptr || inv->heldItemId.empty()) {
        return nullptr;
    }

    return &inv->heldItemId;
}

const std::string* GetControlledActorHeldItem(const GameState& state)
{
    const ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr || inv->heldItemId.empty()) {
        return nullptr;
    }

    return &inv->heldItemId;
}

void ShowInventoryPickupPopup(GameState& state, const std::string& itemId)
{
    if (itemId.empty()) {
        return;
    }

    InventoryPickupPopupState& popup = state.adventure.inventoryUi.pickupPopup;

    if (!popup.active && popup.itemId.empty()) {
        popup.active = true;
        popup.itemId = itemId;
        popup.timerMs = 0.0f;
        return;
    }

    popup.queuedItemIds.push_back(itemId);
}
