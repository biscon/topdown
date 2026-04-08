#pragma once

#include <vector>
#include "render/SceneRenderData.h"
#include "scene/SceneData.h"
#include "adventure/AdventureData.h"

Rectangle SceneRenderGetRenderTargetSourceRect(const Texture2D& tex);
Rectangle SceneRenderGetRenderTargetDestRect(const Texture2D& tex);

float SceneRenderGetPolygonMaxY(const ScenePolygon& polygon);

void SceneRenderBeginWorldTarget(RenderTexture2D& target);
void SceneRenderEndWorldTarget();

void SceneRenderBlitRenderTargetFull(
        const RenderTexture2D& source,
        RenderTexture2D& dest);

void SceneRenderApplySceneSampleEffectsForBucket(
        const GameState& state,
        ScenePropDepthMode depthMode,
        bool overlayOnly,
        RenderTexture2D*& currentSource,
        RenderTexture2D*& currentDest);

void SceneRenderBuildDepthSortedWorldDrawItems(
        const GameState& state,
        std::vector<SceneRenderWorldDrawItem>& drawItems);

void SceneRenderDrawSceneImageLayer(const GameState& state, const SceneImageLayer& layer);

void SceneRenderDrawSceneEffectSprite(
        const GameState& state,
        const SceneEffectSpriteData& sceneEffect,
        const EffectSpriteInstance& effect);

void SceneRenderDrawSceneEffectRegion(
        const GameState& state,
        const SceneEffectRegionData& sceneEffect,
        const EffectRegionInstance& effect);

void SceneRenderDrawActor(const GameState& state, const ActorInstance& actor);

void SceneRenderDrawProp(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop);

void SceneRenderDrawBackProps(const GameState& state);
void SceneRenderDrawBackEffectSprites(const GameState& state);

void SceneRenderDrawFrontProps(const GameState& state);
void SceneRenderDrawFrontEffectSprites(const GameState& state);
void SceneRenderDrawOverlayEffectSprites(const GameState& state);

void SceneRenderDrawBackEffectRegionsSelfTextureOnly(const GameState& state);
void SceneRenderDrawFrontEffectRegionsSelfTextureOnly(const GameState& state);
void SceneRenderDrawOverlayEffectRegionsSelfTextureOnly(const GameState& state);