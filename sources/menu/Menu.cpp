#include "Menu.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "audio/Audio.h"
#include "input/Input.h"
#include "raylib.h"
#include "save/SaveGame.h"
#include "settings/Settings.h"
#include "topdown/LevelRegistry.h"

static GameState* game = nullptr;

static constexpr Color MENU_BG_COLOR = Color{34, 26, 20, 255};
static constexpr Color MENU_PANEL_COLOR = Color{52, 38, 28, 230};
static constexpr Color MENU_PANEL_HOVER_COLOR = Color{68, 50, 36, 240};
static constexpr Color MENU_PANEL_SELECTED_COLOR = Color{74, 54, 38, 245};
static constexpr Color MENU_BORDER_COLOR = Color{150, 110, 70, 255};
static constexpr Color MENU_BORDER_DIM_COLOR = Color{90, 64, 44, 255};
static constexpr Color MENU_ACCENT_COLOR = Color{196, 140, 70, 255};
static constexpr Color MENU_TITLE_TEXT_COLOR = Color{250, 232, 200, 255};
static constexpr Color MENU_BODY_TEXT_COLOR = Color{222, 200, 168, 255};
static constexpr Color MENU_DISABLED_TEXT_COLOR = Color{118, 96, 76, 255};
static constexpr Color MENU_SHADOW_COLOR = Color{12, 9, 7, 180};

static constexpr float MENU_TITLE_FONT_SIZE = 42.0f;
static constexpr float MENU_BODY_FONT_SIZE = 32.0f;
static constexpr float MENU_FONT_SPACING = 1.0f;

static constexpr int SAVE_SLOT_COUNT = 8;

static std::string menuToastText;
static float menuToastTimer = 0.0f;
static float menuToastDuration = 0.0f;

static int gDraggingSliderIndex = -1;
static float gSliderPreviewCooldown = 0.0f;
static float gLastSliderPreviewValue = -9999.0f;

static void ShowMenuToast(const std::string& text, float durationSeconds = 1.5f)
{
    menuToastText = text;
    menuToastDuration = durationSeconds;
    menuToastTimer = durationSeconds;
}

struct Menu;
static std::shared_ptr<Menu> createMainMenu();

using MenuBuilder = std::function<std::shared_ptr<Menu>()>;

struct MenuItem {
    std::string text;
    bool isSubmenu = false;
    bool isSlider = false;

    std::function<void()> action;
    std::function<float()> getValue;
    std::function<void(float)> setValue;

    float sliderMin = 0.0f;
    float sliderMax = 1.0f;

    MenuBuilder submenuBuilder = nullptr;
    Color color = LIGHTGRAY;
    bool enabled = true;
};

struct Menu {
    std::string title;
    std::string hint;
    std::vector<MenuItem> items;
    int selected = 0;
};

static std::stack<std::shared_ptr<Menu>> menuStack;

static constexpr float MENU_TITLE_Y = 105.0f;
static constexpr float MENU_CENTER_X = INTERNAL_WIDTH * 0.5f;
static constexpr float MENU_CENTER_Y = INTERNAL_HEIGHT * 0.5f;

static constexpr float MENU_ITEM_SPACING = 56.0f;
static constexpr float MENU_ITEM_HEIGHT = 48.0f;
static constexpr float MENU_MIN_ITEM_WIDTH = 760.0f;
static constexpr float MENU_ITEM_SIDE_PADDING = 18.0f;

static constexpr float SLIDER_TRACK_HEIGHT = 8.0f;
static constexpr float SLIDER_KNOB_WIDTH = 14.0f;
static constexpr float SLIDER_KNOB_EXTRA_HEIGHT = 12.0f;
static constexpr float SLIDER_LABEL_WIDTH = 240.0f;
static constexpr float SLIDER_VALUE_WIDTH = 78.0f;
static constexpr float SLIDER_INNER_GAP = 16.0f;
static constexpr float MENU_HINT_GAP_ABOVE_ITEMS = 64.0f;

