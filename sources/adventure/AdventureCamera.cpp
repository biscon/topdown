#include "AdventureCamera.h"
#include "raymath.h"
#include "render/RenderHelpers.h"
#include "AdventureHelpers.h"

static Vector2 ClampCameraPositionToScene(const SceneData& scene, const CameraData& cam, Vector2 pos)
{
    const float maxX = std::max(0.0f, scene.worldWidth - cam.viewportWidth);
    const float maxY = std::max(0.0f, scene.worldHeight - cam.viewportHeight);

    pos.x = Clamp(pos.x, 0.0f, maxX);
    pos.y = Clamp(pos.y, 0.0f, maxY);
    return pos;
}

Vector2 GetActorCameraCenterWorldPos(const GameState& state, const ActorInstance& actor)
{
    const CameraData& cam = state.adventure.camera;

    const Rectangle actorRect = GetActorScreenRect(state, actor);

    return Vector2{
            actorRect.x + (0.5f * actorRect.width) + cam.position.x,
            actorRect.y + (0.5f * actorRect.height) + cam.position.y
    };
}

static Vector2 MakeCameraTopLeftForCenterTarget(const CameraData& cam, Vector2 centerWorldPos)
{
    return Vector2{
            centerWorldPos.x - cam.viewportWidth * 0.5f,
            centerWorldPos.y - cam.viewportHeight * 0.5f
    };
}

// observe, the epsion cut
static float ApplyScalarSmoothing(float currentValue, float targetValue, float smoothing, float dt)
{
    if (smoothing <= 0.0f) {
        return targetValue;
    }

    const float factor = 1.0f - std::exp(-smoothing * dt);
    const float out = currentValue + (targetValue - currentValue) * factor;

    if (std::fabs(targetValue - out) <= 1.0f) {
        return targetValue;
    }

    return out;
}

static Vector2 ApplyCameraSmoothing(Vector2 currentPos, Vector2 targetPos, float smoothing, float dt)
{
    if (smoothing <= 0.0f) {
        return targetPos;
    }

    const float factor = 1.0f - std::exp(-smoothing * dt);

    Vector2 out{
            currentPos.x + (targetPos.x - currentPos.x) * factor,
            currentPos.y + (targetPos.y - currentPos.y) * factor
    };

    const float dx = targetPos.x - out.x;
    const float dy = targetPos.y - out.y;
    const float distSq = dx * dx + dy * dy;

    if (distSq <= 1.0f) { // 0.5 px threshold
        return targetPos;
    }

    return out;
}

static Vector2 ApplyCameraDeadZone(CameraData& cam,
                                   Vector2 currentCameraPos,
                                   Vector2 targetCenterWorldPos)
{
    if (cam.followDeadZoneWidth <= 0.0f || cam.followDeadZoneHeight <= 0.0f) {
        return MakeCameraTopLeftForCenterTarget(cam, targetCenterWorldPos);
    }

    const float zoneHalfW = cam.followDeadZoneWidth * 0.5f;
    const float zoneHalfH = cam.followDeadZoneHeight * 0.5f;

    const float screenCenterX = currentCameraPos.x + cam.viewportWidth * 0.5f;
    const float screenCenterY = currentCameraPos.y + cam.viewportHeight * 0.5f;

    const float zoneLeft   = screenCenterX - zoneHalfW + cam.currentBiasShiftX;
    const float zoneRight  = screenCenterX + zoneHalfW + cam.currentBiasShiftX;
    const float zoneTop    = screenCenterY - zoneHalfH;
    const float zoneBottom = screenCenterY + zoneHalfH;

    Vector2 newCameraPos = currentCameraPos;

    if (targetCenterWorldPos.x < zoneLeft) {
        if (cam.biasLatch != CameraBiasLatch::Left) {
            cam.biasLatch = CameraBiasLatch::Left;
        }

        newCameraPos.x += targetCenterWorldPos.x - zoneLeft;
    } else if (targetCenterWorldPos.x > zoneRight) {
        if (cam.biasLatch != CameraBiasLatch::Right) {
            cam.biasLatch = CameraBiasLatch::Right;
        }

        newCameraPos.x += targetCenterWorldPos.x - zoneRight;
    }

    if (targetCenterWorldPos.y < zoneTop) {
        newCameraPos.y += targetCenterWorldPos.y - zoneTop;
    } else if (targetCenterWorldPos.y > zoneBottom) {
        newCameraPos.y += targetCenterWorldPos.y - zoneBottom;
    }

    return newCameraPos;
}

