#include "ui/Cursor.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>

#include "topdown/TopdownHelpers.h"
#include "topdown/PlayerRegistry.h"

namespace {

static Texture2D LoadTexturePreMultiplied(const char* fileName)
{
    Image img = LoadImage(fileName);
    if (img.data == nullptr) {
        TraceLog(LOG_ERROR, "Failed to load image: %s", fileName);
        return Texture2D{};
    }

    ImageAlphaPremultiply(&img);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    }

    return tex;
}

enum class AimCursorColorState {
    Neutral,
    OverHostileInRange,
    OutOfRange
};

bool ShouldRenderAimCursor(const GameState& state)
{
    return state.mode == GameMode::TopDown && state.topdown.runtime.levelActive;
}

bool IsMouseOverHostileNpc(const GameState& state, Vector2 mouseWorld)
{
    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible || npc.dead || !npc.hostile) {
            continue;
        }

        if (CheckCollisionPointCircle(mouseWorld, npc.position, npc.collisionRadius)) {
            return true;
        }
    }

    return false;
}

bool IsMouseAimOutOfRange(const GameState& state, Vector2 mouseWorld)
{
    const TopdownRuntimeData& runtime = state.topdown.runtime;
    const TopdownCharacterRuntime& character = runtime.playerCharacter;

    const TopdownPlayerWeaponConfig* weaponConfig =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, character.equippedSetId);
    if (weaponConfig == nullptr || weaponConfig->maxRange <= 0.0f) {
        return false;
    }

    const Vector2 toAim = TopdownSub(mouseWorld, runtime.player.position);
    const float distanceToAim = TopdownLength(toAim);
    return distanceToAim > weaponConfig->maxRange;
}

AimCursorColorState EvaluateAimCursorColorState(const GameState& state)
{
    const Vector2 mouseWorld = GetMouseWorldPosition(state);
    if (IsMouseAimOutOfRange(state, mouseWorld)) {
        return AimCursorColorState::OutOfRange;
    }

    if (IsMouseOverHostileNpc(state, mouseWorld)) {
        return AimCursorColorState::OverHostileInRange;
    }

    return AimCursorColorState::Neutral;
}

Color GetAimCursorColor(AimCursorColorState colorState)
{
    switch (colorState) {
        case AimCursorColorState::OutOfRange:
            return RED;
        case AimCursorColorState::OverHostileInRange:
            return GREEN;
        case AimCursorColorState::Neutral:
        default:
            return WHITE;
    }
}

void DrawAimCursorCrosshair(Vector2 center, float pulseOffset, float scale, Color color)
{
    const float lineThickness = std::max(1.0f, 2.0f * scale);
    const float centerGap = (4.0f + pulseOffset) * scale;
    const float armLength = (10.0f + pulseOffset) * scale;
    const Vector2 shadowOffset{2.0f * scale, 2.0f * scale};

    const auto drawCrosshair = [&](Vector2 offset, Color drawColor) {
        DrawLineEx(
                Vector2{center.x + offset.x, center.y - centerGap + offset.y},
                Vector2{center.x + offset.x, center.y - armLength + offset.y},
                lineThickness,
                drawColor);
        DrawLineEx(
                Vector2{center.x + offset.x, center.y + centerGap + offset.y},
                Vector2{center.x + offset.x, center.y + armLength + offset.y},
                lineThickness,
                drawColor);
        DrawLineEx(
                Vector2{center.x - centerGap + offset.x, center.y + offset.y},
                Vector2{center.x - armLength + offset.x, center.y + offset.y},
                lineThickness,
                drawColor);
        DrawLineEx(
                Vector2{center.x + centerGap + offset.x, center.y + offset.y},
                Vector2{center.x + armLength + offset.x, center.y + offset.y},
                lineThickness,
                drawColor);
    };

    drawCrosshair(shadowOffset, BLACK);
    drawCrosshair(Vector2{}, color);
}

void RenderBitmapCursor(const GameState& state, float scale)
{
    const CursorData& cursor = state.cursor;

    Vector2 mouse = GetMousePosition();

    // convert internal → screen space
    mouse.x *= scale;
    mouse.y *= scale;

    const Texture2D* tex = nullptr;
    Vector2 hotspot{};

    switch (cursor.type) {
        case CursorType::Default:
        case CursorType::Aim:
            tex = &cursor.defaultTexture;
            hotspot = cursor.defaultHotspot;
            break;

        case CursorType::Interact:
            tex = &cursor.interactTexture;
            hotspot = cursor.interactHotspot;
            break;
    }

    if (!tex || tex->id == 0) {
        return;
    }

    Vector2 pos{
            mouse.x - hotspot.x * scale,
            mouse.y - hotspot.y * scale
    };
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTextureEx(*tex, pos, 0.0f, scale, WHITE);
    EndBlendMode();
}

void RenderAimCursor(const GameState& state, float scale)
{
    Vector2 mouse = GetMousePosition();
    mouse.x *= scale;
    mouse.y *= scale;

    const float pulsePhase = state.cursor.aimPulseTimeSeconds * 3.0f;
    const float pulseOffset = std::sin(pulsePhase) * 1.2f;

    const AimCursorColorState colorState = EvaluateAimCursorColorState(state);
    DrawAimCursorCrosshair(mouse, pulseOffset, scale, GetAimCursorColor(colorState));
}

} // namespace

void InitCursor(GameState& state)
{
    CursorData& cursor = state.cursor;

    cursor.defaultTexture = LoadTexturePreMultiplied(ASSETS_PATH "ui/cursor_default_small.png");
    cursor.interactTexture = LoadTexturePreMultiplied(ASSETS_PATH "ui/cursor_interact_small.png");

    SetTextureFilter(cursor.defaultTexture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(cursor.interactTexture, TEXTURE_FILTER_BILINEAR);

    // set hotspot on the cursors
    cursor.defaultHotspot = {2, 4};     // arrow tip
    cursor.interactHotspot = {13, 4};

    cursor.aimPulseTimeSeconds = 0.0f;
    cursor.initialized = true;
}

void ShutdownCursor(GameState& state)
{
    CursorData& cursor = state.cursor;

    if (cursor.defaultTexture.id != 0) {
        UnloadTexture(cursor.defaultTexture);
    }

    if (cursor.interactTexture.id != 0) {
        UnloadTexture(cursor.interactTexture);
    }

    ShowCursor();
}

void UpdateCursor(GameState& state)
{
    if (IsCursorOnScreen()) {
        HideCursor();
    } else {
        ShowCursor();
    }

    state.cursor.aimPulseTimeSeconds += GetFrameTime();
}

void RenderCursor(const GameState& state, float scale)
{
    if (!IsCursorOnScreen()) {
        return;
    }

    if (ShouldRenderAimCursor(state)) {
        RenderAimCursor(state, scale);
        return;
    }

    RenderBitmapCursor(state, scale);
}
