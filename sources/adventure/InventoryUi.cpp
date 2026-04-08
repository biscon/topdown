#include "adventure/InventoryUi.h"

#include <algorithm>
#include "adventure/Inventory.h"
#include "input/Input.h"
#include "raylib.h"
#include "scripting/ScriptSystem.h"
#include "resources/TextureAsset.h"
#include "Dialogue.h"
#include "audio/Audio.h"
#include "AdventureScriptCommands.h"

static constexpr float INVENTORY_TRIGGER_HEIGHT = 24.0f;
static constexpr float INVENTORY_PANEL_WIDTH = 1348.0f;
static constexpr float INVENTORY_PANEL_HEIGHT = 150.0f;
static constexpr float INVENTORY_OPEN_Y = 0.0f;
static constexpr float INVENTORY_CLOSED_Y = -150.0f;
static constexpr float INVENTORY_OPEN_SPEED = 10.0f;
static constexpr float INVENTORY_CLOSE_DELAY_MS = 500.0f;

static constexpr int INVENTORY_VISIBLE_SLOT_COUNT = 10;

static constexpr float INVENTORY_SIDE_BUTTON_WIDTH = 96.0f;
static constexpr float INVENTORY_SLOT_SIZE = 96.0f;
static constexpr float INVENTORY_SLOT_GAP = 12.0f;
static constexpr float INVENTORY_SIDE_PADDING = 26.0f;
static constexpr float INVENTORY_INNER_GAP = 18.0f;

static constexpr float INVENTORY_CONTENT_Y = 26.0f;

static float Clamp01(float t)
{
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

Rectangle GetInventoryPanelRect(const GameState& state)
{
    const float x = (static_cast<float>(INTERNAL_WIDTH) - INVENTORY_PANEL_WIDTH) * 0.5f;

    const float y =
            INVENTORY_CLOSED_Y +
            (INVENTORY_OPEN_Y - INVENTORY_CLOSED_Y) * Clamp01(state.adventure.inventoryUi.openAmount);

    return Rectangle{
            x,
            y,
            INVENTORY_PANEL_WIDTH,
            INVENTORY_PANEL_HEIGHT
    };
}

static Rectangle GetInventoryPrevPageRect(const GameState& state)
{
    const Rectangle panel = GetInventoryPanelRect(state);
    return Rectangle{
            panel.x + INVENTORY_SIDE_PADDING,
            panel.y + INVENTORY_CONTENT_Y,
            INVENTORY_SIDE_BUTTON_WIDTH,
            INVENTORY_SLOT_SIZE
    };
}

static Rectangle GetInventoryNextPageRect(const GameState& state)
{
    const Rectangle panel = GetInventoryPanelRect(state);
    return Rectangle{
            panel.x + panel.width - INVENTORY_SIDE_PADDING - INVENTORY_SIDE_BUTTON_WIDTH,
            panel.y + INVENTORY_CONTENT_Y,
            INVENTORY_SIDE_BUTTON_WIDTH,
            INVENTORY_SLOT_SIZE
    };
}

static Rectangle GetInventorySlotRect(const GameState& state, int visibleSlotIndex)
{
    const Rectangle prevRect = GetInventoryPrevPageRect(state);

    const float slotsStartX = prevRect.x + prevRect.width + INVENTORY_INNER_GAP;
    const float x =
            slotsStartX +
            static_cast<float>(visibleSlotIndex) * (INVENTORY_SLOT_SIZE + INVENTORY_SLOT_GAP);

    return Rectangle{
            x,
            GetInventoryPanelRect(state).y + INVENTORY_CONTENT_Y,
            INVENTORY_SLOT_SIZE,
            INVENTORY_SLOT_SIZE
    };
}

static bool CanInventoryPageBackward(const ActorInventoryData& inv)
{
    return inv.pageStartIndex > 0;
}

static bool CanInventoryPageForward(const ActorInventoryData& inv)
{
    return inv.pageStartIndex + INVENTORY_VISIBLE_SLOT_COUNT < static_cast<int>(inv.itemIds.size());
}

static void ClampInventoryPageStart(ActorInventoryData& inv)
{
    if (inv.pageStartIndex < 0) {
        inv.pageStartIndex = 0;
    }

    const int count = static_cast<int>(inv.itemIds.size());
    if (count <= 0) {
        inv.pageStartIndex = 0;
        return;
    }

    if (inv.pageStartIndex >= count) {
        inv.pageStartIndex = std::max(0, count - INVENTORY_VISIBLE_SLOT_COUNT);
    }
}

static const ItemDefinitionData* GetHoveredInventoryItem(
        const GameState& state,
        int& outVisibleSlotIndex,
        int& outItemIndex)
{
    outVisibleSlotIndex = -1;
    outItemIndex = -1;

    const ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr) {
        return nullptr;
    }

    const Vector2 mouse = GetMousePosition();

    for (int visibleSlotIndex = 0; visibleSlotIndex < INVENTORY_VISIBLE_SLOT_COUNT; ++visibleSlotIndex) {
        const int itemIndex = inv->pageStartIndex + visibleSlotIndex;
        if (itemIndex < 0 || itemIndex >= static_cast<int>(inv->itemIds.size())) {
            continue;
        }

        const Rectangle slotRect = GetInventorySlotRect(state, visibleSlotIndex);
        if (!CheckCollisionPointRec(mouse, slotRect)) {
            continue;
        }

        const ItemDefinitionData* itemDef = FindItemDefinitionById(state, inv->itemIds[itemIndex]);
        if (itemDef == nullptr) {
            continue;
        }

        outVisibleSlotIndex = visibleSlotIndex;
        outItemIndex = itemIndex;
        return itemDef;
    }

    return nullptr;
}

