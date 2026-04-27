#pragma once

#include <string>
#include <vector>
#include "raylib.h"
#include "resources/ResourceData.h"
#include "render/EffectTypes.h"
#include "nav/NavMeshData.h"
#include "rvo2/RVOSimulator.h"
#include "ui/NarrationPopupsData.h"

using TopdownObstacleHandle = int;
using TopdownImageLayerHandle = int;
using TopdownCharacterHandle = int;

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

enum class TopdownTriggerAffects {
    Player,
    Npc,
    All
};

enum class TopdownBloodDecalKind {
    Spatter,
    Pool
};

enum class TopdownAttackType {
    None,
    Melee,
    Ranged
};

enum class TopdownAttackInput {
    Primary,
    Secondary
};

enum class TopdownTracerStyle {
    None,
    Handgun,
    Shotgun,
    Rifle
};

enum class TopdownPlayerAttackState {
    Idle,
    Recover
};

enum class TopdownFireMode {
    SemiAuto,
    FullAuto,
    Burst
};

enum class TopdownGameOverState {
    None,
    FadingIn,
    WaitingForMenu
};

enum class TopdownNpcAiMode {
    None,
    SeekAndDestroy,
    HoldAndFire
};

enum class TopdownNpcEngagementState {
    Unaware,       // no target, passive
    Reacting,      // directly detected player, but not yet allowed to execute combat behavior
    Investigating, // has stimulus / last known position, moving/searching
    Engaged        // has target, full combat allowed
};

struct TopdownNpcPerceptionResult {
    bool seesPlayer = false;
    bool hearsPlayer = false;
    bool detectsPlayer = false;

    bool heardGunshot = false;
    Vector2 detectedPlayerPosition{};
    Vector2 heardGunshotPosition{};
};

enum class TopdownNpcCombatState {
    None,
    Chase,
    Investigation,
    Attack,
    Recover,
    Search
};

enum class TopdownPlayerLifeState {
    Alive,
    Dying,
    Dead,
    GameOver
};

struct TopdownSegment {
    Vector2 a{};
    Vector2 b{};
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
    bool visible = true;
};

enum class TopdownDoorHingeSide {
    Left,
    Right,
    Top,
    Bottom
};

struct TopdownAuthoredDoor {
    int tiledObjectId = -1;
    std::string id;
    bool visible = true;

    Vector2 rectPosition{};
    Vector2 rectSize{};

    TopdownDoorHingeSide hingeSide = TopdownDoorHingeSide::Left;

    bool locked = false;

    bool autoClose = false;
    float autoCloseStrength = 6.0f;
    float damping = 5.0f;

    float swingMinDegrees = -90.0f;
    float swingMaxDegrees = 90.0f;

    std::string openSoundId;
    std::string closeSoundId;

    Color color = Color{92, 58, 34, 255};
    Color outlineColor = BLACK;
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

struct TopdownAuthoredTrigger {
    int tiledObjectId = -1;
    std::string id;

    bool usePolygon = false;
    std::vector<Vector2> polygon;
    Rectangle worldRect{};

    bool visible = true;
    std::string script;
    TopdownTriggerAffects affects = TopdownTriggerAffects::Player;
    bool repeat = false;
    float delayMs = 0.0f;
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

    int breakParticleCount = 24;
    float breakParticleSpeedMin = 80.0f;
    float breakParticleSpeedMax = 220.0f;
    float breakParticleLifetimeMsMin = 180.0f;
    float breakParticleLifetimeMsMax = 420.0f;
    float breakParticleSizeMin = 2.0f;
    float breakParticleSizeMax = 4.5f;
    float breakParticleSpreadAlongWindow = 26.0f;

    Color breakParticleColor1 = Color{210, 240, 250, 255};
    Color breakParticleColor2 = Color{160, 210, 230, 255};
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

    int breakParticleCount = 24;
    float breakParticleSpeedMin = 80.0f;
    float breakParticleSpeedMax = 220.0f;
    float breakParticleLifetimeMsMin = 180.0f;
    float breakParticleLifetimeMsMax = 420.0f;
    float breakParticleSizeMin = 2.0f;
    float breakParticleSizeMax = 4.5f;
    float breakParticleSpreadAlongWindow = 26.0f;

