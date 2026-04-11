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
}

void TopdownUpdateCamera(GameState& state, float dt)
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

    static constexpr float kAimMaxOffset = 1000.0f;
    static constexpr float kAimStrength = 0.30f;
    static constexpr float kAimResponse = 8.0f;

    if (TopdownLengthSqr(toMouse) > 0.001f) {
        // Compress X so diagonal aim biases a bit more vertically on 16:9.
        const float aspectComp = camera.viewportHeight / camera.viewportWidth; // 1080/1920 = 0.5625

        Vector2 weighted = toMouse;
        weighted.x *= aspectComp;

        const float weightedDist = TopdownLength(weighted);
        if (weightedDist > 0.001f) {
            Vector2 dir = TopdownMul(weighted, 1.0f / weightedDist);

            float strength = std::min(weightedDist, kAimMaxOffset) / kAimMaxOffset;
            strength = 1.0f - std::exp(-3.0f * strength);

            desiredOffset = TopdownMul(dir, strength * kAimMaxOffset * kAimStrength);
        }
    }

    const float aimLerp = std::clamp(kAimResponse * dt, 0.0f, 1.0f);
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

void TopdownUpdateCameraOLD(GameState& state, float dt)
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

    static constexpr float kAimMaxOffset = 500.0f;
    static constexpr float kAimStrength = 0.30f;
    static constexpr float kAimResponse = 8.0f;

    if (TopdownLengthSqr(toMouse) > 0.001f) {
        // Compress X so diagonal aim biases a bit more vertically on 16:9.
        const float aspectComp = camera.viewportHeight / camera.viewportWidth; // 1080/1920 = 0.5625

        Vector2 weighted = toMouse;
        weighted.x *= aspectComp;

        const float weightedDist = TopdownLength(weighted);
        if (weightedDist > 0.001f) {
            Vector2 dir = TopdownMul(weighted, 1.0f / weightedDist);

            float strength = std::min(weightedDist, kAimMaxOffset) / kAimMaxOffset;
            strength = 1.0f - std::exp(-3.0f * strength);

            desiredOffset = TopdownMul(dir, strength * kAimMaxOffset * kAimStrength);
        }
    }

    const float aimLerp = std::clamp(kAimResponse * dt, 0.0f, 1.0f);
    runtime.aimOffset.x += (desiredOffset.x - runtime.aimOffset.x) * aimLerp;
    runtime.aimOffset.y += (desiredOffset.y - runtime.aimOffset.y) * aimLerp;

    // ------------------------------------------------------------
    // 3) Final target = base follow target + smooth aim offset
    // ------------------------------------------------------------
    runtime.targetPosition = TopdownAdd(baseTarget, runtime.aimOffset);
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
