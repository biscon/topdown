#include "topdown/LevelRender.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "data/GameState.h"
#include "raylib.h"
#include "raymath.h"
#include "resources/TextureAsset.h"
#include "render/EffectShaderRegistry.h"
#include "topdown/CharacterRender.h"
#include "TopdownHelpers.h"
#include "rlgl.h"
#include "BloodRenderTarget.h"
#include "TopdownPlayerVignette.h"
#include "audio/Audio.h"
#include "input/Input.h"
#include "LevelWindows.h"
#include "topdown/LevelProps.h"
#include "ui/NarrationPopups.h"

static Rectangle GetRenderTargetSourceRect(const Texture2D& tex)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(tex.width),
            -static_cast<float>(tex.height)
    };
}

static Rectangle GetRenderTargetDestRect(const Texture2D& tex)
{
    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(tex.width),
            static_cast<float>(tex.height)
    };
}


static unsigned char MultiplyU8(unsigned char a, unsigned char b)
{
    const int value = static_cast<int>(a) * static_cast<int>(b);
    return static_cast<unsigned char>((value + 127) / 255);
}

static void SetShaderFloatIfValid(const Shader& shader, int loc, float value)
{
    if (loc < 0) {
        return;
    }

    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_FLOAT);
}

static void SetShaderIntIfValid(const Shader& shader, int loc, int value)
{
    if (loc < 0) {
        return;
    }

    SetShaderValue(shader, loc, &value, SHADER_UNIFORM_INT);
}

static void SetShaderVec2IfValid(const Shader& shader, int loc, Vector2 value)
{
    if (loc < 0) {
        return;
    }

    const float v[2] = { value.x, value.y };
    SetShaderValue(shader, loc, v, SHADER_UNIFORM_VEC2);
}

static void SetShaderPolygonIfValid(
        const Shader& shader,
        int usePolygonLoc,
        int polygonVertexCountLoc,
        int polygonPointsLoc,
        const TopdownAuthoredEffectRegion& effect,
        const Vector2& cam)
{
    const int usePolygon = effect.usePolygon ? 1 : 0;
    SetShaderIntIfValid(shader, usePolygonLoc, usePolygon);

    if (!effect.usePolygon) {
        SetShaderIntIfValid(shader, polygonVertexCountLoc, 0);
        return;
    }

    const int vertexCount = static_cast<int>(effect.polygon.size());
    SetShaderIntIfValid(shader, polygonVertexCountLoc, vertexCount);

    if (polygonPointsLoc < 0 || vertexCount <= 0) {
        return;
    }

    float points[32 * 2] = {};
    for (int i = 0; i < vertexCount && i < 32; ++i) {
        points[i * 2 + 0] = effect.polygon[i].x - cam.x;
        points[i * 2 + 1] = effect.polygon[i].y - cam.y;
    }

    SetShaderValueV(shader, polygonPointsLoc, points, SHADER_UNIFORM_VEC2, vertexCount);
}

