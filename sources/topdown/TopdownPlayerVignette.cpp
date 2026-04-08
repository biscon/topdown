#include "topdown/TopdownPlayerVignette.h"

#include <cmath>

#include "raylib.h"

namespace
{
    static Shader gPlayerVignetteShader{};
    static bool gPlayerVignetteShaderLoaded = false;

    static int gResolutionLoc = -1;
    static int gTimeLoc = -1;
    static int gDamageFlashLoc = -1;
    static int gLowHealthWeightLoc = -1;

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

    static float Clamp01(float v)
    {
        if (v < 0.0f) {
            return 0.0f;
        }
        if (v > 1.0f) {
            return 1.0f;
        }
        return v;
    }
}

bool InitTopdownPlayerVignetteSystem()
{
    if (gPlayerVignetteShaderLoaded) {
        return true;
    }

    gPlayerVignetteShader = LoadShader(
            nullptr,
            ASSETS_PATH "shaders/topdown/player_vignette.fs");

    if (gPlayerVignetteShader.id == 0) {
        TraceLog(LOG_ERROR, "Failed to load topdown player vignette shader");
        return false;
    }

    gResolutionLoc = GetShaderLocation(gPlayerVignetteShader, "uResolution");
    gTimeLoc = GetShaderLocation(gPlayerVignetteShader, "uTime");
    gDamageFlashLoc = GetShaderLocation(gPlayerVignetteShader, "uDamageFlash");
    gLowHealthWeightLoc = GetShaderLocation(gPlayerVignetteShader, "uLowHealthWeight");

    gPlayerVignetteShaderLoaded = true;
    TraceLog(LOG_INFO, "Loaded topdown player vignette shader");
    return true;
}

void ShutdownTopdownPlayerVignetteSystem()
{
    if (gPlayerVignetteShaderLoaded) {
        UnloadShader(gPlayerVignetteShader);
    }

    gPlayerVignetteShader = {};
    gPlayerVignetteShaderLoaded = false;

    gResolutionLoc = -1;
    gTimeLoc = -1;
    gDamageFlashLoc = -1;
    gLowHealthWeightLoc = -1;
}

bool ApplyTopdownPlayerVignette(
        GameState& state,
        const RenderTexture2D& source,
        RenderTexture2D& dest)
{
    if (!gPlayerVignetteShaderLoaded) {
        return false;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    // Existing runtime already drives these.
    const float damageFlash01 = Clamp01(player.damageFlashRemainingMs / 140.0f);
    const float lowHealth01 = Clamp01(player.lowHealthEffectWeight);

    // Skip the pass entirely if there is nothing to show.
    if (damageFlash01 <= 0.001f && lowHealth01 <= 0.001f) {
        return false;
    }

    const float resolution[2] = {
            static_cast<float>(source.texture.width),
            static_cast<float>(source.texture.height)
    };

    const float timeSeconds = static_cast<float>(GetTime());

    BeginTextureMode(dest);
    ClearBackground(BLACK);

    BeginShaderMode(gPlayerVignetteShader);

    if (gResolutionLoc >= 0) {
        SetShaderValue(
                gPlayerVignetteShader,
                gResolutionLoc,
                resolution,
                SHADER_UNIFORM_VEC2);
    }

    if (gTimeLoc >= 0) {
        SetShaderValue(
                gPlayerVignetteShader,
                gTimeLoc,
                &timeSeconds,
                SHADER_UNIFORM_FLOAT);
    }

    if (gDamageFlashLoc >= 0) {
        SetShaderValue(
                gPlayerVignetteShader,
                gDamageFlashLoc,
                &damageFlash01,
                SHADER_UNIFORM_FLOAT);
    }

    if (gLowHealthWeightLoc >= 0) {
        SetShaderValue(
                gPlayerVignetteShader,
                gLowHealthWeightLoc,
                &lowHealth01,
                SHADER_UNIFORM_FLOAT);
    }

    DrawTexturePro(
            source.texture,
            GetRenderTargetSourceRect(source.texture),
            GetRenderTargetDestRect(dest.texture),
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE);

    EndShaderMode();
    EndTextureMode();

    return true;
}
