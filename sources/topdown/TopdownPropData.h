#pragma once

#include <string>

#include "raylib.h"
#include "resources/ResourceData.h"
#include "topdown/TopdownLevelObjectData.h"
#include "utils/Interpolation.h"

enum class TopdownPropType {
    Image,
    Sprite
};

struct TopdownAuthoredProp {
    int tiledObjectId = -1;
    std::string id;

    Vector2 position{};
    bool visible = true;

    std::string assetPath;
    TopdownPropType type = TopdownPropType::Image;
    TextureHandle textureHandle = -1;
    SpriteAssetHandle spriteHandle = -1;

    bool flipX = false;

    std::string animation;
    bool loop = false;

    TopdownEffectPlacement placement = TopdownEffectPlacement::AfterBottom;
    float sortIndex = 0.0f;

    float opacity = 1.0f;

    bool hasOriginOverride = false;
    Vector2 originOverride{};
};

struct TopdownRuntimeProp {
    bool active = false;
    int authoredIndex = -1;
    std::string id;

    Vector2 position{};
    bool visible = true;

    TopdownPropType type = TopdownPropType::Image;
    TextureHandle textureHandle = -1;
    SpriteAssetHandle spriteHandle = -1;

    bool flipX = false;

    float opacity = 1.0f;

    std::string baseAnimation;
    std::string currentAnimation;
    bool oneShotActive = false;
    std::string oneShotAnimation;
    float animationTimeMs = 0.0f;
    float oneShotDurationMs = 0.0f;
    bool loop = false;
    bool moving = false;
    Vector2 moveStart{};
    Vector2 moveEnd{};
    float moveTimerMs = 0.0f;
    float moveDurationMs = 0.0f;
    MoveInterpolation moveInterpolation = MoveInterpolation::Linear;

    bool hasOriginOverride = false;
    Vector2 originOverride{};

    TopdownEffectPlacement placement = TopdownEffectPlacement::AfterBottom;
    float sortIndex = 0.0f;
};