    Color breakParticleColor1 = Color{210, 240, 250, 255};
    Color breakParticleColor2 = Color{160, 210, 230, 255};
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

struct TopdownAuthoredLevelData {
    bool loaded = false;

    std::string levelId;
    std::string saveName;
    std::string tiledFilePath;

    int baseAssetScale = 1;

    std::vector<Vector2> levelBoundary;
    std::vector<TopdownAuthoredPolygon> obstacles;
    std::vector<TopdownAuthoredImageLayer> imageLayers;
    std::vector<TopdownAuthoredSpawn> spawns;
    std::vector<TopdownAuthoredEffectRegion> effectRegions;
    std::vector<TopdownAuthoredTrigger> triggers;
    std::vector<TopdownAuthoredNpc> npcs;
    std::vector<TopdownAuthoredDoor> doors;
    std::vector<TopdownAuthoredWindow> windows;
    std::vector<TopdownAuthoredSoundEmitter> soundEmitters;
};

struct TopdownRuntimeObstacle {
    TopdownObstacleHandle handle = -1;
    int tiledObjectId = -1;
    TopdownObstacleKind kind = TopdownObstacleKind::MovementAndVision;
    std::string name;

    std::vector<Vector2> polygon;
    std::vector<TopdownSegment> edges;

    Rectangle bounds{};
    bool visible = true;
};

struct TopdownRuntimeDoor {
    int tiledObjectId = -1;
    std::string id;
    bool visible = true;

    Vector2 hinge{};
    float length = 0.0f;
    float thickness = 0.0f;

    float closedAngleRadians = 0.0f;
    float angleRadians = 0.0f;
    float angularVelocity = 0.0f;

    float swingMinRadians = -90.0f * DEG2RAD;
    float swingMaxRadians = 90.0f * DEG2RAD;

    bool locked = false;

    bool autoClose = false;
    float autoCloseStrength = 6.0f;
    float damping = 5.0f;

    std::string openSoundId;
    std::string closeSoundId;
    bool wasNearClosed = true;
    bool openSoundPlayedThisSwing = false;
    Color color = Color{92, 58, 34, 255};
    Color outlineColor = BLACK;
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
};

using TopdownTriggerHandle = int;

struct TopdownRuntimeTriggerPendingCall {
    bool active = false;
    int authoredIndex = -1;
    TopdownCharacterHandle instigatorHandle = -1;
    bool instigatorIsPlayer = false;
    float remainingMs = 0.0f;
};

struct TopdownRuntimeTrigger {
    TopdownTriggerHandle handle = -1;
    int authoredIndex = -1;

    bool enabled = true;
    bool repeat = false;
    bool fired = false;

    bool playerInside = false;
    std::vector<TopdownCharacterHandle> npcHandlesInside;

    std::vector<TopdownRuntimeTriggerPendingCall> pendingCalls;
};

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

struct TopdownSpatialCell {
    std::vector<int> movementSegmentIndices;
    std::vector<int> visionSegmentIndices;
    std::vector<int> obstacleIndices;
};

struct TopdownSpatialGrid {
    bool built = false;

    Vector2 origin{};
    float cellSize = 128.0f;

    int width = 0;
    int height = 0;

    std::vector<TopdownSpatialCell> cells;
};

struct TopdownCollisionWorld {
    std::vector<TopdownRuntimeObstacle> obstacles;

    std::vector<TopdownSegment> movementSegments;
    std::vector<TopdownSegment> visionSegments;
    std::vector<TopdownSegment> boundarySegments;

    TopdownSpatialGrid spatialGrid;

    int nextObstacleHandle = 1;
};

struct TopdownNavWorld {
    bool valid = false;

    std::vector<Vector2> levelBoundary;
    std::vector<std::vector<Vector2>> holePolygons;

    NavMeshData navMesh;
    float agentRadius = 0.0f;
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

struct TopdownPlayerWeaponConfig {
    std::string equipmentSetId;
    int slot = 0;

