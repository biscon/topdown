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

#if defined(__APPLE__)
#include "platform/MacFullscreenBridge.h"
#endif

static GameState* game = nullptr;

static constexpr Color MENU_BG_COLOR = Color{25, 25, 25, 255};
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

static std::stack<std::function<std::shared_ptr<Menu>()>> menuStack;

static constexpr float MENU_TITLE_Y = 105.0f;
static constexpr float MENU_CENTER_X = INTERNAL_WIDTH * 0.5f;
static constexpr float MENU_CENTER_Y = INTERNAL_HEIGHT * 0.5f;

static constexpr float MENU_ITEM_SPACING = 40.0f;
static constexpr float MENU_ITEM_HEIGHT = 36.0f;
static constexpr float MENU_MIN_ITEM_WIDTH = 560.0f;
static constexpr float MENU_ITEM_SIDE_PADDING = 12.0f;

static constexpr float SLIDER_TRACK_HEIGHT = 6.0f;
static constexpr float SLIDER_KNOB_WIDTH = 10.0f;
static constexpr float SLIDER_KNOB_EXTRA_HEIGHT = 4.0f;
static constexpr float SLIDER_LABEL_WIDTH = 170.0f;
static constexpr float SLIDER_VALUE_WIDTH = 56.0f;
static constexpr float SLIDER_INNER_GAP = 12.0f;
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
    const float y = MENU_CENTER_Y + (index - menu.items.size() * 0.5f) * MENU_ITEM_SPACING;

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
        const int textWidth = MeasureText(item.text.c_str(), 20);
        if (static_cast<float>(textWidth) > itemWidth) {
            itemWidth = static_cast<float>(textWidth);
        }
    }

    itemWidth += 120.0f;
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

static void ReturnToMainMenuRoot()
{
    while (!menuStack.empty()) {
        menuStack.pop();
    }
    menuStack.push(&createMainMenu);
}

static std::shared_ptr<Menu> createResolutionMenu()
{
    RefreshResolutions(game->settings);

    auto menu = std::make_shared<Menu>();
    menu->title = "Resolution";

    if (game->settings.displayMode == DisplayMode::Borderless) {
        menu->hint = "Borderless uses the desktop resolution.";

        MenuItem back;
        back.text = "Back";
        back.action = [] {
            if (!menuStack.empty()) {
                menuStack.pop();
            }
        };
        menu->items.push_back(back);

        return menu;
    }

    for (size_t i = 0; i < game->settings.availableResolutions.size(); ++i) {
        const Resolution& availRes = game->settings.availableResolutions[i];

        MenuItem item;
        const bool selected = static_cast<int>(i) == game->settings.selectedResolutionIndex;
        item.text =
                (selected ? "< " : "  ") +
                std::to_string(availRes.width) + " x " + std::to_string(availRes.height) +
                (selected ? " >" : "");
        item.color = selected ? WHITE : LIGHTGRAY;
        item.action = [i] {
            SettingsData& settings = game->settings;
            settings.selectedResolutionIndex = static_cast<int>(i);
            settings.needsApply = true;
            ApplySettings(settings);
            SaveSettings(settings);
        };

        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        if (!menuStack.empty()) {
            menuStack.pop();
        }
    };
    menu->items.push_back(back);

    return menu;
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
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        if (!menuStack.empty()) {
            menuStack.pop();
        }
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
        item.text = game->settings.displayMode == DisplayMode::Borderless
                    ? "Resolution (desktop controlled)"
                    : "Resolution";
        item.isSubmenu = true;
        item.submenuBuilder = createResolutionMenu;
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = "Display Mode";
        item.isSubmenu = true;
        item.submenuBuilder = createDisplayModeMenu;
        menu->items.push_back(item);
    }

#if defined(__APPLE__)
    {
        MenuItem item;
        item.text = IsMacNativeFullscreenActive()
                    ? "Exit Mac Fullscreen (Experimental)"
                    : "Enter Mac Fullscreen (Experimental)";
        item.action = [] {
            ToggleMacNativeFullscreen();
        };
        menu->items.push_back(item);
    }
