#include "render/SceneRenderInternal.h"

#include <cmath>

#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"
#include "scene/SceneHelpers.h"
#include "adventure/AdventureHelpers.h"
#include "render/EffectShaderRegistry.h"

static unsigned char MultiplyU8(unsigned char a, unsigned char b)
{
    const int value = static_cast<int>(a) * static_cast<int>(b);
    return static_cast<unsigned char>((value + 127) / 255);
}

static Color BuildEffectSpriteDrawColor(const EffectSpriteInstance& effect)
{
    Color c = effect.tint;
    c.a = MultiplyU8(c.a, static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity))));
    return c;
}

static int GetRaylibBlendMode(EffectBlendMode mode)
{
    switch (mode) {
        case EffectBlendMode::Add:
            return BLEND_ADDITIVE;
        case EffectBlendMode::Multiply:
            return BLEND_MULTIPLIED;
        case EffectBlendMode::Normal:
        default:
            return BLEND_ALPHA_PREMULTIPLY;
    }
}

static void SetShaderFloatIfValid(const Shader& shader, int loc, float value)
{
    if (loc < 0) {
        return;
    }

    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_FLOAT);
}

static void SetShaderVec2IfValid(const Shader& shader, int loc, Vector2 value)
{
    if (loc < 0) {
        return;
    }

    const float v[2] = { value.x, value.y };
    SetShaderValue(shader, loc, v, SHADER_UNIFORM_VEC2);
}

static Rectangle BuildPolygonBounds(const ScenePolygon& polygon)
{
    Rectangle r{};

    if (polygon.vertices.empty()) {
        return r;
    }

    float minX = polygon.vertices[0].x;
    float minY = polygon.vertices[0].y;
    float maxX = polygon.vertices[0].x;
    float maxY = polygon.vertices[0].y;

    for (const Vector2& v : polygon.vertices) {
        if (v.x < minX) minX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.x > maxX) maxX = v.x;
        if (v.y > maxY) maxY = v.y;
    }

    r.x = minX;
    r.y = minY;
    r.width = maxX - minX;
    r.height = maxY - minY;
    return r;
}

static void SetShaderIntIfValid(const Shader& shader, int loc, int value)
{
    if (loc < 0) {
        return;
    }

    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_INT);
}

static void SetShaderPolygonIfValid(
        const Shader& shader,
        int usePolygonLoc,
        int polygonVertexCountLoc,
        int polygonPointsLoc,
        const SceneEffectRegionData& sceneEffect,
        const Vector2& cam)
{
    const int usePolygon = sceneEffect.usePolygon ? 1 : 0;
    SetShaderIntIfValid(shader, usePolygonLoc, usePolygon);

    if (!sceneEffect.usePolygon) {
        SetShaderIntIfValid(shader, polygonVertexCountLoc, 0);
        return;
    }

    const int vertexCount = static_cast<int>(sceneEffect.polygon.vertices.size());
    SetShaderIntIfValid(shader, polygonVertexCountLoc, vertexCount);

    if (polygonPointsLoc < 0 || vertexCount <= 0) {
        return;
    }

    float points[32 * 2] = {};
    for (int i = 0; i < vertexCount && i < 32; ++i) {
        points[i * 2 + 0] = sceneEffect.polygon.vertices[i].x - cam.x;
        points[i * 2 + 1] = sceneEffect.polygon.vertices[i].y - cam.y;
    }

    SetShaderValueV(shader, polygonPointsLoc, points, SHADER_UNIFORM_VEC2, vertexCount);
}

static Vector2 GetRoundedRenderCamera(const GameState& state)
{
    //return state.adventure.camera.position;

    const Vector2 cam = state.adventure.camera.position;
    return Vector2{
            std::round(cam.x),
            std::round(cam.y)
    };
}