    TopdownAttackType primaryAttackType = TopdownAttackType::None;
    TopdownAttackType secondaryAttackType = TopdownAttackType::None;

    float primaryCooldownMs = 0.0f;
    float secondaryCooldownMs = 0.0f;

    float rangedDamage = 0.0f;
    float meleeDamage = 0.0f;

    float maxRange = 0.0f;

    int pelletCount = 1;
    float spreadDegrees = 0.0f;

    float meleeRange = 0.0f;
    float meleeArcDegrees = 0.0f;

    float rangedKnockback = 0.0f;
    float meleeKnockback = 0.0f;

    float rangedDoorImpulse = 0.0f;
    float meleeDoorImpulse = 0.0f;
    float noiseRadius = 1200.0f;

    TopdownTracerStyle tracerStyle = TopdownTracerStyle::None;

    TopdownBallisticImpactEffectConfig ballisticImpactEffects{};
    TopdownMuzzleEffectConfig muzzleEffects{};
    TopdownBloodEffectConfig bloodEffects{};

    std::vector<TopdownFireMode> supportedFireModes;
    TopdownFireMode defaultFireMode = TopdownFireMode::SemiAuto;

    int burstCount = 3;
    float burstIntervalMs = 70.0f;
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

    int nextImageLayerHandle = 1;
    int nextEffectRegionHandle = 1;

    bool hasOcclusionRebuildCameraCache = false;
    Vector2 occlusionRebuildLastCamera{};
    std::vector<float> occlusionRebuildLastDoorAngles;
};

struct TopdownPlayerRuntime {
    Vector2 position{};
    Vector2 velocity{};
    Vector2 desiredVelocity{};
    Vector2 facing{1.0f, 0.0f};

    float radius = 45.0f;

    float walkSpeed = 550.0f;
    float runSpeed = 750.0f;

    float acceleration = 2800.0f;
    float deceleration = 4500.0f;

    bool wantsRun = false;

    float moveInputForward = 0.0f;
    float moveInputRight = 0.0f;

    float health = 100.0f;
    float maxHealth = 100.0f;

    float hurtCooldownRemainingMs = 0.0f;

    float hitSlowdownRemainingMs = 0.0f;
    float hitSlowdownMultiplier = 1.0f;

    float damageFlashRemainingMs = 0.0f;
    float lowHealthEffectWeight = 0.0f;

    TopdownPlayerLifeState lifeState = TopdownPlayerLifeState::Alive;
};

struct TopdownCameraData {
    float viewportWidth = 1920.0f;
    float viewportHeight = 1080.0f;

    float deadzoneWidth = 160.0f;
    float deadzoneHeight = 40.0f;

    float aimMaxOffset = 1200.0f;
    float aimStrength = 0.30f;
    float aimResponse = 8.0f;

    float smoothing = 10.0f;
};

enum class TopdownCameraMode {
    Player,
    Scripted,
    Manual
};

struct TopdownCameraRuntime {
    Vector2 position{};
    Vector2 targetPosition{};
    Vector2 aimOffset{};

    TopdownCameraMode mode = TopdownCameraMode::Player;

    Vector2 scriptedTarget{};
    Vector2 panStart{};
    Vector2 panEnd{};
    float panTimerMs = 0.0f;
    float panDurationMs = 0.0f;
    bool isPanning = false;
};

enum class TopdownLocomotionType {
    Idle,
    Forward,
    Backward,
    StrafeLeft,
    StrafeRight
};

struct TopdownPlayerAnimationEntry {
    std::string id;
    SpriteAssetHandle spriteHandle = -1;

    bool hasMuzzle = false;
    Vector2 muzzle{};
};

struct TopdownCharacterAssetData {
    bool loaded = false;
    std::string id;

    float maxHealth = 100.0f;
    float hurtCooldownMs = 150.0f;
    float meleeHitSlowdownMs = 100.0f;
    float meleeHitSlowdownMultiplier = 0.65f;

    std::vector<TopdownPlayerAnimationEntry> animations;
    std::vector<TopdownPlayerWeaponConfig> weaponConfigs;
};

struct TopdownCharacterRuntime {
    bool active = false;