static void SetShaderOcclusionPolygonIfValid(
        const Shader& shader,
        int useOcclusionPolygonLoc,
        int occlusionPolygonVertexCountLoc,
        int occlusionPolygonPointsLoc,
        const TopdownRuntimeEffectRegion& runtime,
        const Vector2& cam)
{
    const int usePolygon =
            runtime.hasWallOcclusionPolygon &&
            runtime.wallOcclusionPolygon.size() >= 3
            ? 1
            : 0;

    SetShaderIntIfValid(shader, useOcclusionPolygonLoc, usePolygon);

    if (!usePolygon) {
        SetShaderIntIfValid(shader, occlusionPolygonVertexCountLoc, 0);
        return;
    }

    const int vertexCount = static_cast<int>(runtime.wallOcclusionPolygon.size());
    SetShaderIntIfValid(shader, occlusionPolygonVertexCountLoc, vertexCount);

    if (occlusionPolygonPointsLoc < 0 || vertexCount <= 0) {
        return;
    }

    float points[256 * 2] = {};
    for (int i = 0; i < vertexCount && i < 256; ++i) {
        points[i * 2 + 0] = runtime.wallOcclusionPolygon[i].x - cam.x;
        points[i * 2 + 1] = runtime.wallOcclusionPolygon[i].y - cam.y;
    }

    SetShaderValueV(shader, occlusionPolygonPointsLoc, points, SHADER_UNIFORM_VEC2, vertexCount);
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

static void BeginWorldTarget(RenderTexture2D& target)
{
    BeginTextureMode(target);
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void EndWorldTarget()
{
    EndBlendMode();
    EndTextureMode();
}

static void BlitRenderTargetFull(
        const RenderTexture2D& source,
        RenderTexture2D& dest)
{
    BeginTextureMode(dest);
    ClearBackground(BLACK);
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(
            source.texture,
            GetRenderTargetSourceRect(source.texture),
            GetRenderTargetDestRect(dest.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);
    EndBlendMode();
    EndTextureMode();
}

static Rectangle BuildScaledCenteredImageLayerDestRect(
        const GameState& state,
        const TopdownRuntimeImageLayer& layer,
        const TextureResource& texRes)
{
    const Vector2 screenPos = TopdownWorldToScreen(state, layer.position);

    const float baseWidth =
            layer.imageSize.x > 0.0f ? layer.imageSize.x
                                     : static_cast<float>(texRes.texture.width);

    const float baseHeight =
            layer.imageSize.y > 0.0f ? layer.imageSize.y
                                     : static_cast<float>(texRes.texture.height);

    const float safeScale = layer.scale > 0.0f ? layer.scale : 1.0f;

    const float scaledWidth = baseWidth * safeScale;
    const float scaledHeight = baseHeight * safeScale;

    Rectangle dst{};
    dst.x = std::round(screenPos.x - (scaledWidth - baseWidth) * 0.5f);
    dst.y = std::round(screenPos.y - (scaledHeight - baseHeight) * 0.5f);
    dst.width = std::round(scaledWidth);
    dst.height = std::round(scaledHeight);
    return dst;
}

static void DrawImageLayer(const GameState& state, const TopdownRuntimeImageLayer& layer)
{
    if (!layer.visible || layer.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, layer.textureHandle);
    if (texRes == nullptr || !texRes->loaded || texRes->texture.id == 0) {
        return;
    }

    Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(texRes->texture.width),
            static_cast<float>(texRes->texture.height)
    };

    const Rectangle dst = BuildScaledCenteredImageLayerDestRect(state, layer, *texRes);

    const EffectShaderCategory shaderCategory = GetEffectShaderCategory(layer.shaderType);

    Color drawColor = layer.tint;
    drawColor.a = MultiplyU8(
            drawColor.a,
            static_cast<unsigned char>(std::round(255.0f * Clamp(layer.opacity, 0.0f, 1.0f))));

    EndBlendMode();
    BeginBlendMode(GetRaylibBlendMode(layer.blendMode));

    if (shaderCategory == EffectShaderCategory::SelfTexture) {
        const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(layer.shaderType);
        if (shaderEntry != nullptr) {
            const float timeSeconds = static_cast<float>(GetTime());

            BeginShaderMode(shaderEntry->shader);

            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, layer.shaderParams.scrollSpeed);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, layer.shaderParams.uvScale);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, layer.shaderParams.distortionAmount);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, layer.shaderParams.noiseScrollSpeed);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, layer.shaderParams.intensity);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, layer.shaderParams.phaseOffset);

            const Vector2 sceneSize{
                    static_cast<float>(INTERNAL_WIDTH),
                    static_cast<float>(INTERNAL_HEIGHT)
            };
            const Vector2 regionPos{ dst.x, dst.y };
            const Vector2 regionSize{ dst.width, dst.height };

            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->sceneSizeLoc, sceneSize);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionPosLoc, regionPos);
            SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionSizeLoc, regionSize);
            SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, layer.shaderParams.softness);

            SetShaderIntIfValid(shaderEntry->shader, shaderEntry->usePolygonLoc, 0);
            SetShaderIntIfValid(shaderEntry->shader, shaderEntry->polygonVertexCountLoc, 0);

            SetShaderIntIfValid(shaderEntry->shader, shaderEntry->useOcclusionPolygonLoc, 0);
            SetShaderIntIfValid(shaderEntry->shader, shaderEntry->occlusionPolygonVertexCountLoc, 0);

            if (shaderEntry->tintLoc >= 0) {
                const float tint[3] = {
                        layer.shaderParams.tintR,
                        layer.shaderParams.tintG,
                        layer.shaderParams.tintB
                };
                SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
            }

            DrawTexturePro(texRes->texture, src, dst, Vector2{}, 0.0f, drawColor);

            EndShaderMode();
        } else {
            DrawTexturePro(texRes->texture, src, dst, Vector2{}, 0.0f, drawColor);
        }
    } else {
        DrawTexturePro(texRes->texture, src, dst, Vector2{}, 0.0f, drawColor);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void DrawBottomLayers(const GameState& state)
{
    for (const TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.bottomLayers) {
        DrawImageLayer(state, layer);
    }
}

static void DrawTopLayers(const GameState& state)
{
    for (const TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.topLayers) {
        DrawImageLayer(state, layer);
    }
}

static bool ApplySceneSampleTopdownEffectRegionPass(
        const GameState& state,
        int effectRegionIndex,
        const RenderTexture2D& sourceTarget,
        RenderTexture2D& destTarget)
{
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.topdown.authored.effectRegions.size()) ||
        effectRegionIndex >= static_cast<int>(state.topdown.runtime.render.effectRegions.size())) {
        return false;
    }

    const TopdownAuthoredEffectRegion& authored =
            state.topdown.authored.effectRegions[effectRegionIndex];
    const TopdownRuntimeEffectRegion& runtime =
            state.topdown.runtime.render.effectRegions[effectRegionIndex];

    if (!runtime.visible) {
        return false;
    }

    if (GetEffectShaderCategory(runtime.shaderType) != EffectShaderCategory::SceneSample) {
        return false;
    }

    const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(runtime.shaderType);
    if (shaderEntry == nullptr) {
        return false;
    }

    const Vector2 cam = state.topdown.runtime.camera.position;

    Vector2 regionPos{
            authored.worldRect.x - cam.x,
            authored.worldRect.y - cam.y
    };

    Vector2 regionSize{
            authored.worldRect.width,
            authored.worldRect.height
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
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, runtime.shaderParams.scrollSpeed);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, runtime.shaderParams.uvScale);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, runtime.shaderParams.distortionAmount);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, runtime.shaderParams.noiseScrollSpeed);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, runtime.shaderParams.intensity);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, runtime.shaderParams.phaseOffset);

    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->sceneSizeLoc, sceneSize);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionPosLoc, regionPos);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionSizeLoc, regionSize);

    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->brightnessLoc, runtime.shaderParams.brightness);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->contrastLoc, runtime.shaderParams.contrast);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->saturationLoc, runtime.shaderParams.saturation);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, runtime.shaderParams.softness);

    if (shaderEntry->tintLoc >= 0) {
        const float tint[3] = {
                runtime.shaderParams.tintR,
                runtime.shaderParams.tintG,
                runtime.shaderParams.tintB
        };
        SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
    }

    SetShaderPolygonIfValid(
            shaderEntry->shader,
            shaderEntry->usePolygonLoc,
            shaderEntry->polygonVertexCountLoc,
            shaderEntry->polygonPointsLoc,
            authored,
            cam);

    SetShaderOcclusionPolygonIfValid(
            shaderEntry->shader,
            shaderEntry->useOcclusionPolygonLoc,
            shaderEntry->occlusionPolygonVertexCountLoc,
            shaderEntry->occlusionPolygonPointsLoc,
            runtime,
            cam);

    DrawTexturePro(
            sourceTarget.texture,
            GetRenderTargetSourceRect(sourceTarget.texture),
            GetRenderTargetDestRect(destTarget.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);

    EndShaderMode();
    EndTextureMode();

    return true;
}