static bool IsInventoryOpenWanted(const GameState& state)
{
    if (IsDialogueUiActive(state)) {
        return false;
    }
    if (!state.adventure.controlsEnabled || state.adventure.fadeInputBlocked) {
        return false;
    }

    const Vector2 mouse = GetMousePosition();

    if (mouse.y <= INVENTORY_TRIGGER_HEIGHT) {
        return true;
    }

    const Rectangle panelRect = GetInventoryPanelRect(state);
    if (CheckCollisionPointRec(mouse, panelRect)) {
        return true;
    }

    return false;
}

static void UpdateInventoryOpenState(GameState& state, float dt)
{
    InventoryUiState& ui = state.adventure.inventoryUi;

    const bool wantsOpen = IsInventoryOpenWanted(state);
    ui.hoveringInventory = wantsOpen;

    if (wantsOpen) {
        ui.closeDelayRemainingMs = INVENTORY_CLOSE_DELAY_MS;
        ui.openAmount += dt * INVENTORY_OPEN_SPEED;
    } else {
        ui.closeDelayRemainingMs -= dt * 1000.0f;
        if (ui.closeDelayRemainingMs > 0.0f) {
            ui.openAmount += dt * INVENTORY_OPEN_SPEED;
        } else {
            ui.openAmount -= dt * INVENTORY_OPEN_SPEED;
        }
    }

    ui.openAmount = Clamp01(ui.openAmount);
    ui.open = ui.openAmount > 0.0f;
}

void UpdateInventoryHoverUi(GameState& state)
{
    InventoryUiState& ui = state.adventure.inventoryUi;
    ui.hoveredSlotIndex = -1;
    ui.hoveringPrevPage = false;
    ui.hoveringNextPage = false;

    if (IsDialogueUiActive(state)) {
        return;
    }

    if (!state.adventure.controlsEnabled || state.adventure.fadeInputBlocked) {
        return;
    }

    if (!ui.open) {
        return;
    }

    const Vector2 mouse = GetMousePosition();

    const Rectangle prevRect = GetInventoryPrevPageRect(state);
    const Rectangle nextRect = GetInventoryNextPageRect(state);

    const ActorInventoryData* inv = GetControlledActorInventory(state);

    ui.hoveringPrevPage =
            inv != nullptr &&
            CanInventoryPageBackward(*inv) &&
            CheckCollisionPointRec(mouse, prevRect);

    ui.hoveringNextPage =
            inv != nullptr &&
            CanInventoryPageForward(*inv) &&
            CheckCollisionPointRec(mouse, nextRect);

    int visibleSlotIndex = -1;
    int itemIndex = -1;
    const ItemDefinitionData* itemDef =
            GetHoveredInventoryItem(state, visibleSlotIndex, itemIndex);

    if (itemDef == nullptr) {
        return;
    }

    ui.hoveredSlotIndex = visibleSlotIndex;
    state.adventure.hoverUi.active = true;
    state.adventure.hoverUi.displayName = itemDef->displayName;
}