    std::string equippedSetId = "handgun";

    TopdownLocomotionType locomotion = TopdownLocomotionType::Idle;
    bool running = false;

    float bodyFacingRadians = 0.0f;
    float desiredAimRadians = 0.0f;
    float feetRotationRadians = 0.0f;
    float upperRotationRadians = 0.0f;

    float turnSpeedRadians = 7.0f;
    float maxUpperBodyTwistRadians = 85.0f * DEG2RAD;

    float minAimDistanceEnter = 24.0f;
    float minAimDistanceExit = 40.0f;
    bool aimFrozen = false;

    float feetAnimationTimeMs = 0.0f;
    float upperAnimationTimeMs = 0.0f;

    SpriteAssetHandle currentFeetHandle = -1;
    SpriteAssetHandle currentUpperHandle = -1;
};

struct TopdownPlayerAttackRuntime {
    bool active = false;

    TopdownPlayerAttackState state = TopdownPlayerAttackState::Idle;
    TopdownAttackInput input = TopdownAttackInput::Primary;
    TopdownAttackType attackType = TopdownAttackType::None;

    float stateTimeMs = 0.0f;
    float animationDurationMs = 0.0f;
    float cooldownRemainingMs = 0.0f;

    std::string equipmentSetId;

    TopdownFireMode currentFireMode = TopdownFireMode::SemiAuto;

    bool triggerHeld = false;
    bool wantsTriggerRelease = false;

    int burstShotsRemaining = 0;
    float burstShotTimerMs = 0.0f;

    bool pendingPrimaryAttack = false;
    bool pendingSecondaryAttack = false;

    float fullAutoShakeCooldownMs = 0.0f;

    bool meleeHitPending = false;
    bool meleeHitApplied = false;

    bool rifleLoopPlaying = false;
};

enum class TopdownNpcAnimationMode {
    AutomaticLocomotion,
    ScriptLoop
};

enum class TopdownNpcMoveOwner {
    None,
    Ai,
    ScriptCommand,
    Patrol
};

enum class TopdownNpcScriptBehaviorMode {
    None,
    PatrolRoute
};

struct TopdownNpcMoveState {
    bool active = false;
    bool running = false;
    TopdownNpcMoveOwner owner = TopdownNpcMoveOwner::None;

    std::vector<int> debugTrianglePath;
    std::vector<Vector2> pathPoints;
    int currentPoint = 0;

    Vector2 finalTarget{};
    bool hasFinalTarget = false;

    float currentSpeed = 0.0f;
    float acceleration = 1800.0f;
    float deceleration = 2200.0f;
    float arrivalRadius = 6.0f;
    float stopDistance = 140.0f;
};

struct TopdownNpcPatrolState {
    bool active = false;
    bool paused = false;
    bool interrupted = false;
    bool loop = true;
    bool running = false;

    std::vector<std::string> spawnIds;
    int currentPointIndex = 0;
    int contextHandle = -1;
    int slotIndex = -1;

    float waitDurationMs = 0.0f;
    float waitTimerMs = 0.0f;
};

struct TopdownNpcScriptBehaviorState {
    TopdownNpcScriptBehaviorMode mode = TopdownNpcScriptBehaviorMode::None;
    TopdownNpcPatrolState patrol;
};

struct TopdownNpcAnimationSourceDefinition {
    std::string asepriteJsonPath;
    bool hasOrigin = false;
    Vector2 origin{};
};

struct TopdownNpcAssetDefinition {
    std::string assetId;
    float baseDrawScale = 1.0f;
    float collisionRadius = 32.0f;

    float walkSpeed = 450.0f;
    float runSpeed = 700.0f;
    float hurtStunMs = 0.0f;
    float maxHealth = 100.0f;
    float corpseExpirationMs = -1.0f;

    bool hostile = true;
    TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;

    TopdownAttackType attackType;
    TopdownTracerStyle rangedTracerStyle = TopdownTracerStyle::Handgun;
    int rangedPelletCount = 1;
    float rangedSpreadDegrees = 6.0f;
    float rangedMaxRange = 800.0f;
    TopdownBallisticImpactEffectConfig ballisticImpactEffects{};
    TopdownMuzzleEffectConfig muzzleEffects{};
    float reactionTimeMs = 180.0f;
    float aimInaccuracyMinDegrees = 2.0f;
    float aimInaccuracyMaxDegrees = 10.0f;