void SceneRenderDrawSceneImageLayer(const GameState& state, const SceneImageLayer& layer)
{
    if (!layer.visible || layer.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, layer.textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const Vector2 cam = GetRoundedRenderCamera(state);

    Rectangle src{};
    src.x = 0.0f;
    src.y = 0.0f;
    src.width = layer.sourceSize.x;
    src.height = layer.sourceSize.y;

    Rectangle dst{};
    dst.x = layer.worldPos.x - cam.x * layer.parallaxX;
    dst.y = layer.worldPos.y - cam.y * layer.parallaxY;
    dst.width = layer.worldSize.x;
    dst.height = layer.worldSize.y;

    const unsigned char alpha = static_cast<unsigned char>(std::round(255.0f * Clamp01(layer.opacity)));
    const Color tint = Color{255, 255, 255, alpha};

    DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void SceneRenderDrawSceneEffectSprite(
        const GameState& state,
        const SceneEffectSpriteData& sceneEffect,
        const EffectSpriteInstance& effect)
{
    if (!effect.visible || sceneEffect.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, sceneEffect.textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const Vector2 cam = GetRoundedRenderCamera(state);

    Rectangle src{};
    src.x = 0.0f;
    src.y = 0.0f;
    src.width = sceneEffect.sourceSize.x;
    src.height = sceneEffect.sourceSize.y;

    Rectangle dst{};
    dst.x = sceneEffect.worldPos.x - cam.x;
    dst.y = sceneEffect.worldPos.y - cam.y;
    dst.width = sceneEffect.worldSize.x;
    dst.height = sceneEffect.worldSize.y;

    // round to whole pixels
    dst.x = std::round(dst.x);
    dst.y = std::round(dst.y);
    dst.width = std::round(dst.width);
    dst.height = std::round(dst.height);

    const EffectShaderCategory shaderCategory = GetEffectShaderCategory(effect.shaderType);
    Color drawColor = BuildEffectSpriteDrawColor(effect);
    if (shaderCategory != EffectShaderCategory::None) {
        drawColor = WHITE;
        drawColor.a = static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity)));
    }

    EndBlendMode();
    BeginBlendMode(GetRaylibBlendMode(sceneEffect.blendMode));

    if (shaderCategory == EffectShaderCategory::SelfTexture) {
        const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(effect.shaderType);
        if (shaderEntry != nullptr) {
            const float timeSeconds = static_cast<float>(GetTime());

            BeginShaderMode(shaderEntry->shader);

            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, effect.shaderParams.scrollSpeed);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, effect.shaderParams.uvScale);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, effect.shaderParams.distortionAmount);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, effect.shaderParams.noiseScrollSpeed);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, effect.shaderParams.intensity);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, effect.shaderParams.phaseOffset);
            if (shaderEntry->tintLoc >= 0) {
                const float tint[3] = {
                        effect.shaderParams.tintR,
                        effect.shaderParams.tintG,
                        effect.shaderParams.tintB
                };
                SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
            }

            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);

            EndShaderMode();
        } else {
            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);
        }
    } else {
        DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

void SceneRenderDrawSceneEffectRegion(
        const GameState& state,
        const SceneEffectRegionData& sceneEffect,
        const EffectRegionInstance& effect)
{
    if (!effect.visible) {
        return;
    }

    const EffectShaderCategory shaderCategory = GetEffectShaderCategory(effect.shaderType);
    if (shaderCategory == EffectShaderCategory::SelfTexture && sceneEffect.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = nullptr;
    if (sceneEffect.textureHandle >= 0) {
        texRes = FindTextureResource(state.resources, sceneEffect.textureHandle);
        if (texRes == nullptr || !texRes->loaded) {
            return;
        }
    }

    const Vector2 cam = GetRoundedRenderCamera(state);

    const Rectangle effectBounds = sceneEffect.usePolygon
                                   ? BuildPolygonBounds(sceneEffect.polygon)
                                   : sceneEffect.worldRect;

    Rectangle src{};
    if (texRes != nullptr) {
        src.x = 0.0f;
        src.y = 0.0f;
        src.width = static_cast<float>(texRes->texture.width);
        src.height = static_cast<float>(texRes->texture.height);
    }

    Rectangle dst{};
    dst.x = effectBounds.x - cam.x;
    dst.y = effectBounds.y - cam.y;
    dst.width = effectBounds.width;
    dst.height = effectBounds.height;

    // round to whole pixels
    dst.x = std::round(dst.x);
    dst.y = std::round(dst.y);
    dst.width = std::round(dst.width);
    dst.height = std::round(dst.height);

    Color drawColor = WHITE;
    drawColor.a = static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity)));

    if (shaderCategory == EffectShaderCategory::None) {
        drawColor = effect.tint;
        drawColor.a = MultiplyU8(
                drawColor.a,
                static_cast<unsigned char>(std::round(255.0f * Clamp01(effect.opacity))));
    }

    EndBlendMode();
    BeginBlendMode(GetRaylibBlendMode(sceneEffect.blendMode));

    if (shaderCategory == EffectShaderCategory::SelfTexture) {
        const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(effect.shaderType);
        if (shaderEntry != nullptr) {
            const float timeSeconds = static_cast<float>(GetTime());
            const Vector2 sceneSize{ 1920.0f, 1080.0f };

            const Vector2 regionPos{ dst.x, dst.y };
            const Vector2 regionSize{ dst.width, dst.height };

            BeginShaderMode(shaderEntry->shader);

            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, effect.shaderParams.scrollSpeed);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, effect.shaderParams.uvScale);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, effect.shaderParams.distortionAmount);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, effect.shaderParams.noiseScrollSpeed);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, effect.shaderParams.intensity);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, effect.shaderParams.phaseOffset);

            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->sceneSizeLoc, sceneSize);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionPosLoc, regionPos);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionSizeLoc, regionSize);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, effect.shaderParams.softness);

            if (shaderEntry->tintLoc >= 0) {
                const float tint[3] = {
                        effect.shaderParams.tintR,
                        effect.shaderParams.tintG,
                        effect.shaderParams.tintB
                };
                SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
            }

            SetShaderPolygonIfValid(
                    shaderEntry->shader,
                    shaderEntry->usePolygonLoc,
                    shaderEntry->polygonVertexCountLoc,
                    shaderEntry->polygonPointsLoc,
                    sceneEffect,
                    cam);

            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);

            EndShaderMode();
        } else {
            DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);
        }
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