Vector2 GetImmediateCenteredCameraPosition(const GameState& state, const ActorInstance& actor)
{
    const CameraData& cam = state.adventure.camera;
    const SceneData& scene = state.adventure.currentScene;

    const Vector2 actorCenter = GetActorCameraCenterWorldPos(state, actor);
    Vector2 targetPos = MakeCameraTopLeftForCenterTarget(cam, actorCenter);
    return ClampCameraPositionToScene(scene, cam, targetPos);
}


void UpdateCamera(GameState& state, float dt)
{
    if (!state.adventure.currentScene.loaded) {
        return;
    }

    CameraData& cam = state.adventure.camera;
    const SceneData& scene = state.adventure.currentScene;

    auto getTargetBiasShiftX = [&cam]() -> float {
        switch (cam.biasLatch) {
            case CameraBiasLatch::Left:
                return cam.followBiasX;   // shift whole zone right on screen
            case CameraBiasLatch::Right:
                return -cam.followBiasX;  // shift whole zone left on screen
            case CameraBiasLatch::None:
            default:
                return 0.0f;
        }
    };

    cam.currentBiasShiftX = ApplyScalarSmoothing(
            cam.currentBiasShiftX,
            getTargetBiasShiftX(),
            cam.biasShiftSmoothing,
            dt);

    Vector2 targetPos = cam.position;

    switch (cam.mode) {
        case CameraModeData::FollowControlledActor:
        {
            const ActorInstance* controlledActor = GetControlledActor(state);
            if (controlledActor != nullptr) {
                const Vector2 actorCenter = GetActorCameraCenterWorldPos(state, *controlledActor);
                targetPos = ApplyCameraDeadZone(cam, cam.position, actorCenter);
                targetPos = ApplyCameraSmoothing(cam.position, targetPos, cam.followSmoothing, dt);
            }

            cam.moving = false;
            break;
        }

        case CameraModeData::FollowActor:
        {
            const ActorInstance* actor = FindActorInstanceByHandle(state, cam.followedActor);
            if (actor != nullptr && actor->activeInScene && actor->visible) {
                const Vector2 actorCenter = GetActorCameraCenterWorldPos(state, *actor);
                targetPos = ApplyCameraDeadZone(cam, cam.position, actorCenter);
                targetPos = ApplyCameraSmoothing(cam.position, targetPos, cam.followSmoothing, dt);
            }

            cam.moving = false;
            break;
        }

        case CameraModeData::Scripted:
        {
            cam.currentBiasShiftX = 0.0f;
            cam.biasLatch = CameraBiasLatch::None;

            if (cam.moving) {
                cam.moveElapsedMs += dt * 1000.0f;

                float t = 1.0f;
                if (cam.moveDurationMs > 0.0f) {
                    t = cam.moveElapsedMs / cam.moveDurationMs;
                }

                if (t >= 1.0f) {
                    t = 1.0f;
                    cam.moving = false;
                }

                const float easedT = ApplyInterpolation(cam.interpolation, t);

                targetPos.x = cam.moveStart.x + (cam.moveTarget.x - cam.moveStart.x) * easedT;
                targetPos.y = cam.moveStart.y + (cam.moveTarget.y - cam.moveStart.y) * easedT;
            }
            break;
        }
    }

    cam.position = ClampCameraPositionToScene(scene, cam, targetPos);
}