#endif

    {
        MenuItem item;
        item.text = game->settings.vsync ? "Disable VSync (restart required)" : "Enable VSync (restart required)";
        item.action = [] {
            game->settings.vsync = !game->settings.vsync;
            SaveSettings(game->settings);
            ShowMenuToast("VSync change requires restart");
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
        };
        menu->items.push_back(item);
    }

    {
        MenuItem item;
        item.text = game->settings.showFPS ? "Hide FPS Counter" : "Show FPS Counter";
        item.action = [] {
            game->settings.showFPS = !game->settings.showFPS;
            SaveSettings(game->settings);
        };
        menu->items.push_back(item);
    }

    MenuItem back;
    back.text = "Back";
    back.action = [] {
        if (!menuStack.empty()) {
            menuStack.pop();
        }
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
        if (!menuStack.empty()) {
            menuStack.pop();
        }
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
        if (!menuStack.empty()) {
            menuStack.pop();
        }
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
        item.enabled = false;
        item.action = [slot] {
            if (SaveGameToSlot(*game, slot)) {
                TraceLog(LOG_INFO, "Saved game to slot %d", slot);
                ShowMenuToast("Game Saved");
                ReturnToMainMenuRoot();
                game->mode = GameMode::Game;
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
        if (!menuStack.empty()) {
            menuStack.pop();
        }
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
        if (!menuStack.empty()) {
            menuStack.pop();
        }
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
            save.enabled = game->topdown.runtime.controlsEnabled;
            save.color = game->topdown.runtime.controlsEnabled ? LIGHTGRAY : DARKGRAY;
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
    menuStack = std::stack<std::function<std::shared_ptr<Menu>()>>();
    menuStack.push(&createMainMenu);
    gDraggingSliderIndex = -1;
    gSliderPreviewCooldown = 0.0f;
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

    std::shared_ptr<Menu> menu = menuStack.top()();
    if (!menu) {
        return;
    }

    const float itemWidth = ComputeMenuItemWidth(*menu);
    const Rectangle firstItemRect =
            menu->items.empty()
            ? Rectangle{MENU_CENTER_X - itemWidth * 0.5f, MENU_CENTER_Y, itemWidth, MENU_ITEM_HEIGHT}
            : GetMenuItemRect(*menu, itemWidth, 0);

    if (!menu->title.empty()) {
        DrawText(menu->title.c_str(),
                 static_cast<int>(MENU_CENTER_X - MeasureText(menu->title.c_str(), 40) * 0.5f),
                 static_cast<int>(MENU_TITLE_Y),
                 40,
                 WHITE);
    }

    if (!menu->hint.empty()) {
        DrawText(menu->hint.c_str(),
                 static_cast<int>(MENU_CENTER_X - MeasureText(menu->hint.c_str(), 30) * 0.5f),
                 static_cast<int>(firstItemRect.y - MENU_HINT_GAP_ABOVE_ITEMS),
                 30,
                 LIGHTGRAY);
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

            DrawRectangleRec(itemRect, hovered ? Fade(WHITE, 0.10f) : Fade(WHITE, 0.05f));
            DrawRectangleLinesEx(itemRect, 1.0f, hovered ? YELLOW : DARKGRAY);

            DrawText(item.text.c_str(),
                     static_cast<int>(itemRect.x + 10.0f),
                     static_cast<int>(itemRect.y + 8.0f),
                     20,
                     enabled ? (hovered ? YELLOW : item.color) : DARKGRAY);

            if (clicked && enabled) {
                PlaySoundById(state, "ui_click");

                if (item.isSubmenu && item.submenuBuilder) {
                    menuStack.push(item.submenuBuilder);
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

        DrawRectangleRec(itemRect, hovered ? Fade(WHITE, 0.06f) : Fade(WHITE, 0.035f));
        DrawRectangleLinesEx(itemRect, 1.0f, hovered ? YELLOW : DARKGRAY);

        DrawText(item.text.c_str(),
                 static_cast<int>(itemRect.x + MENU_ITEM_SIDE_PADDING),
                 static_cast<int>(itemRect.y + 8.0f),
                 20,
                 enabled ? (hovered ? YELLOW : item.color) : DARKGRAY);

        const float rawValue = item.getValue ? item.getValue() : item.sliderMin;
        const float normalized =
                Clamp01((rawValue - item.sliderMin) / (item.sliderMax - item.sliderMin));

        const float knobCenterX = trackRect.x + normalized * trackRect.width;

        DrawRectangle(static_cast<int>(trackRect.x),
                      static_cast<int>(trackRect.y),
                      static_cast<int>(trackRect.width),
                      static_cast<int>(trackRect.height),
                      DARKGRAY);

        DrawRectangle(static_cast<int>(knobCenterX - SLIDER_KNOB_WIDTH * 0.5f),
                      static_cast<int>(trackRect.y - SLIDER_KNOB_EXTRA_HEIGHT * 0.5f),
                      static_cast<int>(SLIDER_KNOB_WIDTH),
                      static_cast<int>(trackRect.height + SLIDER_KNOB_EXTRA_HEIGHT),
                      YELLOW);

        char valueBuf[32];
        std::snprintf(valueBuf, sizeof(valueBuf), "%.2f", rawValue);
        const int valueTextWidth = MeasureText(valueBuf, 20);

        DrawText(valueBuf,
                 static_cast<int>(valueRect.x + valueRect.width - valueTextWidth),
                 static_cast<int>(itemRect.y + 8.0f),
                 20,
                 WHITE);

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

    const int fontSize = 24;
    const int paddingX = 18;
    const int paddingY = 12;

    const int textWidth = MeasureText(menuToastText.c_str(), fontSize);
    const float boxWidth = static_cast<float>(textWidth + paddingX * 2);
    const float boxHeight = static_cast<float>(fontSize + paddingY * 2);

    const float x = static_cast<float>(INTERNAL_WIDTH) - boxWidth - 24.0f;
    const float y = 24.0f;

    const Color bg = Color{20, 20, 24, static_cast<unsigned char>(220.0f * alpha)};
    const Color border = Color{180, 180, 200, static_cast<unsigned char>(255.0f * alpha)};
    const Color textColor = Color{255, 255, 255, static_cast<unsigned char>(255.0f * alpha)};

    DrawRectangleRounded(Rectangle{x, y, boxWidth, boxHeight}, 0.15f, 6, bg);
    DrawRectangleRoundedLinesEx(Rectangle{x, y, boxWidth, boxHeight}, 0.15f, 6, 2.0f, border);
    DrawText(menuToastText.c_str(),
             static_cast<int>(x + paddingX),
             static_cast<int>(y + paddingY),
             fontSize,
             textColor);
}

void MenuHandleInput(GameState& state)
{
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if (ev.key.key == KEY_ESCAPE) {
            gDraggingSliderIndex = -1;
            gLastSliderPreviewValue = -9999.0f;

            if (menuStack.size() > 1) {
                menuStack.pop();
            } else {
                ResumeBestAvailableMode();
                TraceLog(LOG_DEBUG, "closing menu");
            }

            ConsumeEvent(ev);
        }
    }
}
