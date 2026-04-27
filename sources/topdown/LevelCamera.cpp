#include "topdown/LevelCamera.h"

#include <algorithm>

#include "topdown/TopdownHelpers.h"
#include "raymath.h"

static Rectangle ComputeCameraClampBounds(const GameState& state)
{
    const TopdownCameraData& camera = state.topdown.camera;
    const std::vector<Vector2>& boundary = state.topdown.authored.levelBoundary;

    if (boundary.empty()) {
        return Rectangle{};
    }

    Rectangle worldBounds = TopdownComputePolygonBounds(boundary);

    Rectangle clamp{};
    clamp.x = worldBounds.x;
    clamp.y = worldBounds.y;
    clamp.width = worldBounds.width;
    clamp.height = worldBounds.height;

    // Camera position is top-left of viewport.
    // Valid camera x range is [worldMinX, worldMaxX - viewportWidth]
    // Valid camera y range is [worldMinY, worldMaxY - viewportHeight]
    clamp.width -= camera.viewportWidth;
    clamp.height -= camera.viewportHeight;

    return clamp;
}

static void ClampCameraPositionToLevel(const GameState& state, Vector2& pos)
{
    const TopdownCameraData& camera = state.topdown.camera;
    const std::vector<Vector2>& boundary = state.topdown.authored.levelBoundary;

    if (boundary.empty()) {
        return;
    }

    const Rectangle worldBounds = TopdownComputePolygonBounds(boundary);
    const Rectangle clamp = ComputeCameraClampBounds(state);

    if (worldBounds.width <= camera.viewportWidth) {
        pos.x = worldBounds.x + (worldBounds.width - camera.viewportWidth) * 0.5f;
    } else {
        pos.x = Clamp(pos.x, clamp.x, clamp.x + clamp.width);
    }

    if (worldBounds.height <= camera.viewportHeight) {
        pos.y = worldBounds.y + (worldBounds.height - camera.viewportHeight) * 0.5f;
    } else {
        pos.y = Clamp(pos.y, clamp.y, clamp.y + clamp.height);
    }
}

static void UpdatePlayerCamera(GameState& state, float dt)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCameraData& camera = state.topdown.camera;
    TopdownCameraRuntime& runtime = state.topdown.runtime.camera;

    const float halfViewportW = camera.viewportWidth * 0.5f;
    const float halfViewportH = camera.viewportHeight * 0.5f;

    const float deadzoneHalfW = camera.deadzoneWidth * 0.5f;
    const float deadzoneHalfH = camera.deadzoneHeight * 0.5f;

    // ------------------------------------------------------------
    // 1) Build base target WITHOUT aim offset accumulation
    // ------------------------------------------------------------
    Vector2 baseTarget = runtime.targetPosition;
    baseTarget = TopdownSub(baseTarget, runtime.aimOffset);

    // If you later decide to keep movement bias persistent in runtime too,
    // subtract it here as well. For now we only add it transiently below.

    const float deadzoneLeft   = baseTarget.x + halfViewportW - deadzoneHalfW;
    const float deadzoneRight  = baseTarget.x + halfViewportW + deadzoneHalfW;
    const float deadzoneTop    = baseTarget.y + halfViewportH - deadzoneHalfH;
    const float deadzoneBottom = baseTarget.y + halfViewportH + deadzoneHalfH;

    if (player.position.x < deadzoneLeft) {
        baseTarget.x = player.position.x - (halfViewportW - deadzoneHalfW);
    } else if (player.position.x > deadzoneRight) {
        baseTarget.x = player.position.x - (halfViewportW + deadzoneHalfW);
    }

    if (player.position.y < deadzoneTop) {
        baseTarget.y = player.position.y - (halfViewportH - deadzoneHalfH);
    } else if (player.position.y > deadzoneBottom) {
        baseTarget.y = player.position.y - (halfViewportH + deadzoneHalfH);
    }

    ClampCameraPositionToLevel(state, baseTarget);

    // ------------------------------------------------------------
    // 2) Mouse-look offset with elliptical weighting
    // ------------------------------------------------------------
    Vector2 mouseWorld = GetMouseWorldPosition(state);
    Vector2 toMouse = TopdownSub(mouseWorld, player.position);

    Vector2 desiredOffset{};

    if (TopdownLengthSqr(toMouse) > 0.001f) {
        // Compress X so diagonal aim biases a bit more vertically on 16:9.
        const float aspectComp = camera.viewportHeight / camera.viewportWidth; // 1080/1920 = 0.5625

        Vector2 weighted = toMouse;
        weighted.x *= aspectComp;

        const float weightedDist = TopdownLength(weighted);
        if (weightedDist > 0.001f) {
            Vector2 dir = TopdownMul(weighted, 1.0f / weightedDist);

            float strength = std::min(weightedDist, camera.aimMaxOffset) / camera.aimMaxOffset;
            strength = 1.0f - std::exp(-3.0f * strength);

            desiredOffset = TopdownMul(dir, strength * camera.aimMaxOffset * camera.aimStrength);
        }
    }

    const float aimLerp = std::clamp(camera.aimResponse * dt, 0.0f, 1.0f);
    runtime.aimOffset.x += (desiredOffset.x - runtime.aimOffset.x) * aimLerp;
    runtime.aimOffset.y += (desiredOffset.y - runtime.aimOffset.y) * aimLerp;

    // ------------------------------------------------------------
    // 2.5) Optional polish
    // ------------------------------------------------------------

    // Slight dampening when standing still.
    // Keeps the camera from feeling too "hung out" when idle.
    if (TopdownLengthSqr(player.velocity) < 1.0f) {
        runtime.aimOffset = TopdownMul(runtime.aimOffset, 0.98f);
    }

    // Tiny movement-direction bias.
    // Helps tactical awareness a little while moving.
    Vector2 moveBias = TopdownMul(player.velocity, 0.10f);

    // ------------------------------------------------------------
    // 3) Final target = base follow target + smooth aim offset + move bias
    // ------------------------------------------------------------
    runtime.targetPosition = TopdownAdd(
            baseTarget,
            TopdownAdd(runtime.aimOffset, moveBias));

    ClampCameraPositionToLevel(state, runtime.targetPosition);

    // ------------------------------------------------------------
    // 4) Normal camera smoothing for final camera motion
    // ------------------------------------------------------------
    const float lerpAlpha = std::clamp(camera.smoothing * dt, 0.0f, 1.0f);

    runtime.position.x += (runtime.targetPosition.x - runtime.position.x) * lerpAlpha;
    runtime.position.y += (runtime.targetPosition.y - runtime.position.y) * lerpAlpha;

    runtime.position.x = std::round(runtime.position.x);
    runtime.position.y = std::round(runtime.position.y);

    ClampCameraPositionToLevel(state, runtime.position);
}

