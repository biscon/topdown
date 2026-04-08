#include "SceneRender.h"

#include <cmath>
#include <algorithm>
#include <sstream>
#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"
#include "scene/SceneHelpers.h"
#include "adventure/AdventureHelpers.h"
#include "render/EffectShaderRegistry.h"
#include "render/SceneRenderData.h"
#include "render/SceneRenderInternal.h"
#include "raymath.h"


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


bool ApplySceneSampleEffectRegionPass(
        const GameState& state,
        int effectRegionIndex,
        const RenderTexture2D& sourceTarget,
        RenderTexture2D& destTarget)
{
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.adventure.currentScene.effectRegions.size()) ||
        effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
        return false;
    }

    const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[effectRegionIndex];
    const EffectRegionInstance& effect = state.adventure.effectRegions[effectRegionIndex];

    if (!effect.visible) {
        return false;
    }

    if (GetEffectShaderCategory(effect.shaderType) != EffectShaderCategory::SceneSample) {
        return false;
    }

    const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(effect.shaderType);
    if (shaderEntry == nullptr) {
        return false;
    }

    const Vector2 cam = state.adventure.camera.position;

    const Rectangle effectBounds = sceneEffect.usePolygon
                                   ? BuildPolygonBounds(sceneEffect.polygon)
                                   : sceneEffect.worldRect;

    Vector2 regionPos{
            effectBounds.x - cam.x,
            effectBounds.y - cam.y
    };

    Vector2 regionSize{
            effectBounds.width,
            effectBounds.height
    };

    const float timeSeconds = static_cast<float>(GetTime());
    const Vector2 sceneSize{
            static_cast<float>(sourceTarget.texture.width),
            static_cast<float>(sourceTarget.texture.height)
    };

    BeginTextureMode(destTarget);
    ClearBackground(BLACK);

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
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->brightnessLoc, effect.shaderParams.brightness);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->contrastLoc, effect.shaderParams.contrast);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->saturationLoc, effect.shaderParams.saturation);

    if (shaderEntry->tintLoc >= 0) {
        const float tint[3] = {
                effect.shaderParams.tintR,
                effect.shaderParams.tintG,
                effect.shaderParams.tintB
        };
        SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
    }

    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, effect.shaderParams.softness);

    SetShaderPolygonIfValid(
            shaderEntry->shader,
            shaderEntry->usePolygonLoc,
            shaderEntry->polygonVertexCountLoc,
            shaderEntry->polygonPointsLoc,
            sceneEffect,
            cam);

    DrawTexturePro(
            sourceTarget.texture,
            SceneRenderGetRenderTargetSourceRect(sourceTarget.texture),
            SceneRenderGetRenderTargetDestRect(destTarget.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);

    EndShaderMode();
    EndTextureMode();

    return true;
}

void RenderAdventureSceneFadeOverlay(const GameState& state)
{
    const float opacity = Clamp(state.adventure.sceneFade.opacity, 0.0f, 1.0f);
    if (opacity <= 0.0001f) {
        return;
    }

    Color fadeColor = BLACK;
    fadeColor.a = static_cast<unsigned char>(std::round(255.0f * opacity));

    DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, fadeColor);
}