static void ExamineInventoryItem(GameState& state, const ItemDefinitionData& itemDef)
{
    const std::string functionName = "Scene_look_item_" + itemDef.itemId;

    bool handled = false;
    const ScriptCallResult result = ScriptSystemCallBoolHook(state, functionName, handled);

    if (result == ScriptCallResult::StartedAsync ||
        result == ScriptCallResult::ImmediateTrue) {
        return;
    }

    if (result == ScriptCallResult::ImmediateFalse) {
        return;
    }

    AdventureScriptSay(state, itemDef.lookText, -1);
}

static void HandleInventoryPanelClickSwallow(GameState& state, InputEvent& ev)
{
    if (ev.type != InputEventType::MouseClick) {
        return;
    }

    if (!state.adventure.inventoryUi.open) {
        return;
    }

    const Vector2 mouse = GetMousePosition();
    const Rectangle panelRect = GetInventoryPanelRect(state);

    if (CheckCollisionPointRec(mouse, panelRect)) {
        ConsumeEvent(ev);
    }
}

static void HandleInventoryPagingInput(GameState& state, InputEvent& ev)
{
    if (ev.type != InputEventType::MouseClick ||
        ev.mouse.button != MOUSE_BUTTON_LEFT) {
        return;
    }

    ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr) {
        return;
    }

    const Vector2 mouse = GetMousePosition();

    const Rectangle prevRect = GetInventoryPrevPageRect(state);
    const Rectangle nextRect = GetInventoryNextPageRect(state);

    if (CanInventoryPageBackward(*inv) && CheckCollisionPointRec(mouse, prevRect)) {
        inv->pageStartIndex -= INVENTORY_VISIBLE_SLOT_COUNT;
        ClampInventoryPageStart(*inv);
        PlaySoundById(state, "ui_click");
        ConsumeEvent(ev);
        return;
    }

    if (CanInventoryPageForward(*inv) && CheckCollisionPointRec(mouse, nextRect)) {
        inv->pageStartIndex += INVENTORY_VISIBLE_SLOT_COUNT;
        ClampInventoryPageStart(*inv);
        PlaySoundById(state, "ui_click");
        ConsumeEvent(ev);
        return;
    }
}

static void HandleInventoryItemInput(GameState& state, InputEvent& ev)
{
    ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr) {
        return;
    }

    int visibleSlotIndex = -1;
    int itemIndex = -1;
    const ItemDefinitionData* itemDef =
            GetHoveredInventoryItem(state, visibleSlotIndex, itemIndex);

    if (itemDef == nullptr) {
        return;
    }

    if (ev.type == InputEventType::MouseClick &&
        ev.mouse.button == MOUSE_BUTTON_LEFT) {
        if (inv->heldItemId.empty()) {
            inv->heldItemId = itemDef->itemId;
            PlaySoundById(state, "ui_click");
            ConsumeEvent(ev);
            return;
        }

        if (inv->heldItemId == itemDef->itemId) {
            PlaySoundById(state, "ui_click");
            ConsumeEvent(ev);
            return;
        }

        const std::string functionName =
                "Scene_use_item_" + inv->heldItemId + "_on_item_" + itemDef->itemId;

        bool success = false;
        const ScriptCallResult result = ScriptSystemCallBoolHook(state, functionName, success);

        if (result == ScriptCallResult::ImmediateTrue) {
            inv->heldItemId.clear();
        } else if (result == ScriptCallResult::StartedAsync) {
            // item-use scripts are treated as non-consuming unless they explicitly
            // remove the item or otherwise change inventory state themselves.
        } else if (result == ScriptCallResult::Missing) {
            AdventureScriptSay(state, "That won't work.", -1);
        } else if (result == ScriptCallResult::ImmediateFalse) {
            // handled failure, keep held item
        } else if (result == ScriptCallResult::Busy) {
            // keep held item
        } else if (result == ScriptCallResult::Error) {
            // keep held item
        }

        ConsumeEvent(ev);
        return;
    }

    if (ev.type == InputEventType::MouseClick &&
        ev.mouse.button == MOUSE_BUTTON_RIGHT) {
        ExamineInventoryItem(state, *itemDef);
        ConsumeEvent(ev);
        return;
    }
}

