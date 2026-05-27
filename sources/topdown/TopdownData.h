#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "resources/ResourceData.h"
#include "ui/NarrationPopupsData.h"
#include "topdown/TopdownCollisionData.h"
#include "topdown/TopdownCoreData.h"
#include "topdown/TopdownDoorData.h"
#include "topdown/TopdownItemData.h"
#include "topdown/TopdownLevelObjectData.h"
#include "topdown/TopdownNavData.h"
#include "topdown/TopdownNpcData.h"
#include "topdown/TopdownPlayerData.h"
#include "topdown/TopdownPropData.h"
#include "topdown/TopdownRenderData.h"
#include "topdown/TopdownTriggerData.h"
#include "topdown/TopdownWindowData.h"

enum class TopdownGameOverState {
    None,
    FadingIn,
    WaitingForMenu
};

enum class TopdownSpeechBubbleAnchorType {
    Player,
    Npc,
    Prop
};

struct TopdownSpeechBubbleEntry {
    bool active = false;

    TopdownSpeechBubbleAnchorType anchorType = TopdownSpeechBubbleAnchorType::Player;

    // For Player, anchorId can be empty.
    // For Npc and Prop, this is the runtime string id.
    std::string anchorId;

    std::string text;
    Color color = BLACK;

    float elapsedMs = 0.0f;
    float durationMs = 0.0f;
};

struct TopdownSpeechBubbleRuntime {
    std::vector<TopdownSpeechBubbleEntry> entries;
};

enum class TopdownObjectiveAnchorType {
    None,
    Trigger,
    Npc,
    Prop,
    Position
};

struct TopdownObjectiveMarkerRuntime {
    bool active = false;
    TopdownObjectiveAnchorType anchorType = TopdownObjectiveAnchorType::None;
    std::string anchorId;
    Vector2 position{};
    bool hasResolvedPosition = false;
    float animTimerMs = 0.0f;
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
    std::vector<TopdownAuthoredProp> props;
    std::vector<TopdownAuthoredItem> items;
    std::vector<TopdownAuthoredSpawn> spawns;
    std::vector<TopdownAuthoredEffectRegion> effectRegions;
    std::vector<TopdownAuthoredTrigger> triggers;
    std::vector<TopdownAuthoredNpc> npcs;
    std::vector<TopdownAuthoredDoor> doors;
    std::vector<TopdownAuthoredWindow> windows;
    std::vector<TopdownAuthoredSoundEmitter> soundEmitters;
};

struct TopdownLevelRegistryEntry {
    std::string levelId;
    std::string saveName;
    std::string metadataFilePath;
    std::string tiledFilePath;
    std::string levelDirectoryPath;
    std::string loadScreenPath;
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

struct TopdownLoadScreenOverlay {
    bool active = false;
    bool waitingForInput = false;
    bool fadingOut = false;
    float opacity = 0.0f;
    float fadeTimerMs = 0.0f;
    float fadeDurationMs = 650.0f;
    float promptTimerMs = 0.0f;
    int baseAssetScale = 1;
    std::string texturePath;
    Texture2D texture{};
    bool textureLoaded = false;
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
    TopdownPlayerInventoryRuntime playerInventory;
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
    std::vector<TopdownRuntimeProp> props;
    std::vector<TopdownRuntimeItem> items;

    int nextTriggerHandle = 1;
    std::vector<TopdownRuntimeTrigger> triggers;
    std::vector<TopdownRuntimeDoor> doors;
    std::vector<TopdownRuntimeWindow> windows;

    std::vector<TopdownWorldEvent> worldEvents;
    TopdownSpeechBubbleRuntime speechBubbles;
    TopdownObjectiveMarkerRuntime objectiveMarker;
    TopdownNarrationPopupsRuntime narrationPopups;
    TopdownLoadScreenOverlay loadScreenOverlay;
    float timeMs = 0.0f; // global timer, advances each frame
};

struct TopdownData {
    TopdownCameraData camera;

    TopdownCharacterAssetData playerCharacterAsset;

    std::vector<TopdownNpcAssetDefinition> npcAssetRegistry;
    std::vector<TopdownNpcAssetRuntime> npcAssets;
    TopdownNpcColorPresetRegistry npcColorPresets;

    std::vector<TopdownLevelRegistryEntry> levelRegistry;

    TopdownBloodStampLibrary bloodStampLibrary;
    TopdownItemRegistry itemRegistry;
    TextureHandle objectiveMarkerTexture = -1;

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