    float visionRange = 700.0f;
    float hearingRange = 220.0f;
    float gunshotHearingRange = 1000.0f;
    float visionHalfAngleDegrees = 65.0f;

    float attackRange = 95.0f;
    float attackCooldownMs = 900.0f;
    float attackDamage = 25.0f;
    float attackHitNormalizedTime = 0.7f;
    float attackRecoverMs = 250.0f;

    float chaseRepathIntervalMs = 250.0f;

    float meleeHitPosX = 0.0f;
    float meleeHitPosY = 0.0f;
    std::string attackStartSoundId;
    std::string attackConnectSoundId;
    std::vector<std::string> hitReactionSoundIds;

    TopdownNpcAttackEffectsConfig attackEffects;

    std::vector<TopdownNpcAnimationSourceDefinition> animations;
};

struct TopdownNpcClipRef {
    SpriteAssetHandle spriteHandle = -1;
    int clipIndex = -1;
    std::string clipName;
};

struct TopdownNpcAssetRuntime {
    bool loaded = false;

    std::string assetId;

    TopdownNpcClipRef idleClip;
    TopdownNpcClipRef walkClip;
    TopdownNpcClipRef runClip;
    TopdownNpcClipRef hurtClip;
    TopdownNpcClipRef deathClip;
    TopdownNpcClipRef rangedAttackClip;
    TopdownNpcClipRef meleeAttackClip;

    std::vector<SpriteAssetHandle> spriteHandles;

    float baseDrawScale = 1.0f;
    float collisionRadius = 32.0f;

    float walkSpeed = 450.0f;
    float runSpeed = 700.0f;
    float hurtStunMs = 0.0f;
    float maxHealth = 100.0f;
    float corpseExpirationMs = -1.0f;

    bool hostile = true;
    TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;

    TopdownAttackType attackType;
    TopdownTracerStyle rangedTracerStyle = TopdownTracerStyle::Handgun;
    int rangedPelletCount = 1;
    float rangedSpreadDegrees = 6.0f;
    float rangedMaxRange = 800.0f;
    TopdownBallisticImpactEffectConfig ballisticImpactEffects{};
    TopdownMuzzleEffectConfig muzzleEffects{};
    float reactionTimeMs = 180.0f;
    float aimInaccuracyMinDegrees = 2.0f;
    float aimInaccuracyMaxDegrees = 10.0f;

    float visionRange = 700.0f;
    float hearingRange = 220.0f;
    float gunshotHearingRange = 1000.0f;
    float visionHalfAngleDegrees = 65.0f;

    float attackRange = 95.0f;
    float attackCooldownMs = 900.0f;
    float attackDamage = 25.0f;
    float attackHitNormalizedTime = 0.7f;
    float attackRecoverMs = 250.0f;

    float chaseRepathIntervalMs = 250.0f;

    float meleeHitPosX = 0.0f;
    float meleeHitPosY = 0.0f;
    std::string attackStartSoundId;
    std::string attackConnectSoundId;
    std::vector<std::string> hitReactionSoundIds;

    TopdownNpcAttackEffectsConfig attackEffects;
};


struct TopdownNpcRuntime {
    TopdownCharacterHandle handle = -1;

    std::string id;
    std::string assetId;

    bool active = false;
    bool visible = true;
    bool dead = false;
    bool corpse = false;
    bool hostile = true;
    bool persistentChase = false;

    TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;

    TopdownNpcEngagementState engagementState = TopdownNpcEngagementState::Unaware;
    TopdownNpcCombatState combatState = TopdownNpcCombatState::None;

    float health = 100.0f;
    float corpseExpirationMs = -1.0f;
    float corpseElapsedMs = 0.0f;

    float visionRange = 700.0f;
    float hearingRange = 220.0f;
    float gunshotHearingRange = 1000.0f;
    float visionHalfAngleDegrees = 65.0f;