void RenderAdventureSceneComposited(
        const GameState& state,
        RenderTexture2D& worldTarget,
        RenderTexture2D& tempTarget)
{
    if (!state.adventure.currentScene.loaded) {
        BeginTextureMode(worldTarget);
        ClearBackground(BLACK);
        EndTextureMode();
        return;
    }

    RenderTexture2D* currentSource = &worldTarget;
    RenderTexture2D* currentDest = &tempTarget;

    SceneRenderBeginWorldTarget(*currentSource);
    ClearBackground(BLACK);

    for (const SceneImageLayer& layer : state.adventure.currentScene.backgroundLayers) {
        SceneRenderDrawSceneImageLayer(state, layer);
    }

    SceneRenderDrawBackProps(state);
    SceneRenderDrawBackEffectSprites(state);
    SceneRenderDrawBackEffectRegionsSelfTextureOnly(state);

    SceneRenderEndWorldTarget();

    SceneRenderApplySceneSampleEffectsForBucket(
            state,
            ScenePropDepthMode::Back,
            false,
            currentSource,
            currentDest);

    std::vector<SceneRenderWorldDrawItem> drawItems;
    SceneRenderBuildDepthSortedWorldDrawItems(state, drawItems);

    SceneRenderBeginWorldTarget(*currentSource);

    for (const SceneRenderWorldDrawItem& item : drawItems) {
        switch (item.type) {
            case SceneRenderWorldDrawItemType::Actor:
                if (item.actorIndex >= 0 &&
                    item.actorIndex < static_cast<int>(state.adventure.actors.size())) {
                    SceneRenderDrawActor(state, state.adventure.actors[item.actorIndex]);
                }
                break;

            case SceneRenderWorldDrawItemType::Prop:
                if (item.propIndex >= 0 &&
                    item.propIndex < static_cast<int>(state.adventure.currentScene.props.size()) &&
                    item.propIndex < static_cast<int>(state.adventure.props.size())) {
                    SceneRenderDrawProp(
                            state,
                            state.adventure.currentScene.props[item.propIndex],
                            state.adventure.props[item.propIndex]);
                }
                break;

            case SceneRenderWorldDrawItemType::EffectSprite:
                if (item.effectIndex >= 0 &&
                    item.effectIndex < static_cast<int>(state.adventure.currentScene.effectSprites.size()) &&
                    item.effectIndex < static_cast<int>(state.adventure.effectSprites.size())) {
                    SceneRenderDrawSceneEffectSprite(
                            state,
                            state.adventure.currentScene.effectSprites[item.effectIndex],
                            state.adventure.effectSprites[item.effectIndex]);
                }
                break;

            case SceneRenderWorldDrawItemType::EffectRegion:
                if (item.effectRegionIndex >= 0 &&
                    item.effectRegionIndex < static_cast<int>(state.adventure.currentScene.effectRegions.size()) &&
                    item.effectRegionIndex < static_cast<int>(state.adventure.effectRegions.size())) {
                    const EffectRegionInstance& effect =
                            state.adventure.effectRegions[item.effectRegionIndex];

                    if (GetEffectShaderCategory(effect.shaderType) == EffectShaderCategory::SceneSample) {
                        SceneRenderEndWorldTarget();

                        if (ApplySceneSampleEffectRegionPass(
                                state,
                                item.effectRegionIndex,
                                *currentSource,
                                *currentDest)) {
                            std::swap(currentSource, currentDest);
                        }

                        SceneRenderBeginWorldTarget(*currentSource);
                    } else {
                        SceneRenderDrawSceneEffectRegion(
                                state,
                                state.adventure.currentScene.effectRegions[item.effectRegionIndex],
                                state.adventure.effectRegions[item.effectRegionIndex]);
                    }
                }
                break;

            default:
                break;
        }
    }

    SceneRenderEndWorldTarget();

    SceneRenderBeginWorldTarget(*currentSource);
    SceneRenderDrawFrontProps(state);
    SceneRenderDrawFrontEffectSprites(state);
    SceneRenderDrawFrontEffectRegionsSelfTextureOnly(state);
    SceneRenderEndWorldTarget();

    SceneRenderApplySceneSampleEffectsForBucket(
            state,
            ScenePropDepthMode::Front,
            false,
            currentSource,
            currentDest);

    SceneRenderBeginWorldTarget(*currentSource);

    for (const SceneImageLayer& layer : state.adventure.currentScene.foregroundLayers) {
        SceneRenderDrawSceneImageLayer(state, layer);
    }

    SceneRenderDrawOverlayEffectSprites(state);
    SceneRenderDrawOverlayEffectRegionsSelfTextureOnly(state);

    SceneRenderEndWorldTarget();

    SceneRenderApplySceneSampleEffectsForBucket(
            state,
            ScenePropDepthMode::Front,
            true,
            currentSource,
            currentDest);

    if (currentSource != &worldTarget) {
        SceneRenderBlitRenderTargetFull(*currentSource, worldTarget);
    }
}
