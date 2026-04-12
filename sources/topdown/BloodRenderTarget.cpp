#include "BloodRenderTarget.h"

#include <cmath>
#include <algorithm>

#include "TopdownHelpers.h"
#include "raymath.h"
#include "raylib.h"

namespace
{
    static Shader gBloodBlurShader{};
    static bool gBloodBlurShaderLoaded = false;
    static int gBloodBlurTexelSizeLoc = -1;

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

    static const TopdownBloodStamp* FindBloodStampForDecal(
            const TopdownBloodStampLibrary& library,
            const TopdownBloodDecal& decal)
    {
        if (!decal.useGeneratedStamp || decal.stampIndex < 0) {
            return nullptr;
        }

        if (decal.preferStreakStamp) {
            if (decal.stampIndex >= static_cast<int>(library.streaks.size())) {
                return nullptr;
            }

            return &library.streaks[decal.stampIndex];
        }

        if (decal.stampIndex >= static_cast<int>(library.splats.size())) {
            return nullptr;
        }

        return &library.splats[decal.stampIndex];
    }

    static bool IsBloodDecalVisibleOnScreen(const GameState& state, const TopdownBloodDecal& decal)
    {
        if (!decal.active || decal.radius <= 0.0f || decal.opacity <= 0.0f) {
            return false;
        }

        const Vector2 screenPos = TopdownWorldToScreen(state, decal.position);
        const float stretch = std::max(0.25f, decal.stretch);
        const float halfWidth = decal.radius * stretch;
        const float halfHeight = decal.radius;

        const float minX = screenPos.x - halfWidth;
        const float maxX = screenPos.x + halfWidth;
        const float minY = screenPos.y - halfHeight;
        const float maxY = screenPos.y + halfHeight;

        if (maxX < 0.0f || maxY < 0.0f) {
            return false;
        }

        if (minX > static_cast<float>(state.topdown.runtime.bloodRenderTarget.width) ||
            minY > static_cast<float>(state.topdown.runtime.bloodRenderTarget.height)) {
            return false;
        }

        return true;
    }

    static void DrawSingleBloodDecalToAccumulationTarget(
            GameState& state,
            const TopdownBloodDecal& decal)
    {
        if (!IsBloodDecalVisibleOnScreen(state, decal)) {
            return;
        }

        const TopdownBloodStamp* stamp =
                FindBloodStampForDecal(state.topdown.bloodStampLibrary, decal);

        if (stamp == nullptr || !stamp->loaded || stamp->texture.id == 0) {
            return;
        }

        const Vector2 screenPos = TopdownWorldToScreen(state, decal.position);

        const float baseSize = decal.radius * 2.0f;
        const float width = baseSize * std::max(0.25f, decal.stretch);
        const float height = baseSize;

        Rectangle src{
                0.0f,
                0.0f,
                static_cast<float>(stamp->texture.width),
                static_cast<float>(stamp->texture.height)
        };

        Rectangle dst{
                std::round(screenPos.x),
                std::round(screenPos.y),
                std::round(width),
                std::round(height)
        };

        Vector2 origin{
                dst.width * 0.5f,
                dst.height * 0.5f
        };

        float fade01 = 1.0f;

        if (decal.fadeInMs > 0.0f) {
            fade01 = Clamp(decal.ageMs / decal.fadeInMs, 0.0f, 1.0f);
        }

        float finalOpacity = decal.opacity * fade01;

        const unsigned char alpha =
                (unsigned char)(255.0f * Clamp(finalOpacity, 0.0f, 1.0f));

        /*
        const unsigned char alpha =
                static_cast<unsigned char>(
                        std::round(255.0f * Clamp(decal.opacity, 0.0f, 1.0f)));
                        */

        // Stamp textures are premultiplied alpha white masks,
        // so PMA tint here is grayscale with matching alpha.
        const Color tint{
                alpha,
                alpha,
                alpha,
                alpha
        };

        DrawTexturePro(
                stamp->texture,
                src,
                dst,
                origin,
                decal.rotationRadians * RAD2DEG,
                tint);
    }

    static void BlurBloodAccumulationTarget(TopdownBloodRenderTarget& blood)
    {
        if (!blood.loaded || !gBloodBlurShaderLoaded) {
            return;
        }

        BeginTextureMode(blood.blurredTarget);
        ClearBackground(BLANK);

        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        BeginShaderMode(gBloodBlurShader);

        if (gBloodBlurTexelSizeLoc >= 0) {
            const float texelSize[2] = {
                    1.0f / static_cast<float>(blood.width),
                    1.0f / static_cast<float>(blood.height)
            };

            SetShaderValue(
                    gBloodBlurShader,
                    gBloodBlurTexelSizeLoc,
                    texelSize,
                    SHADER_UNIFORM_VEC2);
        }

        DrawTexturePro(
                blood.target.texture,
                GetRenderTargetSourceRect(blood.target.texture),
                GetRenderTargetDestRect(blood.blurredTarget.texture),
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE);

        EndShaderMode();
        EndBlendMode();
        EndTextureMode();
    }
}