static float Clamp01(float t)
{
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

static float ClampFloat(float v, float minValue, float maxValue)
{
    if (v < minValue) return minValue;
    if (v > maxValue) return maxValue;
    return v;
}

static Vector2 PixelSnap(Vector2 p)
{
    return Vector2{std::round(p.x), std::round(p.y)};
}

static Color WithAlpha(Color color, float alpha)
{
    alpha = ClampFloat(alpha, 0.0f, 1.0f);
    color.a = static_cast<unsigned char>(static_cast<float>(color.a) * alpha);
    return color;
}

static float GetMenuFirstItemY(const Menu& menu)
{
    const float desiredFirstY = MENU_CENTER_Y - static_cast<float>(menu.items.size()) * 0.5f * MENU_ITEM_SPACING;
    const float minFirstY = MENU_TITLE_Y + MENU_TITLE_FONT_SIZE +
            (menu.hint.empty() ? 42.0f : MENU_HINT_GAP_ABOVE_ITEMS);
    return desiredFirstY < minFirstY ? minFirstY : desiredFirstY;
}

static Color GetMenuItemTextColor(const MenuItem& item, bool hovered)
{
    if (!item.enabled) {
        return MENU_DISABLED_TEXT_COLOR;
    }

    if (hovered ||
        (item.color.r == WHITE.r && item.color.g == WHITE.g && item.color.b == WHITE.b)) {
        return MENU_ACCENT_COLOR;
    }

    return MENU_BODY_TEXT_COLOR;
}

static void DrawMenuText(Font font, const char* text, Vector2 position, float fontSize, Color color)
{
    DrawTextEx(font, text, PixelSnap(position), fontSize, MENU_FONT_SPACING, color);
}

static bool HasTopdownLoaded()
{
    return TopdownHasActiveOrResumableLevel(*game);
}

static bool HasAnythingToResume()
{
    return HasTopdownLoaded();
}

static void ResumeBestAvailableMode()
{
    if (HasTopdownLoaded()) {
        game->mode = GameMode::TopDown;
        TraceLog(LOG_DEBUG, "Resuming topdown");
        return;
    }

    game->mode = GameMode::Menu;
}

static void QueueTopdownLevelChange(const std::string& levelId, const std::string& spawnId = "")
{
    game->topdown.hasPendingLevelChange = true;
    game->topdown.pendingLevelId = levelId;
    game->topdown.pendingSpawnId = spawnId;
}

static void startNewGame()
{
    game->script.flags = {};
    QueueTopdownLevelChange("beach_house");
    TraceLog(LOG_DEBUG, "Menu starting new game. Queued first level.");
}

static Rectangle GetMenuItemRect(const Menu& menu, float itemWidth, int index)
{
    const float x = MENU_CENTER_X - itemWidth * 0.5f;
    const float y = GetMenuFirstItemY(menu) + static_cast<float>(index) * MENU_ITEM_SPACING;

    return Rectangle{
            x,
            y,
            itemWidth,
            MENU_ITEM_HEIGHT
    };
}

static float ComputeMenuItemWidth(const Menu& menu)
{
    float itemWidth = 0.0f;

    for (const MenuItem& item : menu.items) {
        const Vector2 textSize = MeasureTextEx(
                game->narrationBodyFont,
                item.text.c_str(),
                MENU_BODY_FONT_SIZE,
                MENU_FONT_SPACING);
        if (textSize.x > itemWidth) {
            itemWidth = textSize.x;
        }
    }

    itemWidth += 160.0f;
    if (itemWidth < MENU_MIN_ITEM_WIDTH) {
        itemWidth = MENU_MIN_ITEM_WIDTH;
    }

    return itemWidth;
}

static Rectangle GetSliderTrackRect(const Rectangle& itemRect)
{
    const float trackX =
            itemRect.x + MENU_ITEM_SIDE_PADDING + SLIDER_LABEL_WIDTH + SLIDER_INNER_GAP;

    const float trackWidth =
            itemRect.width -
            MENU_ITEM_SIDE_PADDING * 2.0f -
            SLIDER_LABEL_WIDTH -
            SLIDER_VALUE_WIDTH -
            SLIDER_INNER_GAP * 2.0f;

    const float trackY = itemRect.y + itemRect.height * 0.5f - SLIDER_TRACK_HEIGHT * 0.5f;

    return Rectangle{
            trackX,
            trackY,
            trackWidth,
            SLIDER_TRACK_HEIGHT
    };
}

static Rectangle GetSliderHitRect(const Rectangle& itemRect)
{
    const Rectangle track = GetSliderTrackRect(itemRect);
    return Rectangle{
            track.x,
            itemRect.y,
            track.width,
            itemRect.height
    };
}

static Rectangle GetSliderValueRect(const Rectangle& itemRect)
{
    const Rectangle track = GetSliderTrackRect(itemRect);
    return Rectangle{
            track.x + track.width + SLIDER_INNER_GAP,
            itemRect.y,
            SLIDER_VALUE_WIDTH,
            itemRect.height
    };
}

static void RebuildMainMenuRoot()
{
    while (!menuStack.empty()) {
        menuStack.pop();
    }

    menuStack.push(createMainMenu());
}

static void ReturnToMainMenuRoot()
{
    RebuildMainMenuRoot();
}

static void PopMenuStackOneLevel()
{
    if (menuStack.size() <= 1) {
        return;
    }

    menuStack.pop();

    if (menuStack.size() == 1) {
        RebuildMainMenuRoot();
    }
}

static void ReplaceCurrentMenu(MenuBuilder builder)
{
    if (menuStack.empty() || !builder) {
        return;
    }

    menuStack.pop();
    menuStack.push(builder());
}

static std::shared_ptr<Menu> createDisplayModeMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Display Mode";
    menu->hint = "Borderless is recommended.";

    {
        MenuItem item;
        item.text = game->settings.displayMode == DisplayMode::Windowed ? "< Windowed >" : "Windowed";
        item.color = game->settings.displayMode == DisplayMode::Windowed ? WHITE : LIGHTGRAY;
        item.action = [] {
            game->settings.displayMode = DisplayMode::Windowed;
            game->settings.needsApply = true;
            ApplySettings(game->settings);
            SaveSettings(game->settings);
            ReplaceCurrentMenu(createDisplayModeMenu);
        };
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = game->settings.displayMode == DisplayMode::Borderless ? "< Borderless >" : "Borderless";
        item.color = game->settings.displayMode == DisplayMode::Borderless ? WHITE : LIGHTGRAY;
        item.action = [] {
            game->settings.displayMode = DisplayMode::Borderless;
            game->settings.needsApply = true;
            ApplySettings(game->settings);
            SaveSettings(game->settings);
            ReplaceCurrentMenu(createDisplayModeMenu);
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        PopMenuStackOneLevel();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createGraphicsMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Graphics";
    menu->hint = "VSync changes require restart.";

    {
        MenuItem item;
        item.text = "Display Mode";
        item.isSubmenu = true;
        item.submenuBuilder = createDisplayModeMenu;
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = game->settings.vsync ? "Disable VSync (restart required)" : "Enable VSync (restart required)";
        item.action = [] {
            game->settings.vsync = !game->settings.vsync;
            SaveSettings(game->settings);
            ShowMenuToast("VSync change requires restart");
            ReplaceCurrentMenu(createGraphicsMenu);
        };
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = game->settings.fpsLock ? "Unlock FPS" : "Lock FPS (60)";
        item.action = [] {
            game->settings.fpsLock = !game->settings.fpsLock;
            ApplySettings(game->settings);
            SaveSettings(game->settings);
            ReplaceCurrentMenu(createGraphicsMenu);
        };
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = game->settings.showFPS ? "Hide FPS Counter" : "Show FPS Counter";
        item.action = [] {
            game->settings.showFPS = !game->settings.showFPS;
            SaveSettings(game->settings);
            ReplaceCurrentMenu(createGraphicsMenu);
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        PopMenuStackOneLevel();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createAudioMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Audio";
    menu->hint = "Adjust sound and music levels.";

    {
        MenuItem slider;
        slider.text = "Sound Volume";
        slider.isSlider = true;
        slider.sliderMin = 0.0f;
        slider.sliderMax = 1.0f;
        slider.getValue = []() { return game->settings.soundVolume; };
        slider.setValue = [](float v) {
            game->settings.soundVolume = v;
        };
        menu->items.push_back(slider);
    }

    {
        MenuItem slider;
        slider.text = "Music Volume";
        slider.isSlider = true;
        slider.sliderMin = 0.0f;
        slider.sliderMax = 1.0f;
        slider.getValue = []() { return game->settings.musicVolume; };
        slider.setValue = [](float v) {
            game->settings.musicVolume = v;
        };
        menu->items.push_back(slider);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        SaveSettings(game->settings);
        PopMenuStackOneLevel();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createSettingsMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Settings";

    {
        MenuItem item;
        item.text = "Graphics";
        item.isSubmenu = true;
        item.submenuBuilder = createGraphicsMenu;
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = "Audio";
        item.isSubmenu = true;
        item.submenuBuilder = createAudioMenu;
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        PopMenuStackOneLevel();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createSaveMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Save Game";
    menu->hint = "Select a slot to overwrite.";

    for (int slot = 1; slot <= SAVE_SLOT_COUNT; ++slot) {
        MenuItem item;
        item.text = "Slot " + std::to_string(slot) + " - " + GetSaveSlotSummary(slot);
        std::string reason;
        item.enabled = CanSaveGame(*game, &reason);
        item.color = item.enabled ? LIGHTGRAY : DARKGRAY;
        item.action = [slot] {
            if (SaveGameToSlot(*game, slot)) {
                TraceLog(LOG_INFO, "Saved game to slot %d", slot);
                ShowMenuToast("Game Saved");
                ReturnToMainMenuRoot();
                game->mode = GameMode::TopDown;
            } else {
                TraceLog(LOG_ERROR, "Failed saving game to slot %d", slot);
                ShowMenuToast("Save Failed");
            }
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        PopMenuStackOneLevel();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createLoadMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Load Game";
    menu->hint = "Select a save slot to load.";

    for (int slot = 1; slot <= SAVE_SLOT_COUNT; ++slot) {
        MenuItem item;
        item.text = "Slot " + std::to_string(slot) + " - " + GetSaveSlotSummary(slot);
        item.enabled = DoesSaveSlotExist(slot);
        item.action = [slot] {
            if (LoadGameFromSlot(*game, slot)) {
                TraceLog(LOG_INFO, "Loaded game from slot %d", slot);
                ShowMenuToast("Game Loaded");
                ReturnToMainMenuRoot();
            } else {
                TraceLog(LOG_ERROR, "Failed loading game from slot %d", slot);
                ShowMenuToast("Load Failed");
            }
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        PopMenuStackOneLevel();
    };
    menu->items.push_back(back);

    return menu;
}

static std::shared_ptr<Menu> createMainMenu()
{
    auto menu = std::make_shared<Menu>();
    menu->title = "Main Menu";

    const bool hasResume = HasAnythingToResume();

    if (!hasResume) {
        MenuItem item;
        item.text = "Start New Game";
        item.action = startNewGame;
        menu->items.push_back(item);
    } else {
        MenuItem resume;
        resume.text = "Resume";
        resume.action = [] {
            ResumeBestAvailableMode();
        };
        menu->items.push_back(resume);

        if (HasTopdownLoaded()) {
            MenuItem save;
            save.text = "Save Game";
            save.isSubmenu = true;
            save.submenuBuilder = createSaveMenu;
            std::string reason;
            save.enabled = CanSaveGame(*game, &reason);
            save.color = save.enabled ? LIGHTGRAY : DARKGRAY;
            menu->items.push_back(save);
        }
    }

    {
        MenuItem item;
        item.text = "Load Game";
        item.isSubmenu = true;
        item.submenuBuilder = createLoadMenu;
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = "Settings";
        item.isSubmenu = true;
        item.submenuBuilder = createSettingsMenu;
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = "Quit";
        item.action = [] {
            TraceLog(LOG_INFO, "main menu quit");
            game->mode = GameMode::Quit;
        };
        menu->items.push_back(item);
    }

    return menu;
}

void MenuInit(GameState* gameState)
{
    game = gameState;
    menuStack = std::stack<std::shared_ptr<Menu>>();
    RebuildMainMenuRoot();
    gDraggingSliderIndex = -1;
    gSliderPreviewCooldown = 0.0f;
    gLastSliderPreviewValue = -9999.0f;
}

void MenuRefreshRoot()
{
    if (game == nullptr) {
        return;
    }

    RebuildMainMenuRoot();

    gDraggingSliderIndex = -1;
    gLastSliderPreviewValue = -9999.0f;
}

void MenuUpdate(float dt)
{
    if (menuToastTimer > 0.0f) {
        menuToastTimer -= dt;
        if (menuToastTimer < 0.0f) {
            menuToastTimer = 0.0f;
            menuToastText.clear();
        }
    }

    if (gSliderPreviewCooldown > 0.0f) {
        gSliderPreviewCooldown -= dt;
        if (gSliderPreviewCooldown < 0.0f) {
            gSliderPreviewCooldown = 0.0f;
        }
    }
}

void MenuRenderUi(GameState& state)
{
    ClearBackground(MENU_BG_COLOR);
    if (menuStack.empty()) {
        return;
    }

    std::shared_ptr<Menu> menu = menuStack.top();
    if (!menu) {
        return;
    }

    const float itemWidth = ComputeMenuItemWidth(*menu);
    const Rectangle firstItemRect =
            menu->items.empty()
            ? Rectangle{MENU_CENTER_X - itemWidth * 0.5f, MENU_CENTER_Y, itemWidth, MENU_ITEM_HEIGHT}
            : GetMenuItemRect(*menu, itemWidth, 0);

    if (!menu->title.empty()) {
        const Vector2 titleSize = MeasureTextEx(
                state.narrationTitleFont,
                menu->title.c_str(),
                MENU_TITLE_FONT_SIZE,
                MENU_FONT_SPACING);
        DrawMenuText(state.narrationTitleFont,
                     menu->title.c_str(),
                     Vector2{MENU_CENTER_X - titleSize.x * 0.5f, MENU_TITLE_Y},
                     MENU_TITLE_FONT_SIZE,
                     MENU_TITLE_TEXT_COLOR);
    }

    if (!menu->hint.empty()) {
        const Vector2 hintSize = MeasureTextEx(
                state.narrationBodyFont,
                menu->hint.c_str(),
                MENU_BODY_FONT_SIZE,
                MENU_FONT_SPACING);
        DrawMenuText(state.narrationBodyFont,
                     menu->hint.c_str(),
                     Vector2{MENU_CENTER_X - hintSize.x * 0.5f,
                             firstItemRect.y - MENU_HINT_GAP_ABOVE_ITEMS},
                     MENU_BODY_FONT_SIZE,
                     MENU_BODY_TEXT_COLOR);
    }

    const Vector2 mouse = GetMousePosition();

    for (int i = 0; i < static_cast<int>(menu->items.size()); ++i) {
        MenuItem& item = menu->items[i];
        const bool enabled = item.enabled;

        const Rectangle itemRect = GetMenuItemRect(*menu, itemWidth, i);

        bool clicked = false;
        for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
            if (ev.mouse.button == MOUSE_LEFT_BUTTON &&
                CheckCollisionPointRec(ev.mouse.pos, itemRect)) {
                clicked = true;
                ConsumeEvent(ev);
            }
        }

        if (!item.isSlider) {
            const bool hovered = enabled && CheckCollisionPointRec(mouse, itemRect);
            const bool selected = item.color.r == WHITE.r && item.color.g == WHITE.g && item.color.b == WHITE.b;
            const Color panelColor = selected ? MENU_PANEL_SELECTED_COLOR :
                    (hovered ? MENU_PANEL_HOVER_COLOR : MENU_PANEL_COLOR);
            const Color borderColor = (hovered || selected) ? MENU_BORDER_COLOR : MENU_BORDER_DIM_COLOR;
            const Color textColor = GetMenuItemTextColor(item, hovered);
            const Vector2 textSize = MeasureTextEx(
                    state.narrationBodyFont,
                    item.text.c_str(),
                    MENU_BODY_FONT_SIZE,
                    MENU_FONT_SPACING);

            DrawRectangleRounded(
                    Rectangle{itemRect.x + 4.0f, itemRect.y + 4.0f, itemRect.width, itemRect.height},
                    0.18f,
                    8,
                    MENU_SHADOW_COLOR);
            DrawRectangleRounded(itemRect, 0.18f, 8, panelColor);
            DrawRectangleRoundedLinesEx(itemRect, 0.18f, 8, 1.5f, borderColor);

            DrawMenuText(state.narrationBodyFont,
                         item.text.c_str(),
                         Vector2{itemRect.x + MENU_ITEM_SIDE_PADDING,
                                 itemRect.y + (itemRect.height - textSize.y) * 0.5f},
                         MENU_BODY_FONT_SIZE,
                         textColor);

            if (clicked && enabled) {
                PlaySoundById(state, "ui_click");

                if (item.isSubmenu && item.submenuBuilder) {
                    menuStack.push(item.submenuBuilder());
                } else if (item.action) {
                    item.action();
                }
            }

            continue;
        }

        const Rectangle sliderHitRect = GetSliderHitRect(itemRect);
        const Rectangle trackRect = GetSliderTrackRect(itemRect);
        const Rectangle valueRect = GetSliderValueRect(itemRect);

        const bool hovered = enabled && CheckCollisionPointRec(mouse, sliderHitRect);

        DrawRectangleRounded(
                Rectangle{itemRect.x + 4.0f, itemRect.y + 4.0f, itemRect.width, itemRect.height},
                0.18f,
                8,
                MENU_SHADOW_COLOR);
        DrawRectangleRounded(itemRect, 0.18f, 8, hovered ? MENU_PANEL_HOVER_COLOR : MENU_PANEL_COLOR);
        DrawRectangleRoundedLinesEx(
                itemRect,
                0.18f,
                8,
                1.5f,
                hovered ? MENU_BORDER_COLOR : MENU_BORDER_DIM_COLOR);

        const Vector2 labelSize = MeasureTextEx(
                state.narrationBodyFont,
                item.text.c_str(),
                MENU_BODY_FONT_SIZE,
                MENU_FONT_SPACING);
        DrawMenuText(state.narrationBodyFont,
                     item.text.c_str(),
                     Vector2{itemRect.x + MENU_ITEM_SIDE_PADDING,
                             itemRect.y + (itemRect.height - labelSize.y) * 0.5f},
                     MENU_BODY_FONT_SIZE,
                     GetMenuItemTextColor(item, hovered));

        const float rawValue = item.getValue ? item.getValue() : item.sliderMin;
        const float normalized =
                Clamp01((rawValue - item.sliderMin) / (item.sliderMax - item.sliderMin));

        const float knobCenterX = trackRect.x + normalized * trackRect.width;

        DrawRectangleRounded(trackRect, 0.5f, 6, MENU_BORDER_DIM_COLOR);
        DrawRectangleRounded(
                Rectangle{trackRect.x, trackRect.y, knobCenterX - trackRect.x, trackRect.height},
                0.5f,
                6,
                MENU_ACCENT_COLOR);

        DrawRectangleRounded(
                Rectangle{knobCenterX - SLIDER_KNOB_WIDTH * 0.5f,
                          trackRect.y - SLIDER_KNOB_EXTRA_HEIGHT * 0.5f,
                          SLIDER_KNOB_WIDTH,
                          trackRect.height + SLIDER_KNOB_EXTRA_HEIGHT},
                0.4f,
                6,
                MENU_ACCENT_COLOR);

        char valueBuf[32];
        std::snprintf(valueBuf, sizeof(valueBuf), "%.2f", rawValue);
        const Vector2 valueTextSize = MeasureTextEx(
                state.narrationBodyFont,
                valueBuf,
                MENU_BODY_FONT_SIZE,
                MENU_FONT_SPACING);

        DrawMenuText(state.narrationBodyFont,
                     valueBuf,
                     Vector2{valueRect.x + valueRect.width - valueTextSize.x,
                             itemRect.y + (itemRect.height - valueTextSize.y) * 0.5f},
                     MENU_BODY_FONT_SIZE,
                     hovered ? MENU_ACCENT_COLOR : MENU_BODY_TEXT_COLOR);

        if (enabled && hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            gDraggingSliderIndex = i;
            gLastSliderPreviewValue = rawValue;
            PlaySoundById(state, "ui_click");
        }

        if (gDraggingSliderIndex == i) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float newNormalized = (mouse.x - trackRect.x) / trackRect.width;
                newNormalized = Clamp01(newNormalized);

                const float newValue =
                        item.sliderMin + newNormalized * (item.sliderMax - item.sliderMin);

                if (item.setValue) {
                    item.setValue(newValue);
                }

                if (std::fabs(newValue - gLastSliderPreviewValue) >= 0.02f &&
                    gSliderPreviewCooldown <= 0.0f) {
                    gLastSliderPreviewValue = newValue;
                    gSliderPreviewCooldown = 0.08f;
                    if (item.text == "Sound Volume") {
                        PlaySoundById(state, "ui_click");
                    }
                }
            } else {
                gDraggingSliderIndex = -1;
                gLastSliderPreviewValue = -9999.0f;
            }
        }
    }
}

void MenuRenderOverlay()
{
    if (menuToastTimer <= 0.0f || menuToastText.empty()) {
        return;
    }

    float alpha = 1.0f;
    if (menuToastDuration > 0.0f && menuToastTimer < 0.35f) {
        alpha = menuToastTimer / 0.35f;
        alpha = ClampFloat(alpha, 0.0f, 1.0f);
    }

    if (game == nullptr) {
        return;
    }

    const float paddingX = 24.0f;
    const float paddingY = 14.0f;
    const Vector2 textSize = MeasureTextEx(
            game->narrationBodyFont,
            menuToastText.c_str(),
            MENU_BODY_FONT_SIZE,
            MENU_FONT_SPACING);
    const float boxWidth = textSize.x + paddingX * 2.0f;
    const float boxHeight = textSize.y + paddingY * 2.0f;

    const float x = static_cast<float>(INTERNAL_WIDTH) * 0.5f - boxWidth * 0.5f;
    const float y = 24.0f;

    const Rectangle boxRect = Rectangle{x, y, boxWidth, boxHeight};
    const Color bg = WithAlpha(MENU_PANEL_COLOR, alpha);
    const Color border = WithAlpha(MENU_BORDER_COLOR, alpha);
    const Color textColor = WithAlpha(MENU_BODY_TEXT_COLOR, alpha);
    const Color shadow = WithAlpha(MENU_SHADOW_COLOR, alpha);

    DrawRectangleRounded(Rectangle{x + 4.0f, y + 4.0f, boxWidth, boxHeight}, 0.18f, 8, shadow);
    DrawRectangleRounded(boxRect, 0.18f, 8, bg);
    DrawRectangleRoundedLinesEx(boxRect, 0.18f, 8, 2.0f, border);
    DrawMenuText(game->narrationBodyFont,
                 menuToastText.c_str(),
                 Vector2{x + paddingX, y + paddingY},
                 MENU_BODY_FONT_SIZE,
                 textColor);
}

void MenuHandleInput(GameState& state)
{
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if (ev.key.key == KEY_ESCAPE) {
            gDraggingSliderIndex = -1;
            gLastSliderPreviewValue = -9999.0f;

            if (menuStack.size() > 1) {
                PopMenuStackOneLevel();
            } else {
                ResumeBestAvailableMode();
                TraceLog(LOG_DEBUG, "closing menu");
            }

            ConsumeEvent(ev);
        }
    }
}
