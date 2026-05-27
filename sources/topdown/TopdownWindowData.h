#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "topdown/TopdownCoreData.h"

struct TopdownWindowBreakParticleConfig {
    int count = 24;
    float speedMin = 80.0f;
    float speedMax = 220.0f;
    float lifetimeMsMin = 180.0f;
    float lifetimeMsMax = 420.0f;
    float sizeMin = 2.0f;
    float sizeMax = 4.5f;
    float spreadAlongWindow = 26.0f;

    Color color1 = Color{210, 240, 250, 255};
    Color color2 = Color{160, 210, 230, 255};
};

struct TopdownAuthoredWindow {
    int tiledObjectId = -1;
    std::string id;
    bool visible = true;

    Vector2 rectPosition{};
    Vector2 rectSize{};
    bool horizontal = true;

    Color color1 = Color{138, 196, 195, 255};
    Color color2 = Color{100, 135, 140, 255};
    Color outlineColor = Color{23, 24, 25, 255};

    std::string breakSoundId;
    TopdownWindowBreakParticleConfig breakParticles;
};

struct TopdownRuntimeWindow {
    int tiledObjectId = -1;
    std::string id;
    bool visible = true;

    Rectangle worldRect{};
    bool horizontal = true;
    bool broken = false;

    std::vector<Vector2> polygon;
    std::vector<TopdownSegment> edges;

    Texture2D atlasTexture{};
    bool atlasLoaded = false;

    Rectangle intactSrc{};
    Rectangle brokenSrc{};

    Color color1 = Color{138, 196, 195, 255};
    Color color2 = Color{100, 135, 140, 255};
    Color outlineColor = Color{23, 24, 25, 255};

    std::string breakSoundId;
    TopdownWindowBreakParticleConfig breakParticles;
};

struct TopdownWindowGlassParticle {
    bool active = false;

    Vector2 position{};
    Vector2 velocity{};

    float ageMs = 0.0f;
    float lifetimeMs = 220.0f;

    float size = 2.0f;
    float alpha = 1.0f;

    float rotationRadians = 0.0f;

    Color color = Color{210, 240, 250, 255};
};