    float attackRange = 95.0f;
    float preferredAttackRangeFactor = 0.9f;
    float attackCooldownMs = 900.0f;
    float attackCooldownRemainingMs = 0.0f;
    float attackDamage = 25.0f;
    float attackHitNormalizedTime = 0.7f;
    float attackRecoverMs = 250.0f;

    float chaseRepathIntervalMs = 250.0f;

    bool hasPlayerTarget = false;
    Vector2 lastKnownPlayerPosition{};
    Vector2 investigationPosition{};

    float repathTimerMs = 0.0f;

    bool attackHitPending = false;
    bool attackHitApplied = false;
    float attackStateTimeMs = 0.0f;
    float attackAnimationDurationMs = 0.0f;

    float searchStateTimeMs = 0.0f;
    float searchDurationMs = 1600.0f;
    float searchBaseFacingRadians = 0.0f;
    float searchSweepDegrees = 300.0f;

    float renderOpacity = 1.0f;

    Vector2 position{};
    Vector2 facing{1.0f, 0.0f};
    Vector2 currentVelocity{};

    float collisionRadius = 32.0f;

    float rotationRadians = 0.0f;

    TopdownNpcAnimationMode animationMode = TopdownNpcAnimationMode::AutomaticLocomotion;

    TopdownNpcClipRef automaticLoopClip;
    float automaticLoopTimeMs = 0.0f;

    TopdownNpcClipRef scriptLoopClip;
    float scriptLoopTimeMs = 0.0f;

    bool oneShotActive = false;
    TopdownNpcClipRef oneShotClip;
    float oneShotTimeMs = 0.0f;

    float hurtStunRemainingMs = 0.0f;

    Vector2 knockbackVelocity{};
    float knockbackDeceleration = 5000.0f;

    TopdownNpcMoveState move;
    TopdownNpcScriptBehaviorState scriptBehavior;

    bool moving = false;
    bool running = false;
    float painSoundCooldownMs = 0.0f;

    float meleeHitPosX = 0.0f;
    float meleeHitPosY = 0.0f;
    std::string attackStartSoundId;
    std::string attackConnectSoundId;
    std::vector<std::string> hitReactionSoundIds;

    TopdownNpcAttackEffectsConfig attackEffects;

    float chaseStuckTimerMs = 0.0f;
    Vector2 chaseStuckLastPosition{};

    Vector2 patrolLastProgressPosition{};
    float patrolStuckTimerMs = 0.0f;
    float patrolYieldTimerMs = 0.0f;
    float patrolRetryDelayMs = 0.0f;
    int patrolStuckCount = 0;
    bool patrolIsYielding = false;
    bool patrolIsRetryDelay = false;

    int investigationContextHandle = -1;
    int investigationSlotIndex = -1;
    float investigationProgressTimerMs = 0.0f;
    Vector2 investigationLastPosition{};
    float investigationRetargetCooldownMs = 300;

    float reactionTimerMs = 0.0f;
    bool hasReactedToPlayer = false;

    int strafeDir = 1;
    float strafeTimerMs = 0.0f;
    float engagedLostTargetTimerMs = 0.0f;
};

struct TopdownNpcInvestigationSlot {
    Vector2 position{};
    int claimedByNpcHandle = -1;
};

struct TopdownNpcPatrolSlot {
    Vector2 position{};
    int claimedByNpcHandle = -1;
};

struct TopdownNpcPatrolContext {
    bool active = false;
    int handle = -1;
    std::string waypointSpawnId;
    Vector2 origin{};
    std::vector<TopdownNpcPatrolSlot> slots;
};

struct TopdownNpcInvestigationContext {
    bool active = false;
    int handle = -1;
    Vector2 origin{};
    std::vector<TopdownNpcInvestigationSlot> slots;
};

struct TopdownLevelRegistryEntry {
    std::string levelId;
    std::string saveName;
    std::string metadataFilePath;
    std::string tiledFilePath;
    std::string levelDirectoryPath;
    int baseAssetScale = 1;
};

struct TopdownDebugData {
    bool showBlockers = false;
    bool showTriggers = false;
    bool showNav = false;
    bool showPlayer = false;
    bool showSpawnPoints = false;
    bool showEffects = false;
    bool showImageLayers = false;
    bool showScriptDebug = false;
    bool showCombatDebug = false;
    bool showAiDebug = false;
    bool showDoors = false;
};

struct TopdownScriptMoveState {
    bool active = false;
    bool running = false;

