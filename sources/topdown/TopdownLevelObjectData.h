#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "render/EffectTypes.h"
#include "resources/ResourceData.h"
#include "topdown/TopdownCoreData.h"

enum class TopdownObstacleKind {
    MovementAndVision,
    MovementOnly
};

enum class TopdownImageLayerKind {
    Bottom,
    Top
};

enum class TopdownEffectPlacement {
    AfterBottom,
    AfterCharacters,
    Final
};

struct TopdownAuthoredPolygon {
    int tiledObjectId = -1;
    TopdownObstacleKind kind = TopdownObstacleKind::MovementAndVision;
    std::string name;
    std::vector<Vector2> points;
    bool visible = true;
};

struct TopdownAuthoredImageLayer {
    int tiledLayerId = -1;
    TopdownImageLayerKind kind = TopdownImageLayerKind::Bottom;
    std::string name;
    std::string imagePath;

    Vector2 position{};
    Vector2 imageSize{};
    float scale = 1.0f;

    float opacity = 1.0f;
    bool visible = true;
    Color tint = WHITE;

    EffectBlendMode blendMode = EffectBlendMode::Normal;

    EffectShaderType shaderType = EffectShaderType::None;
    std::string shaderIdString;
    EffectShaderParams shaderParams{};

    TextureHandle textureHandle = -1;
};

struct TopdownAuthoredSpawn {
    int tiledObjectId = -1;
    std::string id;
    Vector2 position{};
    float orientationDegrees = 0.0f;
    bool visible = true;
};

struct TopdownAuthoredNpc {
    int tiledObjectId = -1;
    std::string id;
    std::string assetId;
    Vector2 position{};
    float orientationDegrees = 0.0f;
    bool persistentChase = false;
    bool guard = false;
    bool visible = true;
};

struct TopdownAuthoredEffectRegion {
    int tiledObjectId = -1;
    std::string id;

    bool usePolygon = false;
    std::vector<Vector2> polygon;
    Rectangle worldRect{};

    bool hasOcclusionOriginOverride = false;
    Vector2 occlusionOrigin{};

    bool visible = true;
    float opacity = 1.0f;
    Color tint = WHITE;

    TopdownEffectPlacement placement = TopdownEffectPlacement::AfterBottom;
    int sortIndex = 0;

    EffectBlendMode blendMode = EffectBlendMode::Normal;
    bool occludedByWalls = false;

    EffectShaderType shaderType = EffectShaderType::None;
    std::string shaderIdString;
    EffectShaderParams shaderParams{};

    std::string imagePath;
    TextureHandle textureHandle = -1;
};

struct TopdownRuntimeImageLayer {
    TopdownImageLayerHandle handle = -1;
    int authoredIndex = -1;
    TopdownImageLayerKind kind = TopdownImageLayerKind::Bottom;

    TextureHandle textureHandle = -1;

    Vector2 position{};
    Vector2 imageSize{};
    float scale = 1.0f;

    float opacity = 1.0f;
    bool visible = true;
    Color tint = WHITE;

    EffectBlendMode blendMode = EffectBlendMode::Normal;

    EffectShaderType shaderType = EffectShaderType::None;
    EffectShaderParams shaderParams{};
};

using TopdownEffectRegionHandle = int;

struct TopdownRuntimeEffectRegion {
    TopdownEffectRegionHandle handle = -1;
    int authoredIndex = -1;

    bool visible = true;
    float opacity = 1.0f;
    Color tint = WHITE;

    EffectShaderType shaderType = EffectShaderType::None;
    EffectShaderParams shaderParams{};

    bool occludedByWalls = false;

    bool hasWallOcclusionPolygon = false;
    std::vector<Vector2> wallOcclusionPolygon;

    bool hasWallOcclusionTriangles = false;
    std::vector<Vector2> wallOcclusionTriangleVertices;
};

struct TopdownAuthoredSoundEmitter {
    std::string id;
    Vector2 position{};

    std::string soundId;
    bool loop = false;
    bool pan = false;

    float radius = 0.0f;
    float volume = 1.0f;

    bool enabled = true;
};
