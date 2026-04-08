#pragma once

#include <string>
#include "raylib.h"

enum class EffectBlendMode {
    Normal,
    Add,
    Multiply
};

enum class EffectShaderType {
    None,
    UvScroll,
    HeatShimmer,
    RegionGrade,
    WaterRipple,
    WindSway,
    PolyClip
};

enum class EffectShaderCategory {
    None,
    SelfTexture,
    SceneSample
};

struct EffectShaderParams {
    Vector2 scrollSpeed{0.0f, 0.0f};
    Vector2 uvScale{1.0f, 1.0f};
    Vector2 distortionAmount{0.0f, 0.0f};
    Vector2 noiseScrollSpeed{0.0f, 0.0f};
    float intensity = 1.0f;
    float phaseOffset = 0.0f;

    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float softness = 0.2f;
};
