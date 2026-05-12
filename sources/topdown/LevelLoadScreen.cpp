#include "topdown/LevelLoadScreen.h"

#include <algorithm>
#include <cmath>

#include "input/Input.h"
#include "raylib.h"

static constexpr float LOAD_SCREEN_FADE_DURATION_MS = 650.0f;
static constexpr float LOAD_SCREEN_PROMPT_PULSE_SPEED = 0.0022f;
static constexpr float LOAD_SCREEN_PROMPT_BOTTOM_PADDING = 116.0f;
static constexpr float LOAD_SCREEN_TEXT_SPACING = 2.0f;
static constexpr Color LOAD_SCREEN_PROMPT_COLOR = Color{246, 232, 190, 255};
static constexpr Color LOAD_SCREEN_PROMPT_SHADOW_COLOR = Color{0, 0, 0, 230};

static Rectangle BuildPresentationRect(float internalW, float internalH, float screenW, float screenH)
{
    const float scale = std::min(screenW / internalW, screenH / internalH);
    const float w = internalW * scale;
    const float h = internalH * scale;
    return Rectangle{(screenW - w) * 0.5f, (screenH - h) * 0.5f, w, h};
}

static Rectangle GetLoadScreenInternalDestRect(const Texture2D& texture, int baseAssetScale)
{
    const float scale = static_cast<float>(std::max(1, baseAssetScale));
    const float width = static_cast<float>(texture.width) * scale;
    const float height = static_cast<float>(texture.height) * scale;
    return Rectangle{
            std::round((INTERNAL_WIDTH - width) * 0.5f),
            std::round((INTERNAL_HEIGHT - height) * 0.5f),
            std::round(width),
            std::round(height)};
}

static void DrawLoadScreenImageInternal(const TopdownLoadScreenOverlay& overlay, unsigned char alpha)
{
    if (!overlay.textureLoaded || overlay.texture.id == 0) {
        return;
    }

    const Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(overlay.texture.width),
            static_cast<float>(overlay.texture.height)};
    const Rectangle dst = GetLoadScreenInternalDestRect(overlay.texture, overlay.baseAssetScale);
    DrawTexturePro(overlay.texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, Color{255, 255, 255, alpha});
}

static void PresentLoadScreenOnce(const TopdownLoadScreenOverlay& overlay)
{
    if (!overlay.textureLoaded || overlay.texture.id == 0) {
        return;
    }

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const Rectangle presentation = BuildPresentationRect(
            static_cast<float>(INTERNAL_WIDTH),
            static_cast<float>(INTERNAL_HEIGHT),
            static_cast<float>(screenW),
            static_cast<float>(screenH));

    const float presentationScale = presentation.width / static_cast<float>(INTERNAL_WIDTH);
    const Rectangle internalDst = GetLoadScreenInternalDestRect(overlay.texture, overlay.baseAssetScale);
    const Rectangle screenDst{
            presentation.x + internalDst.x * presentationScale,
            presentation.y + internalDst.y * presentationScale,
            internalDst.width * presentationScale,
            internalDst.height * presentationScale};
    const Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(overlay.texture.width),
            static_cast<float>(overlay.texture.height)};

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(overlay.texture, src, screenDst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    EndDrawing();
}

bool TopdownPrepareLoadScreenOverlay(
        GameState& state,
        const std::string& texturePath,
        int baseAssetScale,
        bool presentImmediately)
{
    TopdownClearLoadScreenOverlay(state);

    if (texturePath.empty()) {
        return false;
    }

    Texture2D texture = LoadTexture(texturePath.c_str());
    if (texture.id == 0) {
        TraceLog(LOG_WARNING, "Failed loading topdown level load screen: %s", texturePath.c_str());
        return false;
    }

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);

    TopdownLoadScreenOverlay& overlay = state.topdown.runtime.loadScreenOverlay;
    overlay.active = true;
    overlay.waitingForInput = true;
    overlay.fadingOut = false;
    overlay.opacity = 1.0f;
    overlay.fadeTimerMs = 0.0f;
    overlay.fadeDurationMs = LOAD_SCREEN_FADE_DURATION_MS;
    overlay.promptTimerMs = 0.0f;
    overlay.baseAssetScale = std::max(1, baseAssetScale);
    overlay.texturePath = texturePath;
    overlay.texture = texture;
    overlay.textureLoaded = true;

    if (presentImmediately) {
        PresentLoadScreenOnce(overlay);
    }

    return true;
}