static void DrawSelfTextureTopdownEffectRegion(
        const GameState& state,
        const TopdownAuthoredEffectRegion& authored,
        const TopdownRuntimeEffectRegion& runtime)
{
    if (!runtime.visible) {
        return;
    }

    if (GetEffectShaderCategory(runtime.shaderType) != EffectShaderCategory::SelfTexture) {
        return;
    }

    if (authored.textureHandle < 0) {
        return;
    }

    const TextureResource* texRes = FindTextureResource(state.resources, authored.textureHandle);
    if (texRes == nullptr || !texRes->loaded || texRes->texture.id == 0) {
        return;
    }

    const EffectShaderEntry* shaderEntry = FindEffectShaderEntry(runtime.shaderType);
    if (shaderEntry == nullptr) {
        return;
    }

    const Vector2 cam = state.topdown.runtime.camera.position;
    const float timeSeconds = static_cast<float>(GetTime());
    const Vector2 sceneSize{ 1920.0f, 1080.0f };

    Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(texRes->texture.width),
            static_cast<float>(texRes->texture.height)
    };

    Rectangle dst{
            std::round(authored.worldRect.x - cam.x),
            std::round(authored.worldRect.y - cam.y),
            std::round(authored.worldRect.width),
            std::round(authored.worldRect.height)
    };

    Vector2 regionPos{ dst.x, dst.y };
    Vector2 regionSize{ dst.width, dst.height };

    Color drawColor = WHITE;
    drawColor.a = static_cast<unsigned char>(std::round(255.0f * Clamp(runtime.opacity, 0.0f, 1.0f)));

    EndBlendMode();
    BeginBlendMode(GetRaylibBlendMode(authored.blendMode));

    BeginShaderMode(shaderEntry->shader);

    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->timeLoc, timeSeconds);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->scrollSpeedLoc, runtime.shaderParams.scrollSpeed);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->uvScaleLoc, runtime.shaderParams.uvScale);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->distortionAmountLoc, runtime.shaderParams.distortionAmount);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->noiseScrollSpeedLoc, runtime.shaderParams.noiseScrollSpeed);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->intensityLoc, runtime.shaderParams.intensity);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->phaseOffsetLoc, runtime.shaderParams.phaseOffset);

    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->sceneSizeLoc, sceneSize);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionPosLoc, regionPos);
    SetShaderVec2IfValid(shaderEntry->shader, shaderEntry->regionSizeLoc, regionSize);
    SetShaderFloatIfValid(shaderEntry->shader, shaderEntry->softnessLoc, runtime.shaderParams.softness);

    if (shaderEntry->tintLoc >= 0) {
        const float tint[3] = {
                runtime.shaderParams.tintR,
                runtime.shaderParams.tintG,
                runtime.shaderParams.tintB
        };
        SetShaderValue(shaderEntry->shader, shaderEntry->tintLoc, tint, SHADER_UNIFORM_VEC3);
    }

    SetShaderPolygonIfValid(
            shaderEntry->shader,
            shaderEntry->usePolygonLoc,
            shaderEntry->polygonVertexCountLoc,
            shaderEntry->polygonPointsLoc,
            authored,
            cam);

    SetShaderOcclusionPolygonIfValid(
            shaderEntry->shader,
            shaderEntry->useOcclusionPolygonLoc,
            shaderEntry->occlusionPolygonVertexCountLoc,
            shaderEntry->occlusionPolygonPointsLoc,
            runtime,
            cam);

    DrawTexturePro(texRes->texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, drawColor);

    EndShaderMode();

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void ApplyTopdownEffectRegionBucket(
        const GameState& state,
        const std::vector<int>& bucket,
        RenderTexture2D*& currentSource,
        RenderTexture2D*& currentDest)
{
    for (int effectRegionIndex : bucket) {
        if (effectRegionIndex < 0 ||
            effectRegionIndex >= static_cast<int>(state.topdown.authored.effectRegions.size()) ||
            effectRegionIndex >= static_cast<int>(state.topdown.runtime.render.effectRegions.size())) {
            continue;
        }

        const TopdownAuthoredEffectRegion& authored =
                state.topdown.authored.effectRegions[effectRegionIndex];
        const TopdownRuntimeEffectRegion& runtime =
                state.topdown.runtime.render.effectRegions[effectRegionIndex];

        if (!runtime.visible) {
            continue;
        }

        const EffectShaderCategory category = GetEffectShaderCategory(runtime.shaderType);

        if (category == EffectShaderCategory::SceneSample) {
            if (ApplySceneSampleTopdownEffectRegionPass(
                    state,
                    effectRegionIndex,
                    *currentSource,
                    *currentDest)) {
                std::swap(currentSource, currentDest);
            }
        } else if (category == EffectShaderCategory::SelfTexture) {
            BeginWorldTarget(*currentSource);
            DrawSelfTextureTopdownEffectRegion(state, authored, runtime);
            EndWorldTarget();
        }
    }
}

