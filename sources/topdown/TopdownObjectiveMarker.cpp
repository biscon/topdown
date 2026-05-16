#include "topdown/TopdownObjectiveMarker.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"
#include "raymath.h"
#include "resources/TextureAsset.h"
#include "topdown/TopdownHelpers.h"
#include "topdown/TopdownScriptCommands.h"

static constexpr const char* OBJECTIVE_MARKER_TEXTURE_PATH =
        ASSETS_PATH "ui/objective_marker.png";
static constexpr float OBJECTIVE_MARKER_SCREEN_PADDING = 64.0f;
static constexpr float OBJECTIVE_MARKER_HOVER_OFFSET = 64.0f;
static constexpr float OBJECTIVE_MARKER_HOVER_BOUNCE = 6.0f;
static constexpr float OBJECTIVE_MARKER_PULSE_AMOUNT = 0.08f;
static constexpr float OBJECTIVE_MARKER_PULSE_SPEED = 0.008f;
static constexpr float OBJECTIVE_MARKER_VISIBLE_ANGLE_DEGREES = 90.0f;

static bool IsObjectiveScreenPositionVisible(Vector2 screenPos)
{
    return screenPos.x >= 0.0f &&
           screenPos.x <= static_cast<float>(INTERNAL_WIDTH) &&
           screenPos.y >= 0.0f &&
           screenPos.y <= static_cast<float>(INTERNAL_HEIGHT);
}

static Vector2 ClampMarkerCenterToScreen(Vector2 pos, const Texture2D& texture, float scale)
{
    const float halfWidth = static_cast<float>(texture.width) * scale * 0.5f;
    const float halfHeight = static_cast<float>(texture.height) * scale * 0.5f;

    pos.x = Clamp(pos.x, halfWidth, static_cast<float>(INTERNAL_WIDTH) - halfWidth);
    pos.y = Clamp(pos.y, halfHeight, static_cast<float>(INTERNAL_HEIGHT) - halfHeight);
    return pos;
}

static Vector2 ComputeEdgeMarkerPosition(Vector2 objectiveScreenPos)
{
    const Vector2 center{
            static_cast<float>(INTERNAL_WIDTH) * 0.5f,
            static_cast<float>(INTERNAL_HEIGHT) * 0.5f
    };

    Vector2 dir = Vector2Subtract(objectiveScreenPos, center);
    const float len = Vector2Length(dir);
    if (len <= 0.001f) {
        return center;
    }

    dir = Vector2Scale(dir, 1.0f / len);

    const float minX = OBJECTIVE_MARKER_SCREEN_PADDING;
    const float maxX = static_cast<float>(INTERNAL_WIDTH) - OBJECTIVE_MARKER_SCREEN_PADDING;
    const float minY = OBJECTIVE_MARKER_SCREEN_PADDING;
    const float maxY = static_cast<float>(INTERNAL_HEIGHT) - OBJECTIVE_MARKER_SCREEN_PADDING;

    float t = 1000000.0f;
    if (dir.x > 0.001f) {
        t = std::min(t, (maxX - center.x) / dir.x);
    } else if (dir.x < -0.001f) {
        t = std::min(t, (minX - center.x) / dir.x);
    }

    if (dir.y > 0.001f) {
        t = std::min(t, (maxY - center.y) / dir.y);
    } else if (dir.y < -0.001f) {
        t = std::min(t, (minY - center.y) / dir.y);
    }

    Vector2 pos = Vector2Add(center, Vector2Scale(dir, t));
    pos.x = Clamp(pos.x, minX, maxX);
    pos.y = Clamp(pos.y, minY, maxY);
    return pos;
}

static void DrawObjectiveMarkerTexture(
        const Texture2D& texture,
        Vector2 center,
        float angleDegrees,
        float scale)
{
    const Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(texture.width),
            static_cast<float>(texture.height)
    };
    const Rectangle dst{
            std::round(center.x),
            std::round(center.y),
            static_cast<float>(texture.width) * scale,
            static_cast<float>(texture.height) * scale
    };
    const Vector2 origin{dst.width * 0.5f, dst.height * 0.5f};

    DrawTexturePro(texture, src, dst, origin, angleDegrees, WHITE);
}

bool LoadTopdownObjectiveMarker(GameState& state)
{
    if (state.topdown.objectiveMarkerTexture >= 0) {
        return true;
    }

    TextureLoadSettings settings{};
    settings.filter = TextureFilterMode::Point;
    settings.wrap = TextureWrapMode::Clamp;

    state.topdown.objectiveMarkerTexture = LoadTextureAsset(
            state.resources,
            OBJECTIVE_MARKER_TEXTURE_PATH,
            settings,
            ResourceScope::Global);

    return state.topdown.objectiveMarkerTexture >= 0;
}

void UnloadTopdownObjectiveMarker(GameState& state)
{
    state.topdown.objectiveMarkerTexture = -1;
}

void TopdownUpdateObjectiveMarker(GameState& state, float dt)
{
    TopdownObjectiveMarkerRuntime& marker = state.topdown.runtime.objectiveMarker;
    if (!marker.active) {
        return;
    }

    marker.animTimerMs += dt * 1000.0f;
}

void TopdownRenderObjectiveMarker(GameState& state)
{
    if (state.mode != GameMode::TopDown || !state.topdown.runtime.levelActive) {
        return;
    }

    const TopdownObjectiveMarkerRuntime& marker = state.topdown.runtime.objectiveMarker;
    if (!marker.active) {
        return;
    }

    const TextureResource* textureResource = FindTextureResource(
            state.resources,
            state.topdown.objectiveMarkerTexture);
    if (textureResource == nullptr || !textureResource->loaded || textureResource->texture.id == 0) {
        return;
    }

    Vector2 objectiveWorldPos{};
    if (!TopdownScriptResolveObjectivePosition(state, objectiveWorldPos)) {
        return;
    }

    const Texture2D& texture = textureResource->texture;
    const Vector2 objectiveScreenPos = TopdownWorldToScreen(state, objectiveWorldPos);
    const float timer = marker.animTimerMs;
    const float pulseScale = 1.0f + sinf(timer * OBJECTIVE_MARKER_PULSE_SPEED) * OBJECTIVE_MARKER_PULSE_AMOUNT;

    if (IsObjectiveScreenPositionVisible(objectiveScreenPos)) {
        const float bounce = sinf(timer * OBJECTIVE_MARKER_PULSE_SPEED) * OBJECTIVE_MARKER_HOVER_BOUNCE;
        Vector2 markerCenter{
                objectiveScreenPos.x,
                objectiveScreenPos.y - OBJECTIVE_MARKER_HOVER_OFFSET + bounce
        };
        markerCenter = ClampMarkerCenterToScreen(markerCenter, texture, pulseScale);
        DrawObjectiveMarkerTexture(
                texture,
                markerCenter,
                OBJECTIVE_MARKER_VISIBLE_ANGLE_DEGREES,
                pulseScale);
        return;
    }

    const Vector2 screenCenter{
            static_cast<float>(INTERNAL_WIDTH) * 0.5f,
            static_cast<float>(INTERNAL_HEIGHT) * 0.5f
    };
    Vector2 direction = Vector2Subtract(objectiveScreenPos, screenCenter);
    if (Vector2LengthSqr(direction) <= 0.001f) {
        direction = Vector2{1.0f, 0.0f};
    }
    const float angleDegrees = atan2f(direction.y, direction.x) * RAD2DEG;
    const Vector2 markerCenter = ComputeEdgeMarkerPosition(objectiveScreenPos);

    DrawObjectiveMarkerTexture(texture, markerCenter, angleDegrees, pulseScale);
}