    std::vector<Vector2> pathPoints;
    int currentPoint = 0;

    float currentSpeed = 0.0f;
    float acceleration = 1800.0f;
    float deceleration = 2200.0f;
    float arrivalRadius = 6.0f;
    float stopDistance = 140.0f;
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

struct TopdownRvoAgent {
    int npcHandle = -1;
    size_t rvoId = RVO::RVO_ERROR;
};

struct TopdownRvoState {
    bool initialized = false;
    RVO::RVOSimulator* sim = nullptr;

    std::vector<TopdownRvoAgent> agents;

    bool obstaclesBuilt = false;
    bool rebuildRequested = false;

    bool hasPlayerAgent = false;
    size_t playerRvoId = RVO::RVO_ERROR;
};

enum class TopdownWorldEventType {
    Gunshot,
    Explosion,
    Footstep,
    Impact,
    AllyDown
};

enum class TopdownWorldEventSourceType {
    None,
    Player,
    Npc,
    System
};

struct TopdownWorldEvent {
    TopdownWorldEventType type{};
    Vector2 position{};
    float radius = 0.0f;
    float createdAtMs = 0.0f;
    float ttlMs = 0.0f;

    TopdownWorldEventSourceType sourceType = TopdownWorldEventSourceType::None;
    int sourceNpcHandle = -1; // only valid if sourceType == Npc
};

struct TopdownRuntimeData {
    bool levelActive = false;
    bool controlsEnabled = true;

    bool aiFrozen = false;
    bool godMode = false;

    bool gameOverActive = false;
    float gameOverElapsedMs = 0.0f;
    bool returnToMenuRequested = false;

    TopdownCollisionWorld collision;
    TopdownNavWorld nav;
    TopdownRenderWorld render;

    TopdownPlayerRuntime player;
    TopdownCharacterRuntime playerCharacter;
    TopdownPlayerAttackRuntime playerAttack;
    TopdownCameraRuntime camera;
    TopdownDebugData debug;

    TopdownScriptMoveState scriptedMove;

    std::vector<TopdownNpcRuntime> npcs;

    int nextNpcHandle = 1;
    TopdownScreenShakeState screenShake{};
    TopdownBloodRenderTarget bloodRenderTarget{};

    TopdownRvoState rvo;
    int nextNpcInvestigationContextHandle = 1;
    std::vector<TopdownNpcInvestigationContext> npcInvestigations;
    int nextNpcPatrolContextHandle = 1;
    std::vector<TopdownNpcPatrolContext> npcPatrolContexts;
    int nextTriggerHandle = 1;
    std::vector<TopdownRuntimeTrigger> triggers;
    std::vector<TopdownRuntimeDoor> doors;
    std::vector<TopdownRuntimeWindow> windows;

    std::vector<TopdownWorldEvent> worldEvents;
    TopdownNarrationPopupsRuntime narrationPopups;
    float timeMs; // global timer, advances each frame
};

struct TopdownData {
    TopdownCameraData camera;

    TopdownCharacterAssetData playerCharacterAsset;

    std::vector<TopdownNpcAssetDefinition> npcAssetRegistry;
    std::vector<TopdownNpcAssetRuntime> npcAssets;

    std::vector<TopdownLevelRegistryEntry> levelRegistry;

    TopdownBloodStampLibrary bloodStampLibrary;

    std::string currentLevelId;
    std::string currentLevelSaveName;
    std::string currentLevelTiledFilePath;
    std::string currentLevelScriptFilePath;
    int currentLevelBaseAssetScale = 1;

    TopdownAuthoredLevelData authored;
    TopdownRuntimeData runtime;

    bool hasPendingLevelChange = false;
    std::string pendingLevelId;
    std::string pendingSpawnId;
};
