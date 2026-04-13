#include "topdown/LevelWindows.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "audio/Audio.h"
#include "raylib.h"
#include "topdown/LevelCollision.h"
#include "topdown/TopdownHelpers.h"

static constexpr int kMaxWindowGlassParticles = 768;

static float ClampFloat(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint32_t HashU32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static float NextRand01(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

static int ClampInt(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void DrawInsideOutlineRect(
        Image& image,
        int x,
        int y,
        int w,
        int h,
        int thickness,
        Color color)
{
    if (w <= 0 || h <= 0 || thickness <= 0) {
        return;
    }

    thickness = std::min(thickness, std::min(w, h));

    ImageDrawRectangle(&image, x, y, w, thickness, color);
    ImageDrawRectangle(&image, x, y + h - thickness, w, thickness, color);
    ImageDrawRectangle(&image, x, y + thickness, thickness, h - thickness * 2, color);
    ImageDrawRectangle(&image, x + w - thickness, y + thickness, thickness, h - thickness * 2, color);
}

static void DrawWindowFrameToImage(
        Image& atlas,
        int frameX,
        int frameWidth,
        int frameHeight,
        const TopdownRuntimeWindow& window,
        int baseAssetScale,
        bool broken)
{
    if (frameWidth <= 0 || frameHeight <= 0) {
        return;
    }

    ImageDrawRectangle(
            &atlas,
            frameX,
            0,
            frameWidth,
            frameHeight,
            window.color2);

    const int minDim = std::max(1, std::min(frameWidth, frameHeight));
    int outlineThickness = std::max(1, baseAssetScale);
    outlineThickness = std::min(outlineThickness, std::max(1, minDim / 3));

    DrawInsideOutlineRect(
            atlas,
            frameX,
            0,
            frameWidth,
            frameHeight,
            outlineThickness,
            window.outlineColor);

    const int contentX = frameX + outlineThickness;
    const int contentY = outlineThickness;
    const int contentW = std::max(1, frameWidth - outlineThickness * 2);
    const int contentH = std::max(1, frameHeight - outlineThickness * 2);

    if (!broken) {
        if (window.horizontal) {
            const int innerH = std::max(1, static_cast<int>(std::round(contentH * 0.5f)));
            const int innerY = contentY + (contentH - innerH) / 2;

            ImageDrawRectangle(
                    &atlas,
                    contentX,
                    innerY,
                    contentW,
                    innerH,
                    window.color1);
        } else {
            const int innerW = std::max(1, static_cast<int>(std::round(contentW * 0.5f)));
            const int innerX = contentX + (contentW - innerW) / 2;

            ImageDrawRectangle(
                    &atlas,
                    innerX,
                    contentY,
                    innerW,
                    contentH,
                    window.color1);
        }

        return;
    }

    uint32_t seed = HashU32(
            static_cast<uint32_t>(window.tiledObjectId) ^
            static_cast<uint32_t>(frameWidth << 16) ^
            static_cast<uint32_t>(frameHeight));

    const int longAxisPixels = window.horizontal ? contentW : contentH;
    const int fragmentCount = std::max(32, longAxisPixels / 2);

    for (int i = 0; i < fragmentCount; ++i) {
        const float tA = NextRand01(seed);
        const float tB = NextRand01(seed);

        int px = contentX;
        int py = contentY;
        int pw = 2;
        int ph = 2;

        if (window.horizontal) {
            px += ClampInt(static_cast<int>(std::floor(tA * static_cast<float>(std::max(1, contentW - 1)))), 0, std::max(0, contentW - 1));
            py += ClampInt(static_cast<int>(std::floor(tB * static_cast<float>(std::max(1, contentH - 1)))), 0, std::max(0, contentH - 1));

            pw = (NextRand01(seed) < 0.22f) ? 6 : 3;
            ph = (NextRand01(seed) < 0.10f) ? 6 : 3;
        } else {
            px += ClampInt(static_cast<int>(std::floor(tA * static_cast<float>(std::max(1, contentW - 1)))), 0, std::max(0, contentW - 1));
            py += ClampInt(static_cast<int>(std::floor(tB * static_cast<float>(std::max(1, contentH - 1)))), 0, std::max(0, contentH - 1));

            pw = (NextRand01(seed) < 0.10f) ? 6 : 3;
            ph = (NextRand01(seed) < 0.22f) ? 6 : 3;
        }

        pw = std::min(pw, contentX + contentW - px);
        ph = std::min(ph, contentY + contentH - py);

        ImageDrawRectangle(&atlas, px, py, pw, ph, window.color1);
    }
}

static void EnforceWindowGlassParticleCap(TopdownRenderWorld& renderWorld)
{
    std::vector<TopdownWindowGlassParticle>& particles = renderWorld.windowGlassParticles;
    while (static_cast<int>(particles.size()) > kMaxWindowGlassParticles) {
        particles.erase(particles.begin());
    }
}

static void SpawnWindowBreakParticles(
        GameState& state,
        const TopdownRuntimeWindow& window,
        Vector2 hitPoint,
        Vector2 shotDir)
{
    shotDir = TopdownNormalizeOrZero(shotDir);
    if (TopdownLengthSqr(shotDir) <= 0.000001f) {
        shotDir = window.horizontal ? Vector2{0.0f, 1.0f} : Vector2{1.0f, 0.0f};
    }

    const Vector2 axis = window.horizontal ? Vector2{1.0f, 0.0f} : Vector2{0.0f, 1.0f};
    const Vector2 crossAxis = window.horizontal ? Vector2{0.0f, 1.0f} : Vector2{1.0f, 0.0f};

    const float rectMinAlong = window.horizontal ? window.worldRect.x : window.worldRect.y;
    const float rectMaxAlong = window.horizontal
                               ? (window.worldRect.x + window.worldRect.width)
                               : (window.worldRect.y + window.worldRect.height);

    const float rectMinAcross = window.horizontal ? window.worldRect.y : window.worldRect.x;
    const float rectMaxAcross = window.horizontal
                                ? (window.worldRect.y + window.worldRect.height)
                                : (window.worldRect.x + window.worldRect.width);

    const float hitAlong = window.horizontal ? hitPoint.x : hitPoint.y;
    const float centerAcross = (rectMinAcross + rectMaxAcross) * 0.5f;

    for (int i = 0; i < window.breakParticleCount; ++i) {
        TopdownWindowGlassParticle particle;
        particle.active = true;
        particle.ageMs = 0.0f;
        particle.lifetimeMs = RandomRangeFloat(
                window.breakParticleLifetimeMsMin,
                window.breakParticleLifetimeMsMax);
        particle.size = RandomRangeFloat(
                window.breakParticleSizeMin,
                window.breakParticleSizeMax);
        particle.alpha = 1.0f;

        float along = hitAlong;
        if (RandomRangeFloat(0.0f, 1.0f) < 0.78f) {
            const float sign = (RandomRangeFloat(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
            const float mag = RandomRangeFloat(0.0f, 1.0f);
            along += sign * mag * mag * window.breakParticleSpreadAlongWindow;
        } else {
            along = RandomRangeFloat(rectMinAlong, rectMaxAlong);
        }

        along = ClampFloat(along, rectMinAlong + 1.0f, rectMaxAlong - 1.0f);

        const float across = RandomRangeFloat(rectMinAcross + 1.0f, rectMaxAcross - 1.0f);

        if (window.horizontal) {
            particle.position = Vector2{ along, across };
        } else {
            particle.position = Vector2{ across, along };
        }

        const float speed = RandomRangeFloat(
                window.breakParticleSpeedMin,
                window.breakParticleSpeedMax);

        particle.velocity = TopdownAdd(
                TopdownMul(shotDir, speed),
                TopdownAdd(
                        TopdownMul(axis, RandomRangeFloat(-45.0f, 45.0f)),
                        TopdownMul(crossAxis, RandomRangeFloat(-24.0f, 24.0f))));

        particle.color =
                (GetRandomValue(0, 1) == 0)
                ? window.breakParticleColor1
                : window.breakParticleColor2;

        particle.rotationRadians = std::atan2(particle.velocity.y, particle.velocity.x);

        state.topdown.runtime.render.windowGlassParticles.push_back(particle);
    }

    (void)centerAcross;
    EnforceWindowGlassParticleCap(state.topdown.runtime.render);
}

TopdownRuntimeWindow TopdownBuildRuntimeWindowFromAuthored(
        const TopdownAuthoredWindow& authored)
{
    TopdownRuntimeWindow runtime;
    runtime.tiledObjectId = authored.tiledObjectId;
    runtime.id = authored.id;
    runtime.visible = authored.visible;

    runtime.worldRect = Rectangle{
            authored.rectPosition.x,
            authored.rectPosition.y,
            authored.rectSize.x,
            authored.rectSize.y
    };

    runtime.horizontal = authored.rectSize.x >= authored.rectSize.y;
    runtime.broken = false;

    runtime.color1 = authored.color1;
    runtime.color2 = authored.color2;
    runtime.outlineColor = authored.outlineColor;
    runtime.breakSoundId = authored.breakSoundId;

    runtime.breakParticleCount = authored.breakParticleCount;
    runtime.breakParticleSpeedMin = authored.breakParticleSpeedMin;
    runtime.breakParticleSpeedMax = authored.breakParticleSpeedMax;
    runtime.breakParticleLifetimeMsMin = authored.breakParticleLifetimeMsMin;
    runtime.breakParticleLifetimeMsMax = authored.breakParticleLifetimeMsMax;
    runtime.breakParticleSizeMin = authored.breakParticleSizeMin;
    runtime.breakParticleSizeMax = authored.breakParticleSizeMax;
    runtime.breakParticleSpreadAlongWindow = authored.breakParticleSpreadAlongWindow;
    runtime.breakParticleColor1 = authored.breakParticleColor1;
    runtime.breakParticleColor2 = authored.breakParticleColor2;

    runtime.polygon = TopdownBuildRectPolygon(
            authored.rectPosition.x,
            authored.rectPosition.y,
            authored.rectSize.x,
            authored.rectSize.y,
            1.0f);

    runtime.edges = TopdownBuildSegmentsFromPolygon(runtime.polygon);

    const int frameW = std::max(1, static_cast<int>(std::round(runtime.worldRect.width)));
    const int frameH = std::max(1, static_cast<int>(std::round(runtime.worldRect.height)));

    runtime.intactSrc = Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(frameW),
            static_cast<float>(frameH)
    };

    runtime.brokenSrc = Rectangle{
            static_cast<float>(frameW),
            0.0f,
            static_cast<float>(frameW),
            static_cast<float>(frameH)
    };

    return runtime;
}

bool TopdownGenerateWindowTextureAtlas(
        TopdownRuntimeWindow& window,
        int baseAssetScale)
{
    const int frameW = std::max(1, static_cast<int>(std::round(window.worldRect.width)));
    const int frameH = std::max(1, static_cast<int>(std::round(window.worldRect.height)));

    Image atlas = GenImageColor(frameW * 2, frameH, BLANK);

    DrawWindowFrameToImage(atlas, 0, frameW, frameH, window, baseAssetScale, false);
    DrawWindowFrameToImage(atlas, frameW, frameW, frameH, window, baseAssetScale, true);

    window.atlasTexture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);

    if (window.atlasTexture.id == 0) {
        window.atlasLoaded = false;
        return false;
    }

    SetTextureFilter(window.atlasTexture, TEXTURE_FILTER_POINT);
    window.atlasLoaded = true;
    return true;
}

void TopdownUnloadWindowResources(TopdownData& topdown)
{
    for (TopdownRuntimeWindow& window : topdown.runtime.windows) {
        if (window.atlasLoaded && window.atlasTexture.id != 0) {
            UnloadTexture(window.atlasTexture);
        }

        window.atlasLoaded = false;
        window.atlasTexture = Texture2D{};
    }
}

bool RaycastClosestWindow(
        GameState& state,
        Vector2 origin,
        Vector2 dirNormalized,
        float maxDistance,
        TopdownRuntimeWindow*& outWindow,
        Vector2& outHitPoint,
        Vector2& outHitNormal,
        float& outHitDistance)
{
    outWindow = nullptr;
    outHitDistance = maxDistance;

    bool foundHit = false;

    for (TopdownRuntimeWindow& window : state.topdown.runtime.windows) {
        if (!window.visible || window.broken) {
            continue;
        }

        Vector2 hitPoint{};
        Vector2 hitNormal{};
        float hitDistance = outHitDistance;

        if (!RaycastClosestSegmentWithNormal(
                origin,
                dirNormalized,
                window.edges,
                outHitDistance,
                hitPoint,
                hitNormal,
                hitDistance)) {
            continue;
        }

        if (hitDistance < outHitDistance) {
            foundHit = true;
            outWindow = &window;
            outHitPoint = hitPoint;
            outHitNormal = hitNormal;
            outHitDistance = hitDistance;
        }
    }

    return foundHit;
}

bool BreakWindow(
        GameState& state,
        TopdownRuntimeWindow& window,
        Vector2 hitPoint,
        Vector2 shotDir)
{
    if (window.broken || !window.visible) {
        return false;
    }

    window.broken = true;

    if (!window.breakSoundId.empty()) {
        PlaySoundById(state, window.breakSoundId, RandomRangeFloat(0.97f, 1.03f));
    }

    SpawnWindowBreakParticles(state, window, hitPoint, shotDir);
    return true;
}

void TopdownUpdateWindows(GameState& state, float dt)
{
    std::vector<TopdownWindowGlassParticle>& particles =
            state.topdown.runtime.render.windowGlassParticles;

    for (TopdownWindowGlassParticle& particle : particles) {
        if (!particle.active) {
            continue;
        }

        particle.ageMs += dt * 1000.0f;
        if (particle.ageMs >= particle.lifetimeMs) {
            particle.active = false;
            continue;
        }

        particle.position = TopdownAdd(
                particle.position,
                TopdownMul(particle.velocity, dt));

        particle.velocity = MoveTowardsVector(
                particle.velocity,
                Vector2{},
                260.0f * dt);

        particle.rotationRadians = std::atan2(particle.velocity.y, particle.velocity.x);

        const float life01 = ClampFloat(particle.ageMs / particle.lifetimeMs, 0.0f, 1.0f);
        particle.alpha = 1.0f - life01;
    }

    particles.erase(
            std::remove_if(
                    particles.begin(),
                    particles.end(),
                    [](const TopdownWindowGlassParticle& particle) {
                        return !particle.active;
                    }),
            particles.end());
}

static void DrawWindowFallback(
        const GameState& state,
        const TopdownRuntimeWindow& window)
{
    const Vector2 screenPos = TopdownWorldToScreen(
            state,
            Vector2{ window.worldRect.x, window.worldRect.y });

    const Rectangle outer{
            std::round(screenPos.x),
            std::round(screenPos.y),
            std::round(window.worldRect.width),
            std::round(window.worldRect.height)
    };

    DrawRectangleRec(outer, window.color2);

    const int outlineThickness =
            std::max(1, state.topdown.currentLevelBaseAssetScale);

    DrawRectangle(
            static_cast<int>(outer.x),
            static_cast<int>(outer.y),
            static_cast<int>(outer.width),
            outlineThickness,
            window.outlineColor);

    DrawRectangle(
            static_cast<int>(outer.x),
            static_cast<int>(outer.y + outer.height - outlineThickness),
            static_cast<int>(outer.width),
            outlineThickness,
            window.outlineColor);

    DrawRectangle(
            static_cast<int>(outer.x),
            static_cast<int>(outer.y + outlineThickness),
            outlineThickness,
            static_cast<int>(outer.height - outlineThickness * 2.0f),
            window.outlineColor);

    DrawRectangle(
            static_cast<int>(outer.x + outer.width - outlineThickness),
            static_cast<int>(outer.y + outlineThickness),
            outlineThickness,
            static_cast<int>(outer.height - outlineThickness * 2.0f),
            window.outlineColor);

    if (!window.broken) {
        if (window.horizontal) {
            const float innerH = std::max(1.0f, std::round((outer.height - outlineThickness * 2.0f) * 0.5f));
            const float innerY = std::round(outer.y + (outer.height - innerH) * 0.5f);

            DrawRectangle(
                    static_cast<int>(outer.x + outlineThickness),
                    static_cast<int>(innerY),
                    static_cast<int>(outer.width - outlineThickness * 2.0f),
                    static_cast<int>(innerH),
                    window.color1);
        } else {
            const float innerW = std::max(1.0f, std::round((outer.width - outlineThickness * 2.0f) * 0.5f));
            const float innerX = std::round(outer.x + (outer.width - innerW) * 0.5f);

            DrawRectangle(
                    static_cast<int>(innerX),
                    static_cast<int>(outer.y + outlineThickness),
                    static_cast<int>(innerW),
                    static_cast<int>(outer.height - outlineThickness * 2.0f),
                    window.color1);
        }
    }
}

void TopdownRenderWindows(const GameState& state)
{
    for (const TopdownRuntimeWindow& window : state.topdown.runtime.windows) {
        if (!window.visible) {
            continue;
        }

        if (!window.atlasLoaded || window.atlasTexture.id == 0) {
            DrawWindowFallback(state, window);
            continue;
        }

        const Rectangle src = window.broken ? window.brokenSrc : window.intactSrc;

        const Vector2 screenPos = TopdownWorldToScreen(
                state,
                Vector2{ window.worldRect.x, window.worldRect.y });

        const Rectangle dst{
                std::round(screenPos.x),
                std::round(screenPos.y),
                std::round(window.worldRect.width),
                std::round(window.worldRect.height)
        };

        DrawTexturePro(
                window.atlasTexture,
                src,
                dst,
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE);
    }
}

void TopdownRenderWindowGlassParticles(const GameState& state)
{
    const std::vector<TopdownWindowGlassParticle>& particles =
            state.topdown.runtime.render.windowGlassParticles;

    if (particles.empty()) {
        return;
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA);

    for (const TopdownWindowGlassParticle& particle : particles) {
        if (!particle.active || particle.alpha <= 0.0f) {
            continue;
        }

        Vector2 dir{
                std::cos(particle.rotationRadians),
                std::sin(particle.rotationRadians)
        };

        if (TopdownLengthSqr(dir) <= 0.000001f) {
            dir = TopdownNormalizeOrZero(particle.velocity);
        }

        if (TopdownLengthSqr(dir) <= 0.000001f) {
            dir = Vector2{1.0f, 0.0f};
        }

        const Vector2 screenPos = TopdownWorldToScreen(state, particle.position);
        const float halfLen = std::max(1.0f, particle.size * 0.7f);

        const Vector2 a = TopdownSub(screenPos, TopdownMul(dir, halfLen));
        const Vector2 b = TopdownAdd(screenPos, TopdownMul(dir, halfLen));

        Color color = particle.color;
        color.a = static_cast<unsigned char>(
                std::round(255.0f * ClampFloat(particle.alpha, 0.0f, 1.0f)));

        DrawLineEx(
                a,
                b,
                std::max(1.0f, particle.size * 0.55f),
                color);
    }

    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}