void SceneRenderDrawActor(const GameState& state, const ActorInstance& actor)
{
    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, actor.actorDefIndex);
    if (actorDef == nullptr || actorDef->spriteAssetHandle < 0) {
        return;
    }

    const SpriteAssetResource* asset =
            FindSpriteAssetResource(state.resources, actorDef->spriteAssetHandle);
    if (asset == nullptr || !asset->loaded) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, asset->textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const float depthScale = ComputeDepthScale(state.adventure.currentScene, actor.feetPos.y);
    const float finalScale = asset->baseDrawScale * depthScale;

    const Vector2 cam = GetRoundedRenderCamera(state);
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

        int frameIndex = -1;
        if (actor.scriptAnimationActive) {
            frameIndex = GetOneShotFrameIndex(*asset, clip, actor.animationTimeMs);
        } else {
            frameIndex = GetLoopingFrameIndex(*asset, clip, actor.animationTimeMs);
        }

        if (frameIndex < 0) {
            continue;
        }

        const SpriteFrame& frame = asset->frames[frameIndex];

        Rectangle src = frame.sourceRect;
        if (actor.flipX) {
            src.width = -src.width;
        }

        const float drawWidth = frame.sourceSize.x * finalScale;
        const float drawHeight = frame.sourceSize.y * finalScale;

        Rectangle dst{};
        dst.x = screenFeet.x;
        dst.y = screenFeet.y;
        dst.width = drawWidth;
        dst.height = drawHeight;

        const bool hasFeetPivot =
                (asset->feetPivot.x > 0.0f || asset->feetPivot.y > 0.0f);

        const float pivotX = hasFeetPivot
                             ? asset->feetPivot.x
                             : frame.sourceSize.x * 0.5f;
        const float pivotY = hasFeetPivot
                             ? asset->feetPivot.y
                             : frame.sourceSize.y;

        Vector2 origin{};
        origin.y = pivotY * finalScale;

        if (actor.flipX) {
            origin.x = (frame.sourceSize.x - pivotX) * finalScale;
        } else {
            origin.x = pivotX * finalScale;
        }

        // round to whole pixels
        dst.x = std::round(dst.x);
        dst.y = std::round(dst.y);
        origin.x = std::round(origin.x);
        origin.y = std::round(origin.y);
        dst.width = std::round(dst.width);
        dst.height = std::round(dst.height);

        DrawTexturePro(texRes->texture, src, dst, origin, 0.0f, WHITE);
    }
}

