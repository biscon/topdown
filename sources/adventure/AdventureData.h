#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include "raylib.h"
#include "scene/SceneData.h"
#include "resources/ResourceData.h"
#include "adventure/AdventureActionData.h"
#include "adventure/InventoryData.h"
#include "utils/Interpolation.h"
#include "render/EffectTypes.h"

using ActorHandle = int;

enum class ActorFacing {
    Front,
    Back,
    Right,
    Left
};

enum class PendingInteractionType {
    None,
    UseHotspot,
    LookHotspot,
    UseExit,
    LookExit,
    UseActor,
    LookActor
};

struct ActorPath {
    std::vector<Vector2> points;
    int currentPoint = 0;
    bool active = false;
    bool fastMove = false;
};

struct ActorDefinitionData {
    std::string actorId;
    std::string displayName;

    SpriteAssetHandle spriteAssetHandle = -1;
    Color talkColor = WHITE;

    float walkSpeed = 240.0f;
    float fastMoveMultiplier = 2.0f;
    float idleDelayMs = 200.0f;

    bool controllable = false;

    float horizHitboxScale = 1.0f;
    float vertHitboxScale = 1.0f;
};

struct ActorInstance {
    ActorHandle handle = -1;

    std::string actorId;
    int actorDefIndex = -1;

    bool activeInScene = false;
    bool visible = true;

    Vector2 feetPos{};
    ActorFacing facing = ActorFacing::Front;

    std::string currentAnimation = "idle_front";
    bool flipX = false;

    float animationTimeMs = 0.0f;

    float walkSpeed = 240.0f;
    float fastMoveMultiplier = 2.0f;

    float depthScale = 1.0f;

    float idleDelayMs = 200.0f;
    float stoppedTimeMs = 0.0f;
    bool inIdleState = true;

    ActorPath path{};
    bool scriptAnimationActive = false;
    float scriptAnimationDurationMs = 0.0f;
};

using PropHandle = int;

struct PropInstance {
    PropHandle handle = -1;
    int scenePropIndex = -1;

    Vector2 feetPos{};

    bool visible = true;
    bool flipX = false;

    std::string currentAnimation;
    float animationTimeMs = 0.0f;

    bool oneShotActive = false;
    float oneShotDurationMs = 0.0f;

    bool moveActive = false;
    Vector2 moveStartPos{};
    Vector2 moveTargetPos{};
    float moveElapsedMs = 0.0f;
    float moveDurationMs = 0.0f;
    MoveInterpolation moveInterpolation = MoveInterpolation::Linear;
};

using EffectSpriteHandle = int;

struct EffectSpriteInstance {
    EffectSpriteHandle handle = -1;
    int sceneEffectSpriteIndex = -1;

    bool visible = true;
    float opacity = 1.0f;
    Color tint = WHITE;

    EffectShaderType shaderType = EffectShaderType::None;
    EffectShaderParams shaderParams{};
};

using EffectRegionHandle = int;

struct EffectRegionInstance {
    EffectRegionHandle handle = -1;
    int sceneEffectRegionIndex = -1;

    bool visible = true;
    float opacity = 1.0f;
    Color tint = WHITE;

    EffectShaderType shaderType = EffectShaderType::None;
    EffectShaderParams shaderParams{};
};

enum class CameraModeData
{
    FollowActor,
    FollowControlledActor,
    Scripted
};

enum class CameraBiasLatch
{
    None,
    Left,
    Right
};

struct CameraData {
    Vector2 position{};
    float viewportWidth = 1920.0f;
    float viewportHeight = 1080.0f;

    CameraModeData mode = CameraModeData::FollowControlledActor;
    ActorHandle followedActor = -1;

    // scripted movement
    bool moving = false;

    Vector2 moveStart{};
    Vector2 moveTarget{};
    float moveDurationMs = 0.0f;
    float moveElapsedMs = 0.0f;

    MoveInterpolation interpolation = MoveInterpolation::AccelerateDecelerate;

    float followDeadZoneWidth = 260.0f;
    float followDeadZoneHeight = 120.0f;
    float followSmoothing = 4.0f; // 0 = snap

    CameraBiasLatch biasLatch = CameraBiasLatch::None;
    float followBiasX = 240.0f;
    float currentBiasShiftX = 0.0f;
    float biasShiftSmoothing = 4.0f;
};

struct PendingInteraction {
    PendingInteractionType type = PendingInteractionType::None;
    int targetIndex = -1;
    bool active = false;
};

enum class SpeechAnchorType {
    Player,
    Actor,
    Prop,
    Position
};

struct SpeechUiState {
    bool active = false;

    SpeechAnchorType anchorType = SpeechAnchorType::Player;
    int propIndex = -1;
    Vector2 worldPos{};

    int actorIndex = -1;

    std::string text;
    float timerMs = 0.0f;
    float durationMs = 0.0f;
    Color color = WHITE;
    bool skippable = true;

    float fadeInMs = 0.0f;
    float fadeOutMs = 0.0f;
};

struct HoverUiState {
    bool active = false;
    std::string displayName;
};

struct DialogueChoiceOptionData {
    std::string id;
    std::string text;
};

struct DialogueChoiceSetData {
    std::string id;
    std::vector<DialogueChoiceOptionData> options;
};

struct DialogueUiState {
    bool active = false;
    int activeChoiceSetIndex = -1;
    int hoveredOptionIndex = -1;

    bool resultReady = false;
    std::string selectedOptionId;
    std::unordered_set<std::string> hiddenOptionIds;
};

struct ScreenShakeState {
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

enum class SceneFadePhase {
    None,
    FadingOut,
    FadingIn
};

struct SceneFadeState {
    SceneFadePhase phase = SceneFadePhase::None;

    float durationMs = 300.0f;
    float elapsedMs = 0.0f;
    float opacity = 0.0f;

    bool loadTriggered = false;
};

struct AdventureData {
    std::string pendingSceneId;
    std::string pendingSpawnId;
    bool hasPendingSceneLoad = false;

    SceneData currentScene{};

    std::vector<ActorDefinitionData> actorDefinitions;
    std::vector<ActorInstance> actors;
    int controlledActorIndex = -1;

    CameraData camera{};

    int nextActorHandle = 1;

    std::vector<PropInstance> props;
    int nextPropHandle = 1;

    std::vector<EffectSpriteInstance> effectSprites;
    int nextEffectSpriteHandle = 1;

    std::vector<EffectRegionInstance> effectRegions;
    int nextEffectRegionHandle = 1;

    std::vector<ItemDefinitionData> itemDefinitions;
    std::vector<ActorInventoryData> actorInventories;
    InventoryUiState inventoryUi{};

    std::vector<DialogueChoiceSetData> dialogueChoiceSets;
    DialogueUiState dialogueUi{};

    Vector2 lastClickWorldPos{};
    bool hasLastClickWorldPos = false;

    Vector2 lastResolvedTargetPos{};
    bool hasLastResolvedTargetPos = false;

    std::vector<int> debugTrianglePath;

    PendingInteraction pendingInteraction{};

    SpeechUiState speechUi{};
    std::vector<SpeechUiState> ambientSpeechUis{};
    HoverUiState hoverUi{};
    AdventureActionQueue actionQueue{};
    ScreenShakeState screenShake{};
    SceneFadeState sceneFade{};
    bool fadeInputBlocked = false;
    bool controlsEnabled = true;
};