static float Hash01FromSeed(unsigned int seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    return static_cast<float>(seed & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

static void DrawSingleBloodDecalBlob(
        Vector2 screenPos,
        float radius,
        float rotationRadians,
        float opacity,
        unsigned int variantSeed)
{
    const unsigned char baseAlpha =
            static_cast<unsigned char>(std::round(255.0f * Clamp(opacity, 0.0f, 1.0f)));

// Brighter, more saturated blood
    const Color mainColor{
            170,   // was ~90
            24,
            24,
            static_cast<unsigned char>(std::round(baseAlpha * 0.85f))
    };

    const Color darkColor{
            110,
            12,
            12,
            static_cast<unsigned char>(std::round(baseAlpha * 0.95f))
    };

    const Color richColor{
            220,   // highlight / "wet" sheen
            40,
            40,
            static_cast<unsigned char>(std::round(baseAlpha * 0.45f))
    };

    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlTranslatef(std::round(screenPos.x), std::round(screenPos.y), 0.0f);
    rlRotatef(rotationRadians * RAD2DEG, 0.0f, 0.0f, 1.0f);

    const float sx = 1.0f + (Hash01FromSeed(variantSeed ^ 0x1234u) - 0.5f) * 0.35f;
    const float sy = 0.82f + (Hash01FromSeed(variantSeed ^ 0x5678u) - 0.5f) * 0.22f;
    rlScalef(sx, sy, 1.0f);

    DrawCircleV(Vector2{0.0f, 0.0f}, radius, mainColor);

    for (int i = 0; i < 3; ++i) {
        const unsigned int seedA = variantSeed ^ static_cast<unsigned int>(0x1000 + i * 977);
        const unsigned int seedB = variantSeed ^ static_cast<unsigned int>(0x2000 + i * 619);
        const unsigned int seedC = variantSeed ^ static_cast<unsigned int>(0x3000 + i * 383);

        const float angle = Hash01FromSeed(seedA) * 2.0f * PI;
        const float dist = radius * (0.28f + Hash01FromSeed(seedB) * 0.45f);
        const float subRadius = radius * (0.22f + Hash01FromSeed(seedC) * 0.22f);

        const Vector2 offset{
                std::cos(angle) * dist,
                std::sin(angle) * dist
        };

        DrawCircleV(offset, subRadius, darkColor);
    }

    DrawCircleV(
            Vector2{
                    radius * 0.18f,
                    -radius * 0.10f
            },
            radius * 0.42f,
            richColor);

    rlPopMatrix();
    rlDrawRenderBatchActive();
}

static const TopdownBloodStamp* FindBloodStampForDecal(
        const TopdownBloodStampLibrary& library,
        const TopdownBloodDecal& decal)
{
    if (!decal.useGeneratedStamp || decal.stampIndex < 0) {
        return nullptr;
    }

    if (decal.preferStreakStamp) {
        if (decal.stampIndex < 0 ||
            decal.stampIndex >= static_cast<int>(library.streaks.size())) {
            return nullptr;
        }

        return &library.streaks[decal.stampIndex];
    }

    if (decal.stampIndex < 0 ||
        decal.stampIndex >= static_cast<int>(library.splats.size())) {
        return nullptr;
    }

    return &library.splats[decal.stampIndex];
}

static void DrawGeneratedBloodStamp(
        const GameState& state,
        const TopdownBloodDecal& decal,
        const TopdownBloodStamp& stamp)
{
    if (!stamp.loaded || stamp.texture.id == 0) {
        return;
    }

    const Vector2 screenPos = TopdownWorldToScreen(state, decal.position);

    const float baseSize = decal.radius * 2.0f;
    const float stretch = std::max(0.25f, decal.stretch);

    Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(stamp.texture.width),
            static_cast<float>(stamp.texture.height)
    };

    Rectangle dst{};
    dst.x = std::round(screenPos.x);
    dst.y = std::round(screenPos.y);
    dst.width = std::round(baseSize * stretch);
    dst.height = std::round(baseSize);

    Vector2 origin{
            dst.width * 0.5f,
            dst.height * 0.5f
    };

    Color tint{
            170,
            24,
            24,
            static_cast<unsigned char>(std::round(255.0f * Clamp(decal.opacity, 0.0f, 1.0f)))
    };

    DrawTexturePro(
            stamp.texture,
            src,
            dst,
            origin,
            decal.rotationRadians * RAD2DEG,
            tint);
}

static const TopdownBloodStamp* FindBloodParticleStamp(
        const TopdownBloodStampLibrary& library,
        const TopdownBloodImpactParticle& particle)
{
    if (!particle.useGeneratedStamp || particle.stampIndex < 0) {
        return nullptr;
    }

    if (particle.stampIndex >= static_cast<int>(library.particles.size())) {
        return nullptr;
    }

    return &library.particles[particle.stampIndex];
}


static void DrawBloodImpactParticles(const GameState& state)
{
    const std::vector<TopdownBloodImpactParticle>& particles =
            state.topdown.runtime.render.bloodImpactParticles;

    if (particles.empty()) {
        return;
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);

    for (const TopdownBloodImpactParticle& particle : particles) {
        if (!particle.active || particle.alpha <= 0.0f || particle.size <= 0.0f) {
            continue;
        }

        const TopdownBloodStamp* stamp =
                FindBloodParticleStamp(state.topdown.bloodStampLibrary, particle);

        if (stamp == nullptr || !stamp->loaded || stamp->texture.id == 0) {
            continue;
        }

        const Vector2 screenPos = TopdownWorldToScreen(state, particle.position);

        // IMPORTANT:
        // particle.size was originally tuned for primitive circles.
        // Generated particle stamps have lots of soft alpha around the core,
        // so they need a much larger draw size to read properly.
        const float baseSize = std::max(12.0f, particle.size * 8.5f);
        const float stretch = std::max(0.45f, particle.stretch);

        Rectangle src{
                0.0f,
                0.0f,
                static_cast<float>(stamp->texture.width),
                static_cast<float>(stamp->texture.height)
        };

        Rectangle dst{};
        dst.x = std::round(screenPos.x);
        dst.y = std::round(screenPos.y);
        dst.width = std::round(baseSize * stretch);
        dst.height = std::round(baseSize);

        Vector2 origin{
                dst.width * 0.5f,
                dst.height * 0.5f
        };

        const float alpha01 = Clamp(particle.alpha, 0.0f, 1.0f);
        const unsigned char a =
                static_cast<unsigned char>(std::round(255.0f * alpha01));

        // Darken textured blood particles so they match the decal blood better.
        static constexpr float kBloodParticleBrightness = 0.50f;

        const float r01 = (particle.color.r / 255.0f) * kBloodParticleBrightness;
        const float g01 = (particle.color.g / 255.0f) * kBloodParticleBrightness;
        const float b01 = (particle.color.b / 255.0f) * kBloodParticleBrightness;

        Color tint{
                static_cast<unsigned char>(std::round(r01 * a)),
                static_cast<unsigned char>(std::round(g01 * a)),
                static_cast<unsigned char>(std::round(b01 * a)),
                a
        };

        DrawTexturePro(
                stamp->texture,
                src,
                dst,
                origin,
                particle.rotationRadians * RAD2DEG,
                tint);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void DrawTracerEffects(const GameState& state)
{
    //return;
    const std::vector<TopdownTracerEffect>& tracers = state.topdown.runtime.render.tracers;

    if (tracers.empty()) {
        return;
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ADDITIVE);

    for (const TopdownTracerEffect& tracer : tracers) {
        if (!tracer.active || tracer.lifetimeMs <= 0.0f) {
            continue;
        }

        const float alpha01 = 1.0f - Clamp(tracer.ageMs / tracer.lifetimeMs, 0.0f, 1.0f);

        const Vector2 a = TopdownWorldToScreen(state, tracer.start);
        const Vector2 b = TopdownWorldToScreen(state, tracer.end);

        Color outerColor{};
        Color innerColor{};
        Color headColor{};

        switch (tracer.style) {
            case TopdownTracerStyle::Handgun:
                outerColor = Color{255, 210, 110, static_cast<unsigned char>(18.0f * alpha01)};
                innerColor = Color{255, 236, 190, static_cast<unsigned char>(60.0f * alpha01)};
                headColor  = Color{255, 245, 220, static_cast<unsigned char>(90.0f * alpha01)};
                break;

            case TopdownTracerStyle::Shotgun:
                outerColor = Color{255, 170, 80, static_cast<unsigned char>(22.0f * alpha01)};
                innerColor = Color{255, 220, 160, static_cast<unsigned char>(72.0f * alpha01)};
                headColor  = Color{255, 235, 205, static_cast<unsigned char>(105.0f * alpha01)};
                break;

            case TopdownTracerStyle::Rifle:
                outerColor = Color{255, 225, 110, static_cast<unsigned char>(18.0f * alpha01)};
                innerColor = Color{255, 244, 200, static_cast<unsigned char>(60.0f * alpha01)};
                headColor  = Color{255, 250, 225, static_cast<unsigned char>(90.0f * alpha01)};
                break;

            case TopdownTracerStyle::None:
            default:
                outerColor = Color{255, 255, 255, static_cast<unsigned char>(12.0f * alpha01)};
                innerColor = Color{255, 255, 255, static_cast<unsigned char>(42.0f * alpha01)};
                headColor  = Color{255, 255, 255, static_cast<unsigned char>(64.0f * alpha01)};
                break;
        }

        DrawLineEx(a, b, tracer.thickness * 1.35f, outerColor);
        DrawLineEx(a, b, tracer.thickness * 0.72f, innerColor);
        DrawCircleV(b, tracer.thickness * 0.42f, headColor);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void DrawWallImpactParticles(const GameState& state)
{
    const std::vector<TopdownWallImpactParticle>& particles =
            state.topdown.runtime.render.wallImpactParticles;

    if (particles.empty()) {
        return;
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA);

    for (const TopdownWallImpactParticle& particle : particles) {
        if (!particle.active || particle.alpha == 0) {
            continue;
        }

        const Vector2 p = TopdownWorldToScreen(state, particle.position);

        Color color = particle.color;
        color.a = particle.alpha * 0.15;

        DrawCircleV(
                p,
                particle.size,
                color);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void DrawMuzzleSmokeParticles(const GameState& state)
{
    const std::vector<TopdownMuzzleSmokeParticle>& particles =
            state.topdown.runtime.render.muzzleSmokeParticles;

    if (particles.empty()) {
        return;
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA);

    for (const TopdownMuzzleSmokeParticle& particle : particles) {
        if (!particle.active || particle.alpha <= 0.0f || particle.size <= 0.0f) {
            continue;
        }

        const Vector2 p = TopdownWorldToScreen(state, particle.position);

        Color color = particle.color;
        color.a = static_cast<unsigned char>(
                std::round(255.0f * Clamp(particle.alpha, 0.0f, 1.0f)));

        // Soft puff: layered circles instead of one hard dot.
        Color outer = color;
        outer.a = static_cast<unsigned char>(outer.a * 0.28f);

        Color mid = color;
        mid.a = static_cast<unsigned char>(mid.a * 0.55f);

        //outer.a *= 0.2f;
        //mid.a   *= 0.4f;

        DrawCircleV(p, particle.size * 1.45f, outer);
        DrawCircleV(p, particle.size * 0.95f, mid);
        DrawCircleV(p, particle.size * 0.52f, color);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void RLColor(Color c)
{
    rlColor4ub(c.r, c.g, c.b, c.a);
}

static void RLVertex(Vector2 p)
{
    rlVertex2f(p.x, p.y);
}

static void DrawGradientTriangle(
        Vector2 a, Color colorA,
        Vector2 b, Color colorB,
        Vector2 c, Color colorC)
{
    rlBegin(RL_TRIANGLES);
    RLColor(colorA); RLVertex(a);
    RLColor(colorB); RLVertex(b);
    RLColor(colorC); RLVertex(c);

    RLColor(colorA); RLVertex(a);
    RLColor(colorC); RLVertex(c);
    RLColor(colorB); RLVertex(b);
    rlEnd();
}


static void DrawMuzzleFlashEffects(const GameState& state)
{
    const std::vector<TopdownMuzzleFlashEffect>& flashes =
            state.topdown.runtime.render.muzzleFlashes;

    if (flashes.empty()) {
        return;
    }

    EndBlendMode();
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ADDITIVE);
    rlSetTexture(0);

    for (const TopdownMuzzleFlashEffect& flash : flashes) {
        if (!flash.active || flash.lifetimeMs <= 0.0f) {
            continue;
        }

        const float life01 =
                1.0f - Clamp(flash.ageMs / flash.lifetimeMs, 0.0f, 1.0f);

        Vector2 dir = TopdownNormalizeOrZero(flash.direction);
        if (TopdownLengthSqr(dir) <= 0.000001f) {
            dir = Vector2{1.0f, 0.0f};
        }

        const Vector2 right{ -dir.y, dir.x };

        // Nudge the whole flash a bit back toward the gun.
        const Vector2 flashOrigin =
                TopdownAdd(flash.position, TopdownMul(dir, -10.0f));

        // Shape:
        // small neck near muzzle -> wider belly -> tapered tip
        const float halfNeckWidth = std::max(0.8f, flash.sideWidth * 0.10f);
        const float halfMidWidth  = std::max(1.4f, flash.sideWidth * 0.34f);
        const float halfTipWidth  = std::max(0.4f, flash.sideWidth * 0.05f);

        const float neckDist = std::max(1.0f, flash.forwardLength * 0.10f);
        const float midDist  = std::max(neckDist + 1.0f, flash.forwardLength * 0.38f);
        const float tipDist  = std::max(midDist + 1.0f, flash.forwardLength);

        const Vector2 neckCenterW = TopdownAdd(flashOrigin, TopdownMul(dir, neckDist));
        const Vector2 midCenterW  = TopdownAdd(flashOrigin, TopdownMul(dir, midDist));
        const Vector2 tipCenterW  = TopdownAdd(flashOrigin, TopdownMul(dir, tipDist));

        // Outer flame
        const Vector2 outerNeckLeftW  = TopdownAdd(neckCenterW, TopdownMul(right, -halfNeckWidth));
        const Vector2 outerNeckRightW = TopdownAdd(neckCenterW, TopdownMul(right,  halfNeckWidth));

        const Vector2 outerMidLeftW   = TopdownAdd(midCenterW, TopdownMul(right, -halfMidWidth));
        const Vector2 outerMidRightW  = TopdownAdd(midCenterW, TopdownMul(right,  halfMidWidth));

        const Vector2 outerTipLeftW   = TopdownAdd(tipCenterW, TopdownMul(right, -halfTipWidth));
        const Vector2 outerTipRightW  = TopdownAdd(tipCenterW, TopdownMul(right,  halfTipWidth));

        // Inner hot core
        const Vector2 innerNeckCenterW = TopdownAdd(flashOrigin, TopdownMul(dir, neckDist * 0.92f));
        const Vector2 innerMidCenterW  = TopdownAdd(flashOrigin, TopdownMul(dir, midDist * 0.72f));
        const Vector2 innerTipCenterW  = TopdownAdd(flashOrigin, TopdownMul(dir, tipDist * 0.54f));

        const float innerHalfNeckWidth = std::max(0.5f, halfNeckWidth * 1.65f);
        const float innerHalfMidWidth  = std::max(0.7f, halfMidWidth * 0.52f);

        const Vector2 innerNeckLeftW   = TopdownAdd(innerNeckCenterW, TopdownMul(right, -innerHalfNeckWidth));
        const Vector2 innerNeckRightW  = TopdownAdd(innerNeckCenterW, TopdownMul(right,  innerHalfNeckWidth));

        const Vector2 innerMidLeftW    = TopdownAdd(innerMidCenterW, TopdownMul(right, -innerHalfMidWidth));
        const Vector2 innerMidRightW   = TopdownAdd(innerMidCenterW, TopdownMul(right,  innerHalfMidWidth));

        // Screen space
        const Vector2 outerNeckLeft   = TopdownWorldToScreen(state, outerNeckLeftW);
        const Vector2 outerNeckRight  = TopdownWorldToScreen(state, outerNeckRightW);
        const Vector2 outerMidLeft    = TopdownWorldToScreen(state, outerMidLeftW);
        const Vector2 outerMidRight   = TopdownWorldToScreen(state, outerMidRightW);
        const Vector2 outerTipLeft    = TopdownWorldToScreen(state, outerTipLeftW);
        const Vector2 outerTipRight   = TopdownWorldToScreen(state, outerTipRightW);
        const Vector2 outerMidCenter  = TopdownWorldToScreen(state, midCenterW);

        const Vector2 innerNeckLeft   = TopdownWorldToScreen(state, innerNeckLeftW);
        const Vector2 innerNeckRight  = TopdownWorldToScreen(state, innerNeckRightW);
        const Vector2 innerMidLeft    = TopdownWorldToScreen(state, innerMidLeftW);
        const Vector2 innerMidRight   = TopdownWorldToScreen(state, innerMidRightW);
        const Vector2 innerMidCenter  = TopdownWorldToScreen(state, innerMidCenterW);
        const Vector2 innerTipCenter  = TopdownWorldToScreen(state, innerTipCenterW);

        const Color whiteHot{
                255, 245, 220,
                static_cast<unsigned char>(std::round(180.0f * life01))
        };

        const Color yellowHot{
                255, 210, 95,
                static_cast<unsigned char>(std::round(150.0f * life01))
        };

        const Color redHot{
                255, 70, 35,
                static_cast<unsigned char>(std::round(110.0f * life01))
        };

        const Color transparentRed{
                255, 70, 35,
                static_cast<unsigned char>(std::round(16.0f * life01))
        };

        // OUTER FLAME
        DrawGradientTriangle(
                outerNeckLeft, yellowHot,
                outerMidLeft, redHot,
                outerMidCenter, redHot);

        DrawGradientTriangle(
                outerNeckRight, yellowHot,
                outerMidCenter, redHot,
                outerMidRight, redHot);

        DrawGradientTriangle(
                outerMidLeft, redHot,
                outerTipLeft, transparentRed,
                outerMidCenter, redHot);

        DrawGradientTriangle(
                outerMidCenter, redHot,
                outerTipLeft, transparentRed,
                outerTipRight, transparentRed);

        DrawGradientTriangle(
                outerMidCenter, redHot,
                outerTipRight, transparentRed,
                outerMidRight, redHot);

        // INNER HOT CORE
        DrawGradientTriangle(
                innerNeckLeft, whiteHot,
                innerMidCenter, yellowHot,
                innerNeckRight, whiteHot);

        DrawGradientTriangle(
                innerNeckLeft, whiteHot,
                innerMidLeft, yellowHot,
                innerMidCenter, yellowHot);

        DrawGradientTriangle(
                innerNeckRight, whiteHot,
                innerMidCenter, yellowHot,
                innerMidRight, yellowHot);

        DrawGradientTriangle(
                innerMidLeft, yellowHot,
                innerTipCenter, redHot,
                innerMidCenter, yellowHot);

        DrawGradientTriangle(
                innerMidCenter, yellowHot,
                innerTipCenter, redHot,
                innerMidRight, yellowHot);
    }

    rlDrawRenderBatchActive();
    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

static void DrawWorldQuad(
        const GameState& state,
        Vector2 a,
        Vector2 b,
        Vector2 c,
        Vector2 d,
        Color color)
{
    const Vector2 sa = TopdownWorldToScreen(state, a);
    const Vector2 sb = TopdownWorldToScreen(state, b);
    const Vector2 sc = TopdownWorldToScreen(state, c);
    const Vector2 sd = TopdownWorldToScreen(state, d);

    DrawTriangle(sa, sb, sc, color);
    DrawTriangle(sa, sc, sd, color);
}

static void DrawWorldQuadOutline(
        const GameState& state,
        Vector2 a,
        Vector2 b,
        Vector2 c,
        Vector2 d,
        float thickness,
        Color color)
{
    const Vector2 sa = TopdownWorldToScreen(state, a);
    const Vector2 sb = TopdownWorldToScreen(state, b);
    const Vector2 sc = TopdownWorldToScreen(state, c);
    const Vector2 sd = TopdownWorldToScreen(state, d);

    DrawLineEx(sa, sb, thickness, color);
    DrawLineEx(sb, sc, thickness, color);
    DrawLineEx(sc, sd, thickness, color);
    DrawLineEx(sd, sa, thickness, color);
}

static void DrawSingleDoor(const GameState& state, const TopdownRuntimeDoor& door)
{
    if (!door.visible) {
        return;
    }

    Vector2 a{};
    Vector2 b{};
    Vector2 c{};
    Vector2 d{};
    TopdownBuildDoorCorners(door, a, b, c, d);

    const Color fillColor = door.color;
    const Color outlineColor = door.outlineColor;

    const float outlineThickness =
            static_cast<float>(std::max(1, state.topdown.currentLevelBaseAssetScale));

    DrawWorldQuad(state, a, b, c, d, fillColor);
    DrawWorldQuadOutline(state, a, b, c, d, outlineThickness, outlineColor);
}

static void TopdownRenderDoors(const GameState& state)
{
    for (const TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
        DrawSingleDoor(state, door);
    }
}

void TopdownRenderWorld(GameState& state, RenderTexture2D& worldTarget, RenderTexture2D& tempTarget)
{
    if (!state.topdown.runtime.levelActive) {
        BeginTextureMode(worldTarget);
        ClearBackground(BLACK);
        EndTextureMode();
        return;
    }

    RenderTexture2D* currentSource = &worldTarget;
    RenderTexture2D* currentDest = &tempTarget;

    if (EnsureTopdownBloodRenderTarget(state, INTERNAL_WIDTH, INTERNAL_HEIGHT)) {
        RebuildTopdownBloodRenderTargetIfNeeded(state);
    }

    BeginWorldTarget(*currentSource);
    ClearBackground(DARKGRAY);
    DrawBottomLayers(state);
    EndWorldTarget();

    ApplyTopdownEffectRegionBucket(
            state,
            state.topdown.runtime.render.afterBottomEffectRegionIndices,
            currentSource,
            currentDest);

    BeginWorldTarget(*currentSource);
    TopdownRenderProps(state, TopdownEffectPlacement::AfterBottom);
    DrawTopdownBloodRenderTargetToWorld(state);
    TopdownRenderNpcs(state);
    TopdownRenderPlayerCharacter(state);
    TopdownRenderWindows(state);
    TopdownRenderDoors(state);
    TopdownRenderWindowGlassParticles(state);
    DrawWallImpactParticles(state);
    DrawBloodImpactParticles(state);
    DrawMuzzleSmokeParticles(state);
    DrawTracerEffects(state);
    EndWorldTarget();

    ApplyTopdownEffectRegionBucket(
            state,
            state.topdown.runtime.render.afterCharactersEffectRegionIndices,
            currentSource,
            currentDest);

    BeginWorldTarget(*currentSource);
    TopdownRenderProps(state, TopdownEffectPlacement::AfterCharacters);
    DrawMuzzleFlashEffects(state);
    DrawTopLayers(state);
    EndWorldTarget();

    ApplyTopdownEffectRegionBucket(
            state,
            state.topdown.runtime.render.finalEffectRegionIndices,
            currentSource,
            currentDest);

    BeginWorldTarget(*currentSource);
    TopdownRenderProps(state, TopdownEffectPlacement::Final);
    EndWorldTarget();

    if (ApplyTopdownPlayerVignette(state, *currentSource, *currentDest)) {
        std::swap(currentSource, currentDest);
    }

    if (currentSource != &worldTarget) {
        BlitRenderTargetFull(*currentSource, worldTarget);
    }
}

static void DrawHealthBar(GameState& state) {
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    const float maxHealth = (player.maxHealth > 0.0f) ? player.maxHealth : 1.0f;
    const float health01 = std::clamp(player.health / maxHealth, 0.0f, 1.0f);

    static constexpr float kBarX = 28.0f;
    static constexpr float kBarY = 28.0f;
    static constexpr float kBarW = 320.0f;
    static constexpr float kBarH = 26.0f;

    Rectangle outer{
            kBarX - 3.0f,
            kBarY - 3.0f,
            kBarW + 6.0f,
            kBarH + 6.0f
    };

    Rectangle bg{
            kBarX,
            kBarY,
            kBarW,
            kBarH
    };

    Rectangle fill{
            kBarX,
            kBarY,
            std::round(kBarW * health01),
            kBarH
    };

    const float danger = 1.0f - health01;

    Color fillColor{
            static_cast<unsigned char>(std::round(40.0f + 215.0f * danger)),
            static_cast<unsigned char>(std::round(220.0f - 170.0f * danger)),
            35,
            235
    };

    DrawRectangleRec(outer, Fade(BLACK, 0.75f));
    DrawRectangleRec(bg, Color{18, 18, 18, 210});

    if (fill.width > 0.0f) {
        DrawRectangleRec(fill, fillColor);
    }

    DrawRectangleLinesEx(bg, 2.0f, Color{0, 0, 0, 255});

    DrawText(
            TextFormat("%d / %d",
                       static_cast<int>(std::round(player.health)),
                       static_cast<int>(std::round(player.maxHealth))),
            static_cast<int>(kBarX + 10.0f),
            static_cast<int>(kBarY + 4.0f),
            18,
            WHITE);
}

static void DrawGameOver(GameState& state)
{
    TopdownRuntimeData& runtime = state.topdown.runtime;

    if (!runtime.gameOverActive) {
        return;
    }

    const float t = runtime.gameOverElapsedMs;

    // --- dim background ---
    const float dim01 = Clamp(t / 600.0f, 0.0f, 1.0f);
    DrawRectangle(
            0,
            0,
            INTERNAL_WIDTH,
            INTERNAL_HEIGHT,
            Color{0, 0, 0, static_cast<unsigned char>(std::round(255.0f * dim01 * 0.75f))}
    );

    // --- Game Over text ---
    const float textFade01 = Clamp((t - 300.0f) / 1200.0f, 0.0f, 1.0f);

    const char* text = "Game Over";
    const int textFontSize = 90;

    Color textColor{
            210,
            35,
            25,
            static_cast<unsigned char>(std::round(255.0f * textFade01))
    };

    const int textWidth = MeasureText(text, textFontSize);

    DrawText(
            text,
            static_cast<int>(INTERNAL_WIDTH * 0.5f - textWidth * 0.5f),
            static_cast<int>(INTERNAL_HEIGHT * 0.5f - 140.0f),
            textFontSize,
            textColor
    );

    // --- button appears later ---
    if (t <= 1400.0f) {
        return;
    }

    const float buttonFade01 = Clamp((t - 1400.0f) / 400.0f, 0.0f, 1.0f);

    Rectangle button{
            std::round(INTERNAL_WIDTH * 0.5f - 280.0f),
            std::round(INTERNAL_HEIGHT * 0.5f + 20.0f),
            560.0f,
            36.0f
    };

    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse, button);

    // Make it actually fill in.
    Color bg = hovered
               ? Color{44, 44, 44, static_cast<unsigned char>(std::round(255.0f * buttonFade01))}
               : Color{32, 32, 32, static_cast<unsigned char>(std::round(255.0f * buttonFade01))};

    Color border = hovered ? YELLOW : DARKGRAY;
    border.a = static_cast<unsigned char>(std::round(255.0f * buttonFade01));

    Color label = hovered ? YELLOW : LIGHTGRAY;
    label.a = static_cast<unsigned char>(std::round(255.0f * buttonFade01));

    DrawRectangleRec(button, bg);
    DrawRectangleLinesEx(button, 1.0f, border);

    const char* btnText = "Return to Menu";
    const int btnFontSize = 20;
    const int btnTextWidth = MeasureText(btnText, btnFontSize);

    DrawText(
            btnText,
            static_cast<int>(button.x + button.width * 0.5f - btnTextWidth * 0.5f),
            static_cast<int>(button.y + button.height * 0.5f - btnFontSize * 0.5f),
            btnFontSize,
            label
    );

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
        if (ev.mouse.button == MOUSE_LEFT_BUTTON &&
            CheckCollisionPointRec(ev.mouse.pos, button)) {
            runtime.returnToMenuRequested = true;
            PlaySoundById(state, "ui_click");
            ConsumeEvent(ev);
            break;
        }
    }
}

void TopdownRenderUi(GameState& state)
{
    if (!state.topdown.runtime.levelActive) {
        return;
    }

    DrawHealthBar(state);
    TopdownRenderNarrationPopups(state);
    DrawGameOver(state);
}
