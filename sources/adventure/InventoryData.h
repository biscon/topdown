#pragma once

#include <string>
#include <vector>
#include <deque>
#include "resources/ResourceData.h"

struct ItemDefinitionData {
    std::string itemId;
    std::string displayName;
    std::string lookText;

    std::string iconPath;
    TextureHandle iconTextureHandle = -1;
};

struct ActorInventoryData {
    std::string actorId;
    std::vector<std::string> itemIds;

    std::string heldItemId;
    int pageStartIndex = 0;
};

struct InventoryPickupPopupState {
    bool active = false;

    std::string itemId;
    float timerMs = 0.0f;

    float fadeInMs = 180.0f;
    float holdMs = 2000.0f;
    float fadeOutMs = 300.0f;

    std::deque<std::string> queuedItemIds;
};

struct InventoryUiState {
    bool open = false;
    float openAmount = 0.0f;

    float closeDelayRemainingMs = 0.0f;

    bool hoveringInventory = false;
    int hoveredSlotIndex = -1;

    bool hoveringPrevPage = false;
    bool hoveringNextPage = false;
    InventoryPickupPopupState pickupPopup{};
};
