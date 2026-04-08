#include "render/SceneRenderInternal.h"

#include <algorithm>
#include <vector>

#include "render/SceneRender.h"
#include "render/EffectShaderRegistry.h"

Rectangle SceneRenderGetRenderTargetSourceRect(const Texture2D& tex)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(tex.width),
            -static_cast<float>(tex.height)
    };
}

Rectangle SceneRenderGetRenderTargetDestRect(const Texture2D& tex)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(tex.width),
            static_cast<float>(tex.height)
    };
}

float SceneRenderGetPolygonMaxY(const ScenePolygon& polygon)
{
    if (polygon.vertices.empty()) {
        return 0.0f;
    }

    float maxY = polygon.vertices[0].y;
    for (const Vector2& v : polygon.vertices) {
        if (v.y > maxY) {
            maxY = v.y;
        }
    }
    return maxY;
}

void SceneRenderBeginWorldTarget(RenderTexture2D& target)
{
    BeginTextureMode(target);
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

void SceneRenderEndWorldTarget()
{
    EndBlendMode();
    EndTextureMode();
}

void SceneRenderBlitRenderTargetFull(
        const RenderTexture2D& source,
        RenderTexture2D& dest)
{
    BeginTextureMode(dest);
    ClearBackground(BLACK);
    DrawTexturePro(
            source.texture,
            SceneRenderGetRenderTargetSourceRect(source.texture),
            SceneRenderGetRenderTargetDestRect(dest.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndTextureMode();
}

void SceneRenderApplySceneSampleEffectsForBucket(
        const GameState& state,
        ScenePropDepthMode depthMode,
        bool overlayOnly,
        RenderTexture2D*& currentSource,
        RenderTexture2D*& currentDest)
{
    const int effectRegionCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    std::vector<int> sortedIndices;
    sortedIndices.reserve(effectRegionCount);

    for (int i = 0; i < effectRegionCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];

        if (!effect.visible) {
            continue;
        }

        if (GetEffectShaderCategory(effect.shaderType) != EffectShaderCategory::SceneSample) {
            continue;
        }

        if (sceneEffect.depthMode != depthMode) {
            continue;
        }

        if (sceneEffect.renderAsOverlay != overlayOnly) {
            continue;
        }

        sortedIndices.push_back(i);
    }

    std::stable_sort(
            sortedIndices.begin(),
            sortedIndices.end(),
            [&](int a, int b) {
                const SceneEffectRegionData& sceneA = state.adventure.currentScene.effectRegions[a];
                const SceneEffectRegionData& sceneB = state.adventure.currentScene.effectRegions[b];
                return sceneA.sortOrder < sceneB.sortOrder;
            });

    for (int effectRegionIndex : sortedIndices) {
        if (ApplySceneSampleEffectRegionPass(state, effectRegionIndex, *currentSource, *currentDest)) {
            std::swap(currentSource, currentDest);
        }
    }
}