static void UpdateScriptedCamera(GameState& state, float dt)
{
    auto& runtime = state.topdown.runtime.camera;
    const auto& camera = state.topdown.camera;

    if (runtime.isPanning) {
        runtime.panTimerMs += dt * 1000.0f;

        const float durationMs = std::max(runtime.panDurationMs, 0.0001f);
        float t = runtime.panTimerMs / durationMs;
        t = Clamp(t, 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);

        Vector2 pos = Vector2Lerp(runtime.panStart, runtime.panEnd, t);

        runtime.position.x = pos.x - camera.viewportWidth * 0.5f;
        runtime.position.y = pos.y - camera.viewportHeight * 0.5f;

        if (t >= 1.0f) {
            runtime.isPanning = false;
            runtime.scriptedTarget = runtime.panEnd;
        }
    } else {
        runtime.position.x = runtime.scriptedTarget.x - camera.viewportWidth * 0.5f;
        runtime.position.y = runtime.scriptedTarget.y - camera.viewportHeight * 0.5f;
    }

    ClampCameraPositionToLevel(state, runtime.position);
}

static void UpdateManualCamera(GameState& state, float dt)
{
    auto& runtime = state.topdown.runtime.camera;

    const float speed = 800.0f;

    Vector2 move{};

    if (IsKeyDown(KEY_W)) move.y -= 1.0f;
    if (IsKeyDown(KEY_S)) move.y += 1.0f;
    if (IsKeyDown(KEY_A)) move.x -= 1.0f;
    if (IsKeyDown(KEY_D)) move.x += 1.0f;

    if (TopdownLengthSqr(move) > 0.0f) {
        move = TopdownNormalizeOrZero(move);
        runtime.position = TopdownAdd(runtime.position, TopdownMul(move, speed * dt));
    }

    ClampCameraPositionToLevel(state, runtime.position);
}

void TopdownInitCamera(GameState& state)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const TopdownCameraData& camera = state.topdown.camera;
    TopdownCameraRuntime& runtime = state.topdown.runtime.camera;

    runtime.position.x = player.position.x - camera.viewportWidth * 0.5f;
    runtime.position.y = player.position.y - camera.viewportHeight * 0.5f;
    runtime.targetPosition = runtime.position;

    ClampCameraPositionToLevel(state, runtime.targetPosition);
    ClampCameraPositionToLevel(state, runtime.position);
    runtime.aimOffset = Vector2{0.0f, 0.0f};
    runtime.mode = TopdownCameraMode::Player;
    runtime.scriptedTarget = player.position;
    runtime.panStart = runtime.scriptedTarget;
    runtime.panEnd = runtime.scriptedTarget;
    runtime.panTimerMs = 0.0f;
    runtime.panDurationMs = 0.0f;
    runtime.isPanning = false;
}

void TopdownUpdateCamera(GameState& state, float dt)
{
    TopdownCameraRuntime& runtime = state.topdown.runtime.camera;
    switch (runtime.mode) {
        case TopdownCameraMode::Player:
            UpdatePlayerCamera(state, dt);
            break;

        case TopdownCameraMode::Scripted:
            UpdateScriptedCamera(state, dt);
            break;

        case TopdownCameraMode::Manual:
            UpdateManualCamera(state, dt);
            break;
    }
}

