#pragma once

#include <vector>

#include "raylib.h"
#include "topdown/TopdownCoreData.h"
#include "topdown/TopdownLevelObjectData.h"
#include "topdown/TopdownWindowData.h"

struct TopdownBloodStamp {
    Texture2D texture{};
    bool loaded = false;
    bool isStreak = false;
};

struct TopdownBloodStampLibrary {
    bool generated = false;
    std::vector<TopdownBloodStamp> splats;
    std::vector<TopdownBloodStamp> streaks;
    std::vector<TopdownBloodStamp> particles;
};

struct TopdownTracerEffect {
    bool active = false;

    Vector2 start{};
    Vector2 end{};

    float ageMs = 0.0f;
    float lifetimeMs = 50.0f;

    float thickness = 4.0f;
    TopdownTracerStyle style = TopdownTracerStyle::Handgun;

    bool anchoredToPlayer = false;
    TopdownCharacterHandle anchoredNpcHandle = -1;
    Vector2 localStartOffset{};
};

enum class TopdownHitscanHitType {
    None,
    Npc,
    Wall
};

struct TopdownWallImpactParticle {
    bool active = false;

    Vector2 position{};
    Vector2 velocity{};

    float ageMs = 0.0f;
    float lifetimeMs = 0.0f;

    float size = 2.0f;
    unsigned char alpha = 255;

    Color color = Color{160, 160, 160, 255};
};

struct TopdownMuzzleFlashEffect {
    bool active = false;

    Vector2 position{};
    Vector2 direction{1.0f, 0.0f};

    float ageMs = 0.0f;
    float lifetimeMs = 28.0f;

    float forwardLength = 42.0f;
    float sideWidth = 14.0f;

    bool anchoredToPlayer = false;
    TopdownCharacterHandle anchoredNpcHandle = -1;
    Vector2 localOffset{};
};

struct TopdownMuzzleSmokeParticle {
    bool active = false;

    Vector2 position{};
    Vector2 velocity{};

    float ageMs = 0.0f;
    float lifetimeMs = 220.0f;

    float size = 6.0f;
    float alpha = 1.0f;

    Color color = Color{210, 210, 210, 255};
};

struct TopdownBloodDecal {
    bool active = false;
    TopdownBloodDecalKind kind = TopdownBloodDecalKind::Spatter;

    Vector2 position{};

    float rotationRadians = 0.0f;

    float radius = 20.0f;
    float targetRadius = 20.0f;
    float growthRate = 0.0f;

    float opacity = 1.0f;
    float ageMs = 0.0f;

    float spawnOpacity = 1.0f;
    float fadeInMs = 0.0f;

    unsigned int variantSeed = 0;

    bool useGeneratedStamp = false;
    bool preferStreakStamp = false;
    int stampIndex = -1;
    float stretch = 1.0f;
};

struct TopdownBloodImpactParticle {
    bool active = false;

    Vector2 position{};
    Vector2 velocity{};

    float ageMs = 0.0f;
    float lifetimeMs = 140.0f;

    float size = 3.0f;
    float alpha = 1.0f;

    Color color = Color{170, 24, 24, 255};

    bool useGeneratedStamp = false;
    int stampIndex = -1;
    float rotationRadians = 0.0f;
    float stretch = 1.0f;
};

enum class TopdownBloodFxProfile {
    Default,
    Knife,
    Handgun,
    Rifle,
    Shotgun,
    NpcMelee
};

struct TopdownBloodEffectConfig {
    TopdownBloodFxProfile profile = TopdownBloodFxProfile::Default;

    int bloodImpactParticleCount = 8;
    float bloodImpactParticleSpeedMin = 50.0f;
    float bloodImpactParticleSpeedMax = 140.0f;
    float bloodImpactParticleLifetimeMsMin = 90.0f;
    float bloodImpactParticleLifetimeMsMax = 180.0f;
    float bloodImpactParticleSizeMin = 2.0f;
    float bloodImpactParticleSizeMax = 5.0f;
    float bloodImpactSpreadDegrees = 80.0f;

    int bloodDecalCountMin = 4;
    int bloodDecalCountMax = 7;
    float bloodDecalDistanceMin = 18.0f;
    float bloodDecalDistanceMax = 110.0f;
    float bloodDecalRadiusMin = 8.0f;
    float bloodDecalRadiusMax = 18.0f;
    float bloodDecalSpreadDegrees = 85.0f;
    float bloodDecalWallPadding = 6.0f;
    float bloodDecalOpacityMin = 0.72f;
    float bloodDecalOpacityMax = 0.95f;
};