static void HandleInventoryCancelHeldItemInput(GameState& state, InputEvent& ev)
{
    ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr || inv->heldItemId.empty()) {
        return;
    }

    if (ev.type == InputEventType::MouseClick &&
        ev.mouse.button == MOUSE_BUTTON_RIGHT) {
        inv->heldItemId.clear();
        PlaySoundById(state, "ui_click");
        ConsumeEvent(ev);
    }
}

static void DrawHeldInventoryItemCursor(const GameState& state)
{
    const ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv == nullptr || inv->heldItemId.empty()) {
        return;
    }

    const ItemDefinitionData* itemDef = FindItemDefinitionById(state, inv->heldItemId);
    if (itemDef == nullptr || itemDef->iconTextureHandle < 0) {
        return;
    }

    const TextureResource* texRes =
            FindTextureResource(state.resources, itemDef->iconTextureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const Vector2 mouse = GetMousePosition();

    const float size = 72.0f;

    Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(texRes->texture.width),
            static_cast<float>(texRes->texture.height)
    };

    Rectangle dst{
            mouse.x - size * 0.5f,
            mouse.y - size * 0.5f,
            size,
            size
    };

    DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

void UpdateInventoryPickupPopup(GameState& state, float dt)
{
    InventoryPickupPopupState& popup = state.adventure.inventoryUi.pickupPopup;

    if (!popup.active) {
        if (!popup.queuedItemIds.empty()) {
            popup.active = true;
            popup.itemId = popup.queuedItemIds.front();
            popup.queuedItemIds.pop_front();
            popup.timerMs = 0.0f;
        }
        return;
    }

    popup.timerMs += dt * 1000.0f;

    const float totalMs = popup.fadeInMs + popup.holdMs + popup.fadeOutMs;
    if (popup.timerMs >= totalMs) {
        popup.active = false;
        popup.itemId.clear();
        popup.timerMs = 0.0f;

        if (!popup.queuedItemIds.empty()) {
            popup.active = true;
            popup.itemId = popup.queuedItemIds.front();
            popup.queuedItemIds.pop_front();
            popup.timerMs = 0.0f;
        }
    }
}

void UpdateInventoryUi(GameState& state, float dt)
{
    UpdateInventoryOpenState(state, dt);

    ActorInventoryData* inv = GetControlledActorInventory(state);
    if (inv != nullptr) {
        ClampInventoryPageStart(*inv);
    }

    if (IsDialogueUiActive(state)) {
        return;
    }

    if (!state.adventure.controlsEnabled || state.adventure.fadeInputBlocked) {
        return;
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
        HandleInventoryCancelHeldItemInput(state, ev);
        if (ev.handled) {
            continue;
        }

        if (!state.adventure.inventoryUi.open) {
            continue;
        }

        HandleInventoryPagingInput(state, ev);
        if (ev.handled) {
            continue;
        }

        HandleInventoryItemInput(state, ev);
        if (ev.handled) {
            continue;
        }

        HandleInventoryPanelClickSwallow(state, ev);
    }
}

void RenderHeldInventoryItemCursor(const GameState& state)
{
    DrawHeldInventoryItemCursor(state);
}