static void DrawSpriteProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    const SpriteAssetResource* asset =
            FindSpriteAssetResource(state.resources, sceneProp.spriteAssetHandle);
    if (asset == nullptr || !asset->loaded) {
        return;
    }

    const TextureResource* texRes =
            FindTextureResource(state.resources, asset->textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    if (prop.currentAnimation.empty()) {
        return;
    }

    const float depthScale = sceneProp.depthScaling
                             ? ComputeDepthScale(state.adventure.currentScene, prop.feetPos.y)
                             : 1.0f;
    const float finalScale = asset->baseDrawScale * depthScale;

    const Vector2 cam = GetRoundedRenderCamera(state);
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

        Rectangle src = frame.sourceRect;
        if (prop.flipX) {
            src.width = -src.width;
        }

        Rectangle dst{};
        dst.x = screenFeet.x;
        dst.y = screenFeet.y;
        dst.width = frame.sourceSize.x * finalScale;
        dst.height = frame.sourceSize.y * finalScale;

        const bool hasFeetPivot =
                (asset->feetPivot.x > 0.0f || asset->feetPivot.y > 0.0f);

        const float pivotX = hasFeetPivot
                             ? asset->feetPivot.x
                             : frame.sourceSize.x * 0.5f;
        const float pivotY = hasFeetPivot
                             ? asset->feetPivot.y
                             : frame.sourceSize.y;

        Vector2 origin{};
        origin.y = pivotY * finalScale;
        origin.x = prop.flipX
                   ? (frame.sourceSize.x - pivotX) * finalScale
                   : pivotX * finalScale;

        // round to whole pixels
        dst.x = std::round(dst.x);
        dst.y = std::round(dst.y);
        origin.x = std::round(origin.x);
        origin.y = std::round(origin.y);
        dst.width = std::round(dst.width);
        dst.height = std::round(dst.height);

        DrawTexturePro(texRes->texture, src, dst, origin, 0.0f, WHITE);
    }
}

static void DrawImageProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    const TextureResource* texRes =
            FindTextureResource(state.resources, sceneProp.textureHandle);
    if (texRes == nullptr || !texRes->loaded) {
        return;
    }

    const float depthScale = sceneProp.depthScaling
                             ? ComputeDepthScale(state.adventure.currentScene, prop.feetPos.y)
                             : 1.0f;
    const float finalScale = static_cast<float>(state.adventure.currentScene.baseAssetScale) * depthScale;

    const Vector2 cam = GetRoundedRenderCamera(state);
    const Vector2 screenFeet{
            prop.feetPos.x - cam.x,
            prop.feetPos.y - cam.y
    };

    Rectangle src{};
    src.x = 0.0f;
    src.y = 0.0f;
    src.width = static_cast<float>(texRes->texture.width);
    src.height = static_cast<float>(texRes->texture.height);

    if (prop.flipX) {
        src.width = -src.width;
    }

    Rectangle dst{};
    dst.x = screenFeet.x;
    dst.y = screenFeet.y;
    dst.width = static_cast<float>(texRes->texture.width) * finalScale;
    dst.height = static_cast<float>(texRes->texture.height) * finalScale;

    Vector2 origin{};
    origin.x = (static_cast<float>(texRes->texture.width) * 0.5f) * finalScale;
    origin.y = static_cast<float>(texRes->texture.height) * finalScale;

    // round to whole pixels
    dst.x = std::round(dst.x);
    dst.y = std::round(dst.y);
    origin.x = std::round(origin.x);
    origin.y = std::round(origin.y);
    dst.width = std::round(dst.width);
    dst.height = std::round(dst.height);

    DrawTexturePro(texRes->texture, src, dst, origin, 0.0f, WHITE);
}

void SceneRenderDrawProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop)
{
    if (!prop.visible) {
        return;
    }

    switch (sceneProp.visualType) {
        case ScenePropVisualType::Sprite:
            DrawSpriteProp(state, sceneProp, prop);
            break;

        case ScenePropVisualType::Image:
            DrawImageProp(state, sceneProp, prop);
            break;

        case ScenePropVisualType::None:
        default:
            break;
    }
}

void SceneRenderDrawBackProps(const GameState& state)
{
    const int propCount = std::min(
            static_cast<int>(state.adventure.currentScene.props.size()),
            static_cast<int>(state.adventure.props.size()));

    for (int i = 0; i < propCount; ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
        const PropInstance& prop = state.adventure.props[i];

        if (!prop.visible) {
            continue;
        }

        if (sceneProp.depthMode != ScenePropDepthMode::Back) {
            continue;
        }

        SceneRenderDrawProp(state, sceneProp, prop);
    }
}

void SceneRenderDrawBackEffectSprites(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Back) {
            continue;
        }

        SceneRenderDrawSceneEffectSprite(state, sceneEffect, effect);
    }
}

void SceneRenderDrawFrontProps(const GameState& state)
{
    const int propCount = std::min(
            static_cast<int>(state.adventure.currentScene.props.size()),
            static_cast<int>(state.adventure.props.size()));

    for (int i = 0; i < propCount; ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
        const PropInstance& prop = state.adventure.props[i];

        if (!prop.visible) {
            continue;
        }

        if (sceneProp.depthMode != ScenePropDepthMode::Front) {
            continue;
        }

        SceneRenderDrawProp(state, sceneProp, prop);
    }
}

void SceneRenderDrawFrontEffectSprites(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Front) {
            continue;
        }

        if (sceneEffect.renderAsOverlay) {
            continue;
        }

        SceneRenderDrawSceneEffectSprite(state, sceneEffect, effect);
    }
}

void SceneRenderDrawOverlayEffectSprites(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

        if (!effect.visible) {
            continue;
        }

        if (!sceneEffect.renderAsOverlay) {
            continue;
        }

        SceneRenderDrawSceneEffectSprite(state, sceneEffect, effect);
    }
}

void SceneRenderDrawBackEffectRegionsSelfTextureOnly(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Back) {
            continue;
        }

        if (GetEffectShaderCategory(effect.shaderType) == EffectShaderCategory::SceneSample) {
            continue;
        }

        SceneRenderDrawSceneEffectRegion(state, sceneEffect, effect);
    }
}

void SceneRenderDrawFrontEffectRegionsSelfTextureOnly(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::Front) {
            continue;
        }

        if (sceneEffect.renderAsOverlay) {
            continue;
        }

        if (GetEffectShaderCategory(effect.shaderType) == EffectShaderCategory::SceneSample) {
            continue;
        }

        SceneRenderDrawSceneEffectRegion(state, sceneEffect, effect);
    }
}

void SceneRenderDrawOverlayEffectRegionsSelfTextureOnly(const GameState& state)
{
    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (!sceneEffect.renderAsOverlay) {
            continue;
        }

        if (GetEffectShaderCategory(effect.shaderType) == EffectShaderCategory::SceneSample) {
            continue;
        }

        SceneRenderDrawSceneEffectRegion(state, sceneEffect, effect);
    }
}