struct TopdownNpcAttackEffectsConfig {
    TopdownBloodEffectConfig bloodEffects{
            TopdownBloodFxProfile::NpcMelee,
            8,
            45.0f,
            120.0f,
            180.0f,
            320.0f,
            2.5f,
            5.0f,
            70.0f,
            2,
            4,
            8.0f,
            55.0f,
            7.0f,
            14.0f,
            45.0f,
            6.0f,
            0.75f,
            0.95f};
};

struct TopdownBallisticImpactEffectConfig {
    int wallImpactParticleCount = 6;
    float wallImpactParticleSpeedMin = 70.0f;
    float wallImpactParticleSpeedMax = 180.0f;
    float wallImpactParticleLifetimeMsMin = 120.0f;
    float wallImpactParticleLifetimeMsMax = 260.0f;
    float wallImpactParticleSizeMin = 2.0f;
    float wallImpactParticleSizeMax = 5.0f;
    float wallImpactSpreadDegrees = 65.0f;
};

struct TopdownMuzzleEffectConfig {
    float muzzleX = 0.0f;
    float muzzleY = 0.0f;

    float muzzleFlashLifetimeMs = 28.0f;
    float muzzleFlashForwardLength = 42.0f;
    float muzzleFlashSideWidth = 14.0f;

    int muzzleSmokeParticleCount = 3;
    float muzzleSmokeSpeedMin = 18.0f;
    float muzzleSmokeSpeedMax = 55.0f;
    float muzzleSmokeLifetimeMsMin = 180.0f;
    float muzzleSmokeLifetimeMsMax = 320.0f;
    float muzzleSmokeSizeMin = 4.0f;
    float muzzleSmokeSizeMax = 9.0f;
    float muzzleSmokeSpreadDegrees = 85.0f;
    float muzzleSmokeForwardBias = 0.35f;
};

struct TopdownPendingBloodDecalSpawn {
    bool active = false;
    Vector2 hitPoint{};
    Vector2 incomingShotDir{};
    TopdownBloodEffectConfig bloodEffectConfig{};
    float delayMs = 0.0f;
    float elapsedMs = 0.0f;
};

struct TopdownBloodPoolEmitter {
    bool active = false;

    Vector2 position{};

    float elapsedMs = 0.0f;
    float durationMs = 4000.0f;

    float spawnIntervalMs = 90.0f;
    float spawnTimerMs = 0.0f;

    float maxRadius = 70.0f;
};

struct TopdownRenderWorld {
    std::vector<TopdownRuntimeImageLayer> bottomLayers;
    std::vector<TopdownRuntimeImageLayer> topLayers;

    std::vector<TopdownRuntimeEffectRegion> effectRegions;
    std::vector<TopdownBloodDecal> bloodDecals;
    std::vector<TopdownBloodImpactParticle> bloodImpactParticles;
    std::vector<TopdownTracerEffect> tracers;
    std::vector<TopdownWallImpactParticle> wallImpactParticles;
    std::vector<TopdownMuzzleFlashEffect> muzzleFlashes;
    std::vector<TopdownMuzzleSmokeParticle> muzzleSmokeParticles;
    std::vector<TopdownWindowGlassParticle> windowGlassParticles;

    std::vector<TopdownPendingBloodDecalSpawn> pendingBloodDecalSpawns;
    std::vector<TopdownBloodPoolEmitter> bloodPoolEmitters;

    std::vector<int> afterBottomEffectRegionIndices;
    std::vector<int> afterCharactersEffectRegionIndices;
    std::vector<int> finalEffectRegionIndices;

    std::vector<int> afterBottomPropIndices;
    std::vector<int> afterCharactersPropIndices;
    std::vector<int> finalPropIndices;

    int nextImageLayerHandle = 1;
    int nextEffectRegionHandle = 1;

    bool hasOcclusionRebuildCameraCache = false;
    Vector2 occlusionRebuildLastCamera{};
    std::vector<float> occlusionRebuildLastDoorAngles;
};

struct TopdownScreenShakeState {
    bool active = false;

    float durationMs = 0.0f;
    float elapsedMs = 0.0f;

    float strengthX = 0.0f;
    float strengthY = 0.0f;

    float frequencyHz = 30.0f;
    float sampleTimerMs = 0.0f;

    bool smooth = false;
    Vector2 previousOffset{};
    Vector2 sampledOffset{};
    Vector2 currentOffset{};
};

struct TopdownBloodRenderTarget {
    RenderTexture2D target{};
    RenderTexture2D blurredTarget{};
    bool loaded = false;

    int width = 0;
    int height = 0;

    bool dirty = true;

    bool hasLastCameraPosition = false;
    Vector2 lastCameraPosition{};
};