bool InitTopdownBloodRenderTargetSystem()
{
    if (gBloodBlurShaderLoaded) {
        return true;
    }

    gBloodBlurShader = LoadShader(nullptr, ASSETS_PATH "shaders/topdown/blood_accum_blur.fs");
    if (gBloodBlurShader.id == 0) {
        TraceLog(LOG_ERROR, "Failed to load topdown blood blur shader");
        return false;
    }

    gBloodBlurTexelSizeLoc = GetShaderLocation(gBloodBlurShader, "uTexelSize");
    gBloodBlurShaderLoaded = true;

    TraceLog(LOG_INFO, "Loaded topdown blood blur shader");
    return true;
}

void ShutdownTopdownBloodRenderTargetSystem()
{
    if (gBloodBlurShaderLoaded) {
        UnloadShader(gBloodBlurShader);
    }

    gBloodBlurShader = {};
    gBloodBlurShaderLoaded = false;
    gBloodBlurTexelSizeLoc = -1;
}

void UnloadTopdownBloodRenderTarget(GameState& state)
{
    TopdownBloodRenderTarget& blood = state.topdown.runtime.bloodRenderTarget;

    if (blood.loaded) {
        if (blood.target.id != 0) {
            UnloadRenderTexture(blood.target);
        }

        if (blood.blurredTarget.id != 0) {
            UnloadRenderTexture(blood.blurredTarget);
        }
    }

    blood = {};
}

bool EnsureTopdownBloodRenderTarget(GameState& state, int width, int height)
{
    TopdownBloodRenderTarget& blood = state.topdown.runtime.bloodRenderTarget;

    if (blood.loaded && blood.width == width && blood.height == height) {
        return true;
    }

    UnloadTopdownBloodRenderTarget(state);

    blood.target = LoadRenderTexture(width, height);
    blood.blurredTarget = LoadRenderTexture(width, height);

    if (blood.target.texture.id == 0 || blood.blurredTarget.texture.id == 0) {
        UnloadTopdownBloodRenderTarget(state);
        return false;
    }

    SetTextureFilter(blood.target.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(blood.blurredTarget.texture, TEXTURE_FILTER_BILINEAR);

    blood.loaded = true;
    blood.width = width;
    blood.height = height;
    blood.dirty = true;
    blood.hasLastCameraPosition = false;
    blood.lastCameraPosition = Vector2{};

    BeginTextureMode(blood.target);
    ClearBackground(BLANK);
    EndTextureMode();

    BeginTextureMode(blood.blurredTarget);
    ClearBackground(BLANK);
    EndTextureMode();

    return true;
}

void MarkTopdownBloodRenderTargetDirty(GameState& state)
{
    state.topdown.runtime.bloodRenderTarget.dirty = true;
}

void RebuildTopdownBloodRenderTargetIfNeeded(GameState& state)
{
    TopdownBloodRenderTarget& blood = state.topdown.runtime.bloodRenderTarget;
    if (!blood.loaded) {
        return;
    }

    const Vector2 currentCameraPos = state.topdown.runtime.camera.position;

    if (!blood.hasLastCameraPosition ||
        std::fabs(currentCameraPos.x - blood.lastCameraPosition.x) > 0.001f ||
        std::fabs(currentCameraPos.y - blood.lastCameraPosition.y) > 0.001f) {
        blood.dirty = true;
    }

    if (!blood.dirty) {
        return;
    }

    BeginTextureMode(blood.target);
    ClearBackground(BLANK);

    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);

    for (const TopdownBloodDecal& decal : state.topdown.runtime.render.bloodDecals) {
        DrawSingleBloodDecalToAccumulationTarget(state, decal);
    }

    EndBlendMode();
    EndTextureMode();

    BlurBloodAccumulationTarget(blood);

    blood.lastCameraPosition = currentCameraPos;
    blood.hasLastCameraPosition = true;
    blood.dirty = false;
}

void DrawTopdownBloodRenderTargetToWorld(GameState& state)
{
    TopdownBloodRenderTarget& blood = state.topdown.runtime.bloodRenderTarget;
    if (!blood.loaded) {
        return;
    }

    // PMA tint for compositing the blurred white mask into the world.

    const Color bloodTint{
            120,
            18,
            18,
            255
    };

    DrawTexturePro(
            blood.blurredTarget.texture,
            GetRenderTargetSourceRect(blood.blurredTarget.texture),
            Rectangle{
                    0.0f,
                    0.0f,
                    static_cast<float>(blood.width),
                    static_cast<float>(blood.height)
            },
            Vector2{0.0f, 0.0f},
            0.0f,
            bloodTint);
}