void TopdownClearLoadScreenOverlay(GameState& state)
{
    TopdownLoadScreenOverlay& overlay = state.topdown.runtime.loadScreenOverlay;
    if (overlay.textureLoaded && overlay.texture.id != 0) {
        UnloadTexture(overlay.texture);
    }
    overlay = {};
}

bool TopdownLoadScreenBlocksLevelUpdate(const GameState& state)
{
    const TopdownLoadScreenOverlay& overlay = state.topdown.runtime.loadScreenOverlay;
    return overlay.active && overlay.waitingForInput;
}

bool TopdownLoadScreenConsumeInput(GameState& state)
{
    TopdownLoadScreenOverlay& overlay = state.topdown.runtime.loadScreenOverlay;
    if (!overlay.active || !overlay.waitingForInput) {
        return false;
    }

    for (InputEvent& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if (ev.key.key != KEY_SPACE) {
            continue;
        }

        overlay.waitingForInput = false;
        overlay.fadingOut = true;
        overlay.fadeTimerMs = 0.0f;
        overlay.opacity = 1.0f;
        ConsumeEvent(ev);
        return true;
    }

    return true;
}

void TopdownUpdateLoadScreenOverlay(GameState& state, float dt)
{
    TopdownLoadScreenOverlay& overlay = state.topdown.runtime.loadScreenOverlay;
    if (!overlay.active) {
        return;
    }

    overlay.promptTimerMs += dt * 1000.0f;

    if (!overlay.fadingOut) {
        return;
    }

    overlay.fadeTimerMs += dt * 1000.0f;
    const float duration = std::max(1.0f, overlay.fadeDurationMs);
    const float t = std::clamp(overlay.fadeTimerMs / duration, 0.0f, 1.0f);
    overlay.opacity = 1.0f - t;

    if (t >= 1.0f) {
        TopdownClearLoadScreenOverlay(state);
    }
}

static void DrawLoadScreenPrompt(GameState& state, const TopdownLoadScreenOverlay& overlay)
{
    static constexpr const char* kPromptText = "Press SPACE to continue";

    const Font& font = state.narrationTitleFont;
    if (font.texture.id == 0) {
        return;
    }

    const float fontSize = static_cast<float>(font.baseSize);
    const Vector2 textSize = MeasureTextEx(font, kPromptText, fontSize, LOAD_SCREEN_TEXT_SPACING);
    const float pulse01 = 0.5f + 0.5f * std::sin(overlay.promptTimerMs * LOAD_SCREEN_PROMPT_PULSE_SPEED);
    const unsigned char alpha = static_cast<unsigned char>(std::round(170.0f + pulse01 * 85.0f));

    const Vector2 position{
            std::round(INTERNAL_WIDTH * 0.5f - textSize.x * 0.5f),
            std::round(INTERNAL_HEIGHT - LOAD_SCREEN_PROMPT_BOTTOM_PADDING - textSize.y)};

    Color shadow = LOAD_SCREEN_PROMPT_SHADOW_COLOR;
    shadow.a = static_cast<unsigned char>(std::min(230, static_cast<int>(alpha)));
    Color textColor = LOAD_SCREEN_PROMPT_COLOR;
    textColor.a = alpha;

    DrawTextEx(
            font,
            kPromptText,
            Vector2{position.x + 2.0f, position.y + 2.0f},
            fontSize,
            LOAD_SCREEN_TEXT_SPACING,
            shadow);
    DrawTextEx(font, kPromptText, position, fontSize, LOAD_SCREEN_TEXT_SPACING, textColor);
}

void TopdownRenderLoadScreenOverlay(GameState& state)
{
    const TopdownLoadScreenOverlay& overlay = state.topdown.runtime.loadScreenOverlay;
    if (!overlay.active) {
        return;
    }

    const float opacity = std::clamp(overlay.opacity, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(std::round(opacity * 255.0f));
    DrawLoadScreenImageInternal(overlay, alpha);

    if (overlay.waitingForInput) {
        DrawLoadScreenPrompt(state, overlay);
    }
}
