#pragma once

#include <string>
#include <vector>
#include "raylib.h"
#include "resources/ResourceData.h"
#include "nav/NavMeshData.h"
#include "render/EffectTypes.h"

enum class SceneFacing {
    Left,
    Right,
    Front,
    Back
};

struct ScenePolygon {
    std::vector<Vector2> vertices;
};

struct SceneSpawnPoint {
    std::string id;
    Vector2 position{};
    SceneFacing facing = SceneFacing::Front;
};

struct SceneHotspot {
    std::string id;
    std::string displayName;
    std::string lookText;

    ScenePolygon shape{};

    Vector2 walkTo{};
    SceneFacing facing = SceneFacing::Front;
};

struct SceneExit {
    std::string id;
    std::string displayName;
    std::string lookText;

    ScenePolygon shape{};

    Vector2 walkTo{};
    SceneFacing facing = SceneFacing::Front;

    std::string targetScene;
    std::string targetSpawn;
};

struct SceneImageLayer {
    std::string name;
    std::string imagePath;

    TextureHandle textureHandle = -1;

    Vector2 worldPos{};
    Vector2 sourceSize{};
    Vector2 worldSize{};

    float parallaxX = 1.0f;
    float parallaxY = 1.0f;

    bool visible = true;
    float opacity = 1.0f;
};

enum class ScenePropVisualType {
    None,
    Sprite,
    Image
};

enum class ScenePropDepthMode {
    DepthSorted,
    Back,
    Front
};

struct SceneEffectSpriteData {
    std::string id;
    std::string imagePath;

    TextureHandle textureHandle = -1;

    Vector2 worldPos{};
    Vector2 sourceSize{};
    Vector2 worldSize{};

    bool visible = true;
    float opacity = 1.0f;
    Color tint = WHITE;

    ScenePropDepthMode depthMode = ScenePropDepthMode::DepthSorted;
    EffectBlendMode blendMode = EffectBlendMode::Normal;
    bool renderAsOverlay = false;

    EffectShaderType shaderType = EffectShaderType::None;
    std::string shaderIdString;
    EffectShaderParams shaderParams{};
};

struct SceneEffectRegionData {
    std::string id;
    std::string imagePath;

    TextureHandle textureHandle = -1;

    Rectangle worldRect{};
    bool usePolygon = false;
    ScenePolygon polygon{};

    bool visible = true;
    float opacity = 1.0f;
    Color tint = WHITE;

    ScenePropDepthMode depthMode = ScenePropDepthMode::DepthSorted;
    EffectBlendMode blendMode = EffectBlendMode::Normal;
    bool renderAsOverlay = false;
    int sortOrder = 0;

    EffectShaderType shaderType = EffectShaderType::None;
    std::string shaderIdString;
    EffectShaderParams shaderParams{};
};

struct ScenePropData {
    std::string id;

    ScenePropVisualType visualType = ScenePropVisualType::None;

    SpriteAssetHandle spriteAssetHandle = -1;
    TextureHandle textureHandle = -1;

    Vector2 feetPos{};

    std::string defaultAnimation;
    bool flipX = false;
    bool visible = true;
    bool depthScaling = false;
    ScenePropDepthMode depthMode = ScenePropDepthMode::DepthSorted;
};

struct SceneActorPlacement {
    std::string actorId;
    Vector2 position{};
    SceneFacing facing = SceneFacing::Front;
    bool visible = true;
};

struct SceneScaleConfig {
    float nearY = 0.0f;
    float farY = 1080.0f;

    float nearScale = 1.0f;
    float farScale = 1.0f;
};

struct SceneSoundEmitterData {
    std::string id;
    std::string soundId;

    Vector2 position{};

    float radius = 0.0f;
    float volume = 1.0f;

    bool enabled = true;
    bool pan = true;
    bool loop = true;
};

struct SceneData {
    std::string sceneId;
    std::string saveName;
    std::string sceneFilePath;
    std::string tiledFilePath;
    std::string playerActorAssetPath;
    std::string script;

    int baseAssetScale = 1;

    float worldWidth = 1920.0f;
    float worldHeight = 1080.0f;

    Vector2 playerSpawn{};

    SceneScaleConfig scaleConfig{};

    std::vector<SceneImageLayer> backgroundLayers;
    std::vector<SceneImageLayer> foregroundLayers;
    std::vector<SceneEffectSpriteData> effectSprites;
    std::vector<SceneEffectRegionData> effectRegions;

    NavMeshData navMesh;

    std::vector<SceneSpawnPoint> spawns;
    std::vector<SceneHotspot> hotspots;
    std::vector<SceneExit> exits;
    std::vector<ScenePropData> props;
    std::vector<SceneActorPlacement> actorPlacements;

    std::vector<SceneSoundEmitterData> soundEmitters;

    bool loaded = false;
};
