#include "RenderHelpers.h"
#include "resources/AsepriteAsset.h"
#include "scene/SceneHelpers.h"
#include "resources/TextureAsset.h"
#include "adventure/AdventureHelpers.h"
#include "raymath.h"

bool GetActorCurrentFrameInfo(
        const GameState& state,
        const ActorInstance& actor,
        const SpriteAssetResource*& outAsset,
        const SpriteFrame*& outFrame,
        float& outFinalScale,
        Vector2& outScreenFeet)
{
    outAsset = nullptr;
    outFrame = nullptr;
    outFinalScale = 1.0f;
    outScreenFeet = {};

    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, actor.actorDefIndex);
    if (actorDef == nullptr) {
        return false;
    }

    const SpriteAssetResource* asset =
            FindSpriteAssetResource(state.resources, actorDef->spriteAssetHandle);

    if (asset == nullptr || !asset->loaded) {
        return false;
    }

    const float depthScale = ComputeDepthScale(state.adventure.currentScene, actor.feetPos.y);
    const float finalScale = asset->baseDrawScale * depthScale;

    const Vector2 cam = state.adventure.camera.position;
    const Vector2 screenFeet = {
            actor.feetPos.x - cam.x,
            actor.feetPos.y - cam.y
    };

    for (const std::string& layerName : asset->layerNames) {
        int clipIndex = FindClipIndex(*asset, layerName, actor.currentAnimation);
        if (clipIndex < 0) {
            continue;
        }

        const SpriteClip& clip = asset->clips[clipIndex];
        const int frameIndex = GetLoopingFrameIndex(*asset, clip, actor.animationTimeMs);
        if (frameIndex < 0) {
            continue;
        }

        outAsset = asset;
        outFrame = &asset->frames[frameIndex];
        outFinalScale = finalScale;
        outScreenFeet = screenFeet;
        return true;
    }

    return false;
}

Rectangle GetActorScreenRect(const GameState& state, const ActorInstance& actor)
{
    const SpriteAssetResource* asset = nullptr;
    const SpriteFrame* frame = nullptr;
    float finalScale = 1.0f;
    Vector2 screenFeet{};

    if (!GetActorCurrentFrameInfo(state, actor, asset, frame, finalScale, screenFeet)) {
        return Rectangle{
                screenFeet.x - 32.0f,
                screenFeet.y - 128.0f,
                64.0f,
                128.0f
        };
    }

    Rectangle rect{};
    rect.x = screenFeet.x - asset->feetPivot.x * finalScale;
    rect.y = screenFeet.y - asset->feetPivot.y * finalScale;
    rect.width = frame->sourceSize.x * finalScale;
    rect.height = frame->sourceSize.y * finalScale;
    return rect;
}

Rectangle GetActorWorldRect(const GameState& state, const ActorInstance& actor) {
    const CameraData& cam = state.adventure.camera;
    const Rectangle actorRect = GetActorScreenRect(state, actor);
    // convert from screen to world space and center
    Rectangle rect{};
    rect.x = actorRect.x + cam.position.x;
    rect.y = actorRect.y + cam.position.y;
    rect.width = actorRect.width;
    rect.height = actorRect.height;
    return rect;
}

Rectangle GetActorInteractionRect(const GameState& state, const ActorInstance& actor)
{
    Rectangle rect = GetActorScreenRect(state, actor);

    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, actor.actorDefIndex);
    if (actorDef == nullptr) {
        return rect;
    }

    const float horizScale = Clamp(actorDef->horizHitboxScale, 0.1f, 1.0f);
    const float vertScale = Clamp(actorDef->vertHitboxScale, 0.1f, 1.0f);

    const float newWidth = rect.width * horizScale;
    const float newHeight = rect.height * vertScale;

    rect.x += (rect.width - newWidth) * 0.5f;
    rect.y += (rect.height - newHeight);
    rect.width = newWidth;
    rect.height = newHeight;

    return rect;
}

Rectangle GetPropScreenRect(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    if (sceneProp.visualType == ScenePropVisualType::Sprite &&
        sceneProp.spriteAssetHandle >= 0) {
        const SpriteAssetResource* asset =
                FindSpriteAssetResource(state.resources, sceneProp.spriteAssetHandle);
        if (asset != nullptr && asset->loaded) {
            const float depthScale = sceneProp.depthScaling
                                     ? ComputeDepthScale(state.adventure.currentScene, prop.feetPos.y)
                                     : 1.0f;
            const float finalScale = asset->baseDrawScale * depthScale;

            const Vector2 cam = state.adventure.camera.position;
            const Vector2 screenFeet{
                    prop.feetPos.x - cam.x,
                    prop.feetPos.y - cam.y
            };

            for (const std::string& layerName : asset->layerNames) {
                const int clipIndex = FindClipIndex(*asset, layerName, prop.currentAnimation);
                if (clipIndex < 0) {
                    continue;
                }

                const SpriteClip& clip = asset->clips[clipIndex];

                int frameIndex = -1;
                if (prop.oneShotActive) {
                    frameIndex = GetOneShotFrameIndex(*asset, clip, prop.animationTimeMs);
                } else {
                    frameIndex = GetLoopingFrameIndex(*asset, clip, prop.animationTimeMs);
                }

                if (frameIndex < 0) {
                    continue;
                }

                const SpriteFrame& frame = asset->frames[frameIndex];

                const bool hasFeetPivot =
                        (asset->feetPivot.x > 0.0f || asset->feetPivot.y > 0.0f);

                const float pivotX = hasFeetPivot
                                     ? asset->feetPivot.x
                                     : frame.sourceSize.x * 0.5f;
                const float pivotY = hasFeetPivot
                                     ? asset->feetPivot.y
                                     : frame.sourceSize.y;

                Rectangle rect{};
                rect.x = screenFeet.x - pivotX * finalScale;
                rect.y = screenFeet.y - pivotY * finalScale;
                rect.width = frame.sourceSize.x * finalScale;
                rect.height = frame.sourceSize.y * finalScale;
                return rect;
            }
        }
    }

    if (sceneProp.visualType == ScenePropVisualType::Image &&
        sceneProp.textureHandle >= 0) {
        const TextureResource* texRes =
                FindTextureResource(state.resources, sceneProp.textureHandle);
        if (texRes != nullptr && texRes->loaded) {
            const float depthScale = sceneProp.depthScaling
                                     ? ComputeDepthScale(state.adventure.currentScene, prop.feetPos.y)
                                     : 1.0f;
            const float finalScale =
                    static_cast<float>(state.adventure.currentScene.baseAssetScale) * depthScale;

            const Vector2 cam = state.adventure.camera.position;
            const Vector2 screenFeet{
                    prop.feetPos.x - cam.x,
                    prop.feetPos.y - cam.y
            };

            Rectangle rect{};
            rect.x = screenFeet.x - static_cast<float>(texRes->texture.width) * 0.5f * finalScale;
            rect.y = screenFeet.y - static_cast<float>(texRes->texture.height) * finalScale;
            rect.width = static_cast<float>(texRes->texture.width) * finalScale;
            rect.height = static_cast<float>(texRes->texture.height) * finalScale;
            return rect;
        }
    }

    return Rectangle{
            prop.feetPos.x - state.adventure.camera.position.x - 32.0f,
            prop.feetPos.y - state.adventure.camera.position.y - 64.0f,
            64.0f,
            64.0f
    };
}
