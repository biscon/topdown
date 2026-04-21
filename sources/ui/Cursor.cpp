#include "ui/Cursor.h"
#include "raylib.h"

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
}

void RenderCursor(const GameState& state, float scale)
{
    if (!IsCursorOnScreen()) {
        return;
    }

    const CursorData& cursor = state.cursor;

    Vector2 mouse = GetMousePosition();

    // convert internal → screen space
    mouse.x *= scale;
    mouse.y *= scale;

    const Texture2D* tex = nullptr;
    Vector2 hotspot{};

    switch (cursor.type) {
        case CursorType::Default:
            tex = &cursor.defaultTexture;
            hotspot = cursor.defaultHotspot;
            break;

        case CursorType::Interact:
            tex = &cursor.interactTexture;
            hotspot = cursor.interactHotspot;
            break;
    }

    if (!tex || tex->id == 0) return;

    Vector2 pos{
            mouse.x - hotspot.x * scale,
            mouse.y - hotspot.y * scale
    };
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTextureEx(*tex, pos, 0.0f, scale, WHITE);
    EndBlendMode();
}