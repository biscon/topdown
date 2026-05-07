#include "save/SaveGame.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <exception>
#include <unordered_map>
#include <vector>

#include "utils/json.hpp"
#include "debug/DebugConsole.h"
#include "resources/Resources.h"
#include "resources/AsepriteAsset.h"
#include "scripting/ScriptSystem.h"
#include "topdown/LevelLoad.h"
#include "topdown/BloodRenderTarget.h"
#include "topdown/LevelRegistry.h"
#include "topdown/LevelScripting.h"
#include "topdown/NpcRegistry.h"
#include "topdown/PlayerRegistry.h"
#include "topdown/TopdownNpcPatrol.h"
#include "topdown/TopdownRvo.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static constexpr int SAVE_VERSION = 5;


    struct SavedMusicSlotState {
        std::string audioId;
        bool playing = false;
        float positionSeconds = 0.0f;
        float volume = 0.0f;
        float targetVolume = 1.0f;
        int fadeMode = 0;
        float fadeElapsed = 0.0f;
        float fadeDuration = 0.0f;
    };

    struct SavedAudioState {
        SavedMusicSlotState musicA;
        SavedMusicSlotState musicB;
        bool musicAIsCurrent = true;
    };


    struct SavedPlayerRuntime {
        Vector2 position{};
        Vector2 velocity{};
        Vector2 facing{1.0f, 0.0f};

        float health = 100.0f;
        float maxHealth = 100.0f;

        float hurtCooldownRemainingMs = 0.0f;
        float hitSlowdownRemainingMs = 0.0f;
        float hitSlowdownMultiplier = 1.0f;
        float damageFlashRemainingMs = 0.0f;
        float lowHealthEffectWeight = 0.0f;

        TopdownPlayerLifeState lifeState = TopdownPlayerLifeState::Alive;
    };

    struct SavedPlayerCharacterRuntime {
        bool active = false;
        std::string equippedSetId;

        TopdownLocomotionType locomotion = TopdownLocomotionType::Idle;
        bool running = false;

        float bodyFacingRadians = 0.0f;
        float desiredAimRadians = 0.0f;
        float feetRotationRadians = 0.0f;
        float upperRotationRadians = 0.0f;

        bool aimFrozen = false;

        float feetAnimationTimeMs = 0.0f;
        float upperAnimationTimeMs = 0.0f;
    };

    struct SavedPlayerAttackRuntime {
        std::string equipmentSetId;
        TopdownFireMode currentFireMode = TopdownFireMode::SemiAuto;
        float cooldownRemainingMs = 0.0f;
    };

    struct SavedCameraRuntime {
        Vector2 position{};
        Vector2 targetPosition{};
        Vector2 aimOffset{};

        TopdownCameraMode mode = TopdownCameraMode::Player;
        Vector2 scriptedTarget{};
    };

    struct SavedTopdownCoreRuntime {
        bool controlsEnabled = true;
        float timeMs = 0.0f;

        SavedPlayerRuntime player;
        SavedPlayerCharacterRuntime playerCharacter;
        SavedPlayerAttackRuntime playerAttack;
        SavedCameraRuntime camera;
    };

    struct SavedNpcPatrolRuntime {
        bool active = false;
        bool paused = false;
        bool loop = true;
        bool running = false;

        float waitMs = 0.0f;
        float waitRemainingMs = 0.0f;

        int currentPoint = 0;

        std::vector<std::string> routeSpawnIds;
    };

    struct SavedNpcRuntime {
        std::string id;
        std::string assetId;

        int handle = -1;

        bool active = true;
        bool visible = true;
        bool dead = false;
        bool corpse = false;

        Vector2 position{};
        Vector2 facing{1.0f, 0.0f};
        Vector2 currentVelocity{};

        float rotationRadians = 0.0f;
        float health = 100.0f;
        float corpseElapsedMs = 0.0f;

        bool hostile = true;
        bool persistentChase = false;
        bool guard = false;

        TopdownNpcAiMode aiMode = TopdownNpcAiMode::None;
        TopdownNpcEngagementState engagementState = TopdownNpcEngagementState::Unaware;

        Vector2 guardHomePosition{};
        bool hasGuardHomePosition = false;
        float guardLookAtSoundRadians = 0.0f;

        SavedNpcPatrolRuntime patrol;
    };

    struct SavedTopdownNpcsRuntime {
        std::vector<SavedNpcRuntime> npcs;
    };


    struct SavedDoorRuntime {
        std::string id;
        bool visible = true;
        bool locked = false;
        float angleRadians = 0.0f;
        float angularVelocity = 0.0f;
        bool wasNearClosed = true;
        bool openSoundPlayedThisSwing = false;
    };

    struct SavedWindowRuntime {
        std::string id;
        bool visible = true;
        bool broken = false;
    };

    struct SavedPropRuntime {
        std::string id;
        bool active = false;
        bool visible = true;
        Vector2 position{};
        float opacity = 1.0f;
        std::string currentAnimation;
        std::string baseAnimation;
        float animationTimeMs = 0.0f;
        bool oneShotActive = false;
        std::string oneShotAnimation;
        float oneShotDurationMs = 0.0f;
        bool moving = false;
        Vector2 moveStart{};
        Vector2 moveEnd{};
        float moveTimerMs = 0.0f;
        float moveDurationMs = 0.0f;
        MoveInterpolation moveInterpolation = MoveInterpolation::Linear;
    };

    struct SavedImageLayerRuntime {
        std::string name;
        bool visible = true;
        float opacity = 1.0f;
    };

    struct SavedEffectRegionRuntime {
        std::string id;
        bool visible = true;
        float opacity = 1.0f;
    };

    struct SavedTriggerRuntime {
        std::string id;
        bool enabled = true;
        bool repeat = false;
        bool fired = false;
        bool playerInside = false;
        std::vector<TopdownCharacterHandle> npcHandlesInside;
    };

    struct SavedEmitterRuntime {
        std::string id;
        bool enabled = true;
        float volume = 1.0f;
    };

    struct SavedBloodDecalRuntime {
        bool active = false;
        Vector2 position{};
        float radius = 20.0f;
        float rotationRadians = 0.0f;
        float opacity = 1.0f;
        float ageMs = 0.0f;
        float fadeInMs = 0.0f;
        float stretch = 1.0f;
        bool useGeneratedStamp = false;
        bool preferStreakStamp = false;
        int stampIndex = -1;
    };

    struct SavedTopdownWorldRuntime {
        std::vector<SavedDoorRuntime> doors;
        std::vector<SavedWindowRuntime> windows;
        std::vector<SavedPropRuntime> props;
        std::vector<SavedImageLayerRuntime> imageLayers;
        std::vector<SavedEffectRegionRuntime> effectRegions;
        std::vector<SavedTriggerRuntime> triggers;
        std::vector<SavedEmitterRuntime> emitters;
        std::vector<SavedBloodDecalRuntime> bloodDecals;
    };

    struct SaveRestoreData {
        bool controlsEnabled = true;

        std::string levelId;
        std::string saveName;
        std::string savedAt;

        std::unordered_map<std::string, bool> flags;
        std::unordered_map<std::string, int> ints;
        std::unordered_map<std::string, std::string> strings;

        SavedAudioState audio;
        SavedTopdownCoreRuntime topdownCore;
        SavedTopdownNpcsRuntime topdownNpcs;
        SavedTopdownWorldRuntime topdownWorld;
    };

    static std::string NormalizePath(const fs::path& p)
    {
        return p.lexically_normal().string();
    }

    static fs::path GetSaveDirPath()
    {
        return fs::path("saves");
    }

    static fs::path GetSaveSlotPath(int slotIndex)
    {
        return GetSaveDirPath() / ("slot" + std::to_string(slotIndex) + ".json");
    }

    static std::string BuildCurrentSaveTimestamp()
    {
        const std::time_t now = std::time(nullptr);
        std::tm localTm{};

#if defined(_WIN32)
        localtime_s(&localTm, &now);
#else
        localTm = *std::localtime(&now);
#endif

        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &localTm);
        return std::string(buf);
    }

    static std::string FormatSaveSummary(const std::string& saveName, const std::string& savedAt)
    {
        if (saveName.empty() && savedAt.empty()) {
            return "Corrupt";
        }

        if (saveName.empty()) {
            return savedAt;
        }

        if (savedAt.empty()) {
            return saveName;
        }

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;

        if (std::sscanf(savedAt.c_str(), "%d-%d-%d %d:%d", &year, &month, &day, &hour, &minute) == 5) {
            char buf[64];
            std::snprintf(buf, sizeof(buf),
                          "%s %02d:%02d (%04d-%02d-%02d)",
                          saveName.c_str(),
                          hour, minute,
                          year, month, day);
            return std::string(buf);
        }

        return saveName + " " + savedAt;
    }

    static bool EnsureSaveDirExists()
    {
        std::error_code ec;
        fs::create_directories(GetSaveDirPath(), ec);
        return !ec;
    }


    static json SerializeVector2(Vector2 v)
    {
        json j;
        j["x"] = v.x;
        j["y"] = v.y;
        return j;
    }

    static Vector2 DeserializeVector2(const json& j)
    {
        Vector2 v{};
        v.x = j.value("x", 0.0f);
        v.y = j.value("y", 0.0f);
        return v;
    }

    static json SerializeColor(Color c)
    {
        json j;
        j["r"] = c.r;
        j["g"] = c.g;
        j["b"] = c.b;
        j["a"] = c.a;
        return j;
    }

    static Color DeserializeColor(const json& j)
    {
        Color c = WHITE;
        c.r = static_cast<unsigned char>(j.value("r", 255));
        c.g = static_cast<unsigned char>(j.value("g", 255));
        c.b = static_cast<unsigned char>(j.value("b", 255));
        c.a = static_cast<unsigned char>(j.value("a", 255));
        return c;
    }


    static int ToInt(TopdownPlayerLifeState v) { return static_cast<int>(v); }
    static int ToInt(TopdownLocomotionType v) { return static_cast<int>(v); }
    static int ToInt(TopdownFireMode v) { return static_cast<int>(v); }
    static int ToInt(TopdownCameraMode v) { return static_cast<int>(v); }
    static int ToInt(TopdownNpcAiMode v) { return static_cast<int>(v); }
    static int ToInt(TopdownNpcEngagementState v) { return static_cast<int>(v); }
    static int ToInt(MoveInterpolation v) { return static_cast<int>(v); }


    static TopdownPlayerLifeState ToPlayerLifeState(int v)
    {
        switch (static_cast<TopdownPlayerLifeState>(v)) {
            case TopdownPlayerLifeState::Alive:
            case TopdownPlayerLifeState::Dying:
            case TopdownPlayerLifeState::Dead:
            case TopdownPlayerLifeState::GameOver:
                return static_cast<TopdownPlayerLifeState>(v);
        }
        return TopdownPlayerLifeState::Alive;
    }

    static TopdownLocomotionType ToLocomotionType(int v)
    {
        switch (static_cast<TopdownLocomotionType>(v)) {
            case TopdownLocomotionType::Idle:
            case TopdownLocomotionType::Forward:
            case TopdownLocomotionType::Backward:
            case TopdownLocomotionType::StrafeLeft:
            case TopdownLocomotionType::StrafeRight:
                return static_cast<TopdownLocomotionType>(v);
        }
        return TopdownLocomotionType::Idle;
    }

    static TopdownFireMode ToFireMode(int v)
    {
        switch (static_cast<TopdownFireMode>(v)) {
            case TopdownFireMode::SemiAuto:
            case TopdownFireMode::FullAuto:
            case TopdownFireMode::Burst:
                return static_cast<TopdownFireMode>(v);
        }
        return TopdownFireMode::SemiAuto;
    }

    static TopdownCameraMode ToCameraMode(int v)
    {
        switch (static_cast<TopdownCameraMode>(v)) {
            case TopdownCameraMode::Player:
            case TopdownCameraMode::Scripted:
            case TopdownCameraMode::Manual:
                return static_cast<TopdownCameraMode>(v);
        }
        return TopdownCameraMode::Player;
    }

    static TopdownNpcAiMode ToNpcAiMode(int v)
    {
        switch (static_cast<TopdownNpcAiMode>(v)) {
            case TopdownNpcAiMode::None:
            case TopdownNpcAiMode::SeekAndDestroy:
            case TopdownNpcAiMode::HoldAndFire:
                return static_cast<TopdownNpcAiMode>(v);
        }
        return TopdownNpcAiMode::None;
    }

    static TopdownNpcEngagementState ToNpcEngagementState(int v)
    {
        switch (static_cast<TopdownNpcEngagementState>(v)) {
            case TopdownNpcEngagementState::Unaware:
            case TopdownNpcEngagementState::Guarding:
            case TopdownNpcEngagementState::Reacting:
            case TopdownNpcEngagementState::Investigating:
            case TopdownNpcEngagementState::Engaged:
            case TopdownNpcEngagementState::ReturningToGuardPost:
                return static_cast<TopdownNpcEngagementState>(v);
        }
        return TopdownNpcEngagementState::Unaware;
    }


    static MoveInterpolation ToMoveInterpolation(int v)
    {
        switch (static_cast<MoveInterpolation>(v)) {
            case MoveInterpolation::Linear:
            case MoveInterpolation::Accelerate:
            case MoveInterpolation::Decelerate:
            case MoveInterpolation::AccelerateDecelerate:
            case MoveInterpolation::Overshoot:
                return static_cast<MoveInterpolation>(v);
        }
        return MoveInterpolation::Linear;
    }

    static const SpriteClip* FindSpriteClipByNameInSave(
            const SpriteAssetResource& sprite,
            const std::string& clipName)
    {
        for (const SpriteClip& clip : sprite.clips) {
            if (clip.name == clipName) {
                return &clip;
            }
        }
        return nullptr;
    }

    static bool PropHasAnimationClip(const GameState& state, const TopdownRuntimeProp& prop, const std::string& animation)
    {
        if (animation.empty()) {
            return false;
        }

        if (prop.type != TopdownPropType::Sprite || prop.spriteHandle < 0) {
            return false;
        }

        const SpriteAssetResource* sprite = FindSpriteAssetResource(state.resources, prop.spriteHandle);
        return sprite != nullptr && sprite->loaded &&
               FindSpriteClipByNameInSave(*sprite, animation) != nullptr;
    }

    static void SerializeTopdownCoreRuntime(const GameState& state, json& outRoot)
    {
        const TopdownRuntimeData& runtime = state.topdown.runtime;
        const TopdownPlayerRuntime& player = runtime.player;
        const TopdownCharacterRuntime& character = runtime.playerCharacter;
        const TopdownPlayerAttackRuntime& attack = runtime.playerAttack;
        const TopdownCameraRuntime& camera = runtime.camera;

        json topdownCore;
        topdownCore["controlsEnabled"] = runtime.controlsEnabled;
        topdownCore["timeMs"] = runtime.timeMs;

        json playerJson;
        playerJson["position"] = SerializeVector2(player.position);
        playerJson["velocity"] = SerializeVector2(player.velocity);
        playerJson["facing"] = SerializeVector2(player.facing);
        playerJson["health"] = player.health;
        playerJson["maxHealth"] = player.maxHealth;
        playerJson["hurtCooldownRemainingMs"] = player.hurtCooldownRemainingMs;
        playerJson["hitSlowdownRemainingMs"] = player.hitSlowdownRemainingMs;
        playerJson["hitSlowdownMultiplier"] = player.hitSlowdownMultiplier;
        playerJson["damageFlashRemainingMs"] = player.damageFlashRemainingMs;
        playerJson["lowHealthEffectWeight"] = player.lowHealthEffectWeight;
        playerJson["lifeState"] = ToInt(player.lifeState);
        topdownCore["player"] = playerJson;

        json characterJson;
        characterJson["active"] = character.active;
        characterJson["equippedSetId"] = character.equippedSetId;
        characterJson["locomotion"] = ToInt(character.locomotion);
        characterJson["running"] = character.running;
        characterJson["bodyFacingRadians"] = character.bodyFacingRadians;
        characterJson["desiredAimRadians"] = character.desiredAimRadians;
        characterJson["feetRotationRadians"] = character.feetRotationRadians;
        characterJson["upperRotationRadians"] = character.upperRotationRadians;
        characterJson["aimFrozen"] = character.aimFrozen;
        characterJson["feetAnimationTimeMs"] = character.feetAnimationTimeMs;
        characterJson["upperAnimationTimeMs"] = character.upperAnimationTimeMs;
        topdownCore["playerCharacter"] = characterJson;

        json attackJson;
        attackJson["equipmentSetId"] = attack.equipmentSetId;
        attackJson["currentFireMode"] = ToInt(attack.currentFireMode);
        attackJson["cooldownRemainingMs"] = attack.cooldownRemainingMs;
        topdownCore["playerAttack"] = attackJson;

        json cameraJson;
        cameraJson["position"] = SerializeVector2(camera.position);
        cameraJson["targetPosition"] = SerializeVector2(camera.targetPosition);
        cameraJson["aimOffset"] = SerializeVector2(camera.aimOffset);
        cameraJson["mode"] = ToInt(camera.mode);
        cameraJson["scriptedTarget"] = SerializeVector2(camera.scriptedTarget);
        topdownCore["camera"] = cameraJson;

        outRoot["topdownCore"] = topdownCore;
    }

    static SavedTopdownCoreRuntime DeserializeTopdownCoreRuntime(const json& root)
    {
        SavedTopdownCoreRuntime out;
        if (!root.contains("topdownCore") || !root["topdownCore"].is_object()) {
            return out;
        }

        const json& topdownCore = root["topdownCore"];
        out.controlsEnabled = topdownCore.value("controlsEnabled", true);
        out.timeMs = topdownCore.value("timeMs", 0.0f);

        if (topdownCore.contains("player") && topdownCore["player"].is_object()) {
            const json& player = topdownCore["player"];
            if (player.contains("position") && player["position"].is_object()) {
                out.player.position = DeserializeVector2(player["position"]);
            }
            if (player.contains("velocity") && player["velocity"].is_object()) {
                out.player.velocity = DeserializeVector2(player["velocity"]);
            }
            if (player.contains("facing") && player["facing"].is_object()) {
                out.player.facing = DeserializeVector2(player["facing"]);
            }
            out.player.health = player.value("health", 100.0f);
            out.player.maxHealth = player.value("maxHealth", 100.0f);
            out.player.hurtCooldownRemainingMs = player.value("hurtCooldownRemainingMs", 0.0f);
            out.player.hitSlowdownRemainingMs = player.value("hitSlowdownRemainingMs", 0.0f);
            out.player.hitSlowdownMultiplier = player.value("hitSlowdownMultiplier", 1.0f);
            out.player.damageFlashRemainingMs = player.value("damageFlashRemainingMs", 0.0f);
            out.player.lowHealthEffectWeight = player.value("lowHealthEffectWeight", 0.0f);
            out.player.lifeState = ToPlayerLifeState(player.value("lifeState", 0));
        }

        if (topdownCore.contains("playerCharacter") && topdownCore["playerCharacter"].is_object()) {
            const json& character = topdownCore["playerCharacter"];
            out.playerCharacter.active = character.value("active", false);
            out.playerCharacter.equippedSetId = character.value("equippedSetId", "");
            out.playerCharacter.locomotion = ToLocomotionType(character.value("locomotion", 0));
            out.playerCharacter.running = character.value("running", false);
            out.playerCharacter.bodyFacingRadians = character.value("bodyFacingRadians", 0.0f);
            out.playerCharacter.desiredAimRadians = character.value("desiredAimRadians", 0.0f);
            out.playerCharacter.feetRotationRadians = character.value("feetRotationRadians", 0.0f);
            out.playerCharacter.upperRotationRadians = character.value("upperRotationRadians", 0.0f);
            out.playerCharacter.aimFrozen = character.value("aimFrozen", false);
            out.playerCharacter.feetAnimationTimeMs = character.value("feetAnimationTimeMs", 0.0f);
            out.playerCharacter.upperAnimationTimeMs = character.value("upperAnimationTimeMs", 0.0f);
        }

        if (topdownCore.contains("playerAttack") && topdownCore["playerAttack"].is_object()) {
            const json& attack = topdownCore["playerAttack"];
            out.playerAttack.equipmentSetId = attack.value("equipmentSetId", "");
            out.playerAttack.currentFireMode = ToFireMode(attack.value("currentFireMode", 0));
            out.playerAttack.cooldownRemainingMs = attack.value("cooldownRemainingMs", 0.0f);
        }

        if (topdownCore.contains("camera") && topdownCore["camera"].is_object()) {
            const json& camera = topdownCore["camera"];
            if (camera.contains("position") && camera["position"].is_object()) {
                out.camera.position = DeserializeVector2(camera["position"]);
            }
            if (camera.contains("targetPosition") && camera["targetPosition"].is_object()) {
                out.camera.targetPosition = DeserializeVector2(camera["targetPosition"]);
            }
            if (camera.contains("aimOffset") && camera["aimOffset"].is_object()) {
                out.camera.aimOffset = DeserializeVector2(camera["aimOffset"]);
            }
            out.camera.mode = ToCameraMode(camera.value("mode", 0));
            if (camera.contains("scriptedTarget") && camera["scriptedTarget"].is_object()) {
                out.camera.scriptedTarget = DeserializeVector2(camera["scriptedTarget"]);
            }
        }

        return out;
    }

    static void RestoreTopdownCoreRuntime(GameState& state, const SavedTopdownCoreRuntime& saved)
    {
        TopdownRuntimeData& runtime = state.topdown.runtime;

        runtime.controlsEnabled = saved.controlsEnabled;
        runtime.timeMs = saved.timeMs;

        runtime.player.position = saved.player.position;
        runtime.player.velocity = saved.player.velocity;
        runtime.player.desiredVelocity = {};
        runtime.player.facing = saved.player.facing;
        runtime.player.health = saved.player.health;
        runtime.player.maxHealth = saved.player.maxHealth;
        runtime.player.hurtCooldownRemainingMs = saved.player.hurtCooldownRemainingMs;
        runtime.player.hitSlowdownRemainingMs = saved.player.hitSlowdownRemainingMs;
        runtime.player.hitSlowdownMultiplier = saved.player.hitSlowdownMultiplier;
        runtime.player.damageFlashRemainingMs = saved.player.damageFlashRemainingMs;
        runtime.player.lowHealthEffectWeight = saved.player.lowHealthEffectWeight;
        runtime.player.lifeState = saved.player.lifeState;
        runtime.player.moveInputForward = 0.0f;
        runtime.player.moveInputRight = 0.0f;
        runtime.player.wantsRun = false;

        runtime.playerCharacter.active = saved.playerCharacter.active;
        runtime.playerCharacter.equippedSetId = saved.playerCharacter.equippedSetId.empty()
                ? runtime.playerCharacter.equippedSetId
                : saved.playerCharacter.equippedSetId;
        runtime.playerCharacter.locomotion = saved.playerCharacter.locomotion;
        runtime.playerCharacter.running = saved.playerCharacter.running;
        runtime.playerCharacter.bodyFacingRadians = saved.playerCharacter.bodyFacingRadians;
        runtime.playerCharacter.desiredAimRadians = saved.playerCharacter.desiredAimRadians;
        runtime.playerCharacter.feetRotationRadians = saved.playerCharacter.feetRotationRadians;
        runtime.playerCharacter.upperRotationRadians = saved.playerCharacter.upperRotationRadians;
        runtime.playerCharacter.aimFrozen = saved.playerCharacter.aimFrozen;
        runtime.playerCharacter.feetAnimationTimeMs = saved.playerCharacter.feetAnimationTimeMs;
        runtime.playerCharacter.upperAnimationTimeMs = saved.playerCharacter.upperAnimationTimeMs;
        runtime.playerCharacter.currentFeetHandle =
                FindTopdownPlayerFeetAnimationHandle(state, "idle");
        runtime.playerCharacter.currentUpperHandle =
                FindTopdownPlayerEquipmentAnimationHandle(
                        state,
                        runtime.playerCharacter.equippedSetId,
                        "idle");

        runtime.playerAttack = {};
        runtime.playerAttack.equipmentSetId = saved.playerAttack.equipmentSetId.empty()
                ? runtime.playerCharacter.equippedSetId
                : saved.playerAttack.equipmentSetId;
        runtime.playerAttack.currentFireMode = saved.playerAttack.currentFireMode;
        runtime.playerAttack.cooldownRemainingMs = saved.playerAttack.cooldownRemainingMs;
        runtime.playerAttack.state = TopdownPlayerAttackState::Idle;
        runtime.playerAttack.active = false;
        runtime.playerAttack.triggerHeld = false;
        runtime.playerAttack.wantsTriggerRelease = false;
        runtime.playerAttack.pendingPrimaryAttack = false;
        runtime.playerAttack.pendingSecondaryAttack = false;
        runtime.playerAttack.rifleLoopPlaying = false;

        if (FindTopdownPlayerWeaponConfigByEquipmentSetId(state, runtime.playerAttack.equipmentSetId) == nullptr) {
            runtime.playerAttack.equipmentSetId = runtime.playerCharacter.equippedSetId;
            runtime.playerAttack.currentFireMode = TopdownFireMode::SemiAuto;
        }

        runtime.camera.position = saved.camera.position;
        runtime.camera.targetPosition = saved.camera.targetPosition;
        runtime.camera.aimOffset = saved.camera.aimOffset;
        runtime.camera.mode = saved.camera.mode;
        runtime.camera.scriptedTarget = saved.camera.scriptedTarget;
        runtime.camera.isPanning = false;
        runtime.camera.panTimerMs = 0.0f;
        runtime.camera.panDurationMs = 0.0f;
        runtime.camera.panStart = {};
        runtime.camera.panEnd = {};
        if (runtime.controlsEnabled && runtime.camera.mode == TopdownCameraMode::Scripted) {
            runtime.camera.mode = TopdownCameraMode::Player;
        }

        runtime.scriptedMove = {};
        runtime.screenShake = {};
        runtime.worldEvents.clear();
        runtime.speechBubbles.entries.clear();
        runtime.narrationPopups = {};
        runtime.returnToMenuRequested = false;
        runtime.gameOverActive = false;
        runtime.gameOverElapsedMs = 0.0f;
    }

    static void SerializeTopdownNpcsRuntime(const GameState& state, json& outRoot)
    {
        json topdownNpcs;
        topdownNpcs["npcs"] = json::array();

        for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
            json npcJson;
            npcJson["id"] = npc.id;
            npcJson["assetId"] = npc.assetId;
            npcJson["handle"] = npc.handle;
            npcJson["active"] = npc.active;
            npcJson["visible"] = npc.visible;
            npcJson["dead"] = npc.dead;
            npcJson["corpse"] = npc.corpse;
            npcJson["position"] = SerializeVector2(npc.position);
            npcJson["facing"] = SerializeVector2(npc.facing);
            npcJson["rotationRadians"] = npc.rotationRadians;
            npcJson["currentVelocity"] = SerializeVector2(npc.currentVelocity);
            npcJson["health"] = npc.health;
            npcJson["corpseElapsedMs"] = npc.corpseElapsedMs;
            npcJson["hostile"] = npc.hostile;
            npcJson["persistentChase"] = npc.persistentChase;
            npcJson["guard"] = npc.guard;
            npcJson["guardHomePosition"] = SerializeVector2(npc.guardHomePosition);
            npcJson["hasGuardHomePosition"] = npc.hasGuardHomePosition;
            npcJson["guardLookAtSoundRadians"] = npc.guardLookAtSoundRadians;
            npcJson["engagementState"] = ToInt(npc.engagementState);
            npcJson["aiMode"] = ToInt(npc.aiMode);

            const TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
            json patrolJson;
            patrolJson["active"] =
                    npc.scriptBehavior.mode == TopdownNpcScriptBehaviorMode::PatrolRoute &&
                    patrol.active;
            patrolJson["paused"] = patrol.paused;
            patrolJson["loop"] = patrol.loop;
            patrolJson["running"] = patrol.running;
            patrolJson["waitMs"] = patrol.waitDurationMs;
            patrolJson["waitRemainingMs"] = patrol.waitTimerMs;
            patrolJson["currentPoint"] = patrol.currentPointIndex;
            patrolJson["routeSpawnIds"] = json::array();
            for (const std::string& spawnId : patrol.spawnIds) {
                patrolJson["routeSpawnIds"].push_back(spawnId);
            }
            npcJson["patrol"] = patrolJson;

            topdownNpcs["npcs"].push_back(npcJson);
        }

        outRoot["topdownNpcs"] = topdownNpcs;
    }

    static SavedNpcPatrolRuntime DeserializeNpcPatrolRuntime(const json& j)
    {
        SavedNpcPatrolRuntime out;
        out.active = j.value("active", false);
        out.paused = j.value("paused", false);
        out.loop = j.value("loop", true);
        out.running = j.value("running", false);
        out.waitMs = j.value("waitMs", 0.0f);
        out.waitRemainingMs = j.value("waitRemainingMs", 0.0f);
        out.currentPoint = j.value("currentPoint", 0);

        if (j.contains("routeSpawnIds") && j["routeSpawnIds"].is_array()) {
            for (const json& spawnId : j["routeSpawnIds"]) {
                if (spawnId.is_string()) {
                    out.routeSpawnIds.push_back(spawnId.get<std::string>());
                }
            }
        }

        return out;
    }

    static SavedTopdownNpcsRuntime DeserializeTopdownNpcsRuntime(const json& root)
    {
        SavedTopdownNpcsRuntime out;
        if (!root.contains("topdownNpcs") || !root["topdownNpcs"].is_object()) {
            return out;
        }

        const json& topdownNpcs = root["topdownNpcs"];
        if (!topdownNpcs.contains("npcs") || !topdownNpcs["npcs"].is_array()) {
            return out;
        }

        for (const json& npcJson : topdownNpcs["npcs"]) {
            if (!npcJson.is_object()) {
                continue;
            }

            SavedNpcRuntime npc;
            npc.id = npcJson.value("id", "");
            npc.assetId = npcJson.value("assetId", "");
            npc.handle = npcJson.value("handle", -1);
            npc.active = npcJson.value("active", true);
            npc.visible = npcJson.value("visible", true);
            npc.dead = npcJson.value("dead", false);
            npc.corpse = npcJson.value("corpse", false);
            if (npcJson.contains("position") && npcJson["position"].is_object()) {
                npc.position = DeserializeVector2(npcJson["position"]);
            }
            if (npcJson.contains("facing") && npcJson["facing"].is_object()) {
                npc.facing = DeserializeVector2(npcJson["facing"]);
            }
            if (npcJson.contains("currentVelocity") && npcJson["currentVelocity"].is_object()) {
                npc.currentVelocity = DeserializeVector2(npcJson["currentVelocity"]);
            }
            npc.rotationRadians = npcJson.value("rotationRadians", 0.0f);
            npc.health = npcJson.value("health", 100.0f);
            npc.corpseElapsedMs = npcJson.value("corpseElapsedMs", 0.0f);
            npc.hostile = npcJson.value("hostile", true);
            npc.persistentChase = npcJson.value("persistentChase", false);
            npc.guard = npcJson.value("guard", false);
            npc.hasGuardHomePosition = npcJson.value("hasGuardHomePosition", npc.guard);
            if (npcJson.contains("guardHomePosition") && npcJson["guardHomePosition"].is_object()) {
                npc.guardHomePosition = DeserializeVector2(npcJson["guardHomePosition"]);
            } else {
                npc.guardHomePosition = npc.position;
            }
            npc.guardLookAtSoundRadians = npcJson.value("guardLookAtSoundRadians", npc.rotationRadians);
            npc.engagementState = ToNpcEngagementState(npcJson.value("engagementState", 0));
            npc.aiMode = ToNpcAiMode(npcJson.value("aiMode", 0));

            if (npcJson.contains("patrol") && npcJson["patrol"].is_object()) {
                npc.patrol = DeserializeNpcPatrolRuntime(npcJson["patrol"]);
            }

            out.npcs.push_back(npc);
        }

        return out;
    }

    static void ClearNpcTransientRuntime(TopdownNpcRuntime& npc)
    {
        npc.combatState = TopdownNpcCombatState::None;
        npc.hasPlayerTarget = false;
        npc.lastKnownPlayerPosition = {};
        npc.investigationPosition = {};
        npc.repathTimerMs = 0.0f;

        npc.attackHitPending = false;
        npc.attackHitApplied = false;
        npc.attackStateTimeMs = 0.0f;
        npc.attackAnimationDurationMs = 0.0f;
        npc.attackCooldownRemainingMs = 0.0f;

        npc.searchStateTimeMs = 0.0f;
        npc.hurtStunRemainingMs = 0.0f;
        npc.knockbackVelocity = {};

        npc.move = {};
        npc.moving = false;
        npc.running = false;

        npc.oneShotActive = false;
        npc.oneShotClip = {};
        npc.oneShotTimeMs = 0.0f;

        npc.scriptLoopClip = {};
        npc.scriptLoopTimeMs = 0.0f;
        npc.animationMode = TopdownNpcAnimationMode::AutomaticLocomotion;

        npc.renderOpacity = 1.0f;

        npc.chaseStuckTimerMs = 0.0f;
        npc.chaseStuckLastPosition = npc.position;
        npc.patrolLastProgressPosition = npc.position;
        npc.patrolStuckTimerMs = 0.0f;
        npc.patrolYieldTimerMs = 0.0f;
        npc.patrolRetryDelayMs = 0.0f;
        npc.patrolStuckCount = 0;
        npc.patrolIsYielding = false;
        npc.patrolIsRetryDelay = false;
        npc.investigationContextHandle = -1;
        npc.investigationSlotIndex = -1;
        npc.investigationProgressTimerMs = 0.0f;
        npc.investigationLastPosition = npc.position;
        npc.reactionTimerMs = 0.0f;
        npc.hasReactedToPlayer = false;
        npc.engagedLostTargetTimerMs = 0.0f;
    }

    static void RestoreNpcPatrolRuntime(
            GameState& state,
            TopdownNpcRuntime& npc,
            const SavedNpcPatrolRuntime& saved)
    {
        if (!saved.active || saved.routeSpawnIds.empty()) {
            npc.scriptBehavior = {};
            return;
        }

        TopdownNpcPatrolRouteOptions options;
        options.loop = saved.loop;
        options.running = saved.running;
        options.waitMs = saved.waitMs;

        if (!TopdownAssignNpcPatrolRoute(state, npc, saved.routeSpawnIds, options)) {
            TraceLog(LOG_WARNING,
                     "Failed restoring NPC patrol route for '%s'",
                     npc.id.c_str());
            npc.scriptBehavior = {};
            return;
        }

        TopdownNpcPatrolState& patrol = npc.scriptBehavior.patrol;
        const int routeSize = static_cast<int>(patrol.spawnIds.size());
        patrol.currentPointIndex = routeSize > 0
                ? std::max(0, std::min(saved.currentPoint, routeSize - 1))
                : 0;
        patrol.waitTimerMs = std::max(0.0f, saved.waitRemainingMs);
        patrol.paused = saved.paused;
        patrol.running = saved.running;
        patrol.loop = saved.loop;
        patrol.interrupted = false;
    }

    static void RestoreNpcDeathAnimation(GameState& state, TopdownNpcRuntime& npc)
    {
        const TopdownNpcAssetRuntime* asset = FindTopdownNpcAssetRuntime(state, npc.assetId);
        if (asset == nullptr) {
            return;
        }

        if (TopdownNpcClipRefIsValid(asset->deathClip)) {
            TopdownSetNpcAutomaticLoopAnimation(npc, asset->deathClip);
        } else if (TopdownNpcClipRefIsValid(asset->idleClip)) {
            TopdownSetNpcAutomaticLoopAnimation(npc, asset->idleClip);
        }
    }

    static void RestoreTopdownNpcsRuntime(GameState& state, const SavedTopdownNpcsRuntime& saved)
    {
        TopdownRuntimeData& runtime = state.topdown.runtime;
        runtime.npcs.clear();
        runtime.npcPatrolContexts.clear();
        runtime.nextNpcPatrolContextHandle = 1;
        runtime.npcInvestigations.clear();
        runtime.nextNpcInvestigationContextHandle = 1;
        runtime.nextNpcHandle = 1;

        for (const SavedNpcRuntime& savedNpc : saved.npcs) {
            if (savedNpc.id.empty() || savedNpc.assetId.empty()) {
                continue;
            }

            if (!EnsureTopdownNpcAssetLoaded(state, savedNpc.assetId)) {
                TraceLog(LOG_WARNING,
                         "Skipping restored NPC '%s', asset failed to load: %s",
                         savedNpc.id.c_str(),
                         savedNpc.assetId.c_str());
                continue;
            }

            if (!TopdownSpawnNpcRuntime(
                    state,
                    savedNpc.id,
                    savedNpc.assetId,
                    savedNpc.position,
                    savedNpc.rotationRadians * RAD2DEG,
                    savedNpc.visible,
                    savedNpc.persistentChase,
                    savedNpc.guard,
                    false)) {
                TraceLog(LOG_WARNING,
                         "Skipping restored NPC '%s', spawn failed",
                         savedNpc.id.c_str());
                continue;
            }

            TopdownNpcRuntime* npc = runtime.npcs.empty() ? nullptr : &runtime.npcs.back();
            if (npc == nullptr || npc->id != savedNpc.id) {
                continue;
            }

            npc->handle = savedNpc.handle > 0 ? savedNpc.handle : npc->handle;
            npc->active = savedNpc.active;
            npc->visible = savedNpc.visible;
            npc->dead = savedNpc.dead;
            npc->corpse = savedNpc.corpse;
            npc->position = savedNpc.position;
            npc->facing = savedNpc.facing;
            npc->rotationRadians = savedNpc.rotationRadians;
            npc->currentVelocity = savedNpc.currentVelocity;
            npc->health = savedNpc.health;
            npc->corpseElapsedMs = savedNpc.corpseElapsedMs;
            npc->persistentChase = savedNpc.persistentChase;
            npc->guard = savedNpc.guard;
            npc->hostile = savedNpc.hostile;
            npc->aiMode = savedNpc.aiMode;
            npc->engagementState = savedNpc.engagementState;
            npc->guardHomePosition = savedNpc.guardHomePosition;
            npc->hasGuardHomePosition = savedNpc.hasGuardHomePosition;
            npc->guardLookAtSoundRadians = savedNpc.guardLookAtSoundRadians;

            ClearNpcTransientRuntime(*npc);

            if (npc->dead || npc->corpse) {
                npc->combatState = TopdownNpcCombatState::None;
                npc->hasPlayerTarget = false;
                npc->move = {};
                npc->moving = false;
                npc->running = false;
                RestoreNpcDeathAnimation(state, *npc);
            } else {
                RestoreNpcPatrolRuntime(state, *npc, savedNpc.patrol);
            }
        }

        int maxHandle = 0;
        for (const TopdownNpcRuntime& npc : runtime.npcs) {
            maxHandle = std::max(maxHandle, npc.handle);
        }
        runtime.nextNpcHandle = maxHandle + 1;

        TopdownRvoRequestRebuild(state);
        TopdownRvoEnsureReady(state);
    }


    static void SerializeTopdownWorldRuntime(const GameState& state, json& outRoot)
    {
        json world;

        world["doors"] = json::array();
        for (const TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
            json j;
            j["id"] = door.id;
            j["visible"] = door.visible;
            j["locked"] = door.locked;
            j["angleRadians"] = door.angleRadians;
            j["angularVelocity"] = door.angularVelocity;
            j["wasNearClosed"] = door.wasNearClosed;
            j["openSoundPlayedThisSwing"] = door.openSoundPlayedThisSwing;
            world["doors"].push_back(j);
        }

        world["windows"] = json::array();
        for (const TopdownRuntimeWindow& window : state.topdown.runtime.windows) {
            json j;
            j["id"] = window.id;
            j["visible"] = window.visible;
            j["broken"] = window.broken;
            world["windows"].push_back(j);
        }

        world["props"] = json::array();
        for (const TopdownRuntimeProp& prop : state.topdown.runtime.props) {
            json j;
            j["id"] = prop.id;
            j["active"] = prop.active;
            j["visible"] = prop.visible;
            j["position"] = SerializeVector2(prop.position);
            j["opacity"] = prop.opacity;
            j["currentAnimation"] = prop.currentAnimation;
            j["baseAnimation"] = prop.baseAnimation;
            j["animationTimeMs"] = prop.animationTimeMs;
            j["oneShotActive"] = prop.oneShotActive;
            j["oneShotAnimation"] = prop.oneShotAnimation;
            j["oneShotDurationMs"] = prop.oneShotDurationMs;
            j["moving"] = prop.moving;
            j["moveStart"] = SerializeVector2(prop.moveStart);
            j["moveEnd"] = SerializeVector2(prop.moveEnd);
            j["moveTimerMs"] = prop.moveTimerMs;
            j["moveDurationMs"] = prop.moveDurationMs;
            j["moveInterpolation"] = ToInt(prop.moveInterpolation);
            world["props"].push_back(j);
        }

        world["imageLayers"] = json::array();
        auto writeLayer = [&](const TopdownRuntimeImageLayer& layer) {
            if (layer.authoredIndex < 0 ||
                layer.authoredIndex >= static_cast<int>(state.topdown.authored.imageLayers.size())) {
                return;
            }

            json j;
            j["name"] = state.topdown.authored.imageLayers[layer.authoredIndex].name;
            j["visible"] = layer.visible;
            j["opacity"] = layer.opacity;
            world["imageLayers"].push_back(j);
        };
        for (const TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.bottomLayers) {
            writeLayer(layer);
        }
        for (const TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.topLayers) {
            writeLayer(layer);
        }

        world["effectRegions"] = json::array();
        for (const TopdownRuntimeEffectRegion& effect : state.topdown.runtime.render.effectRegions) {
            if (effect.authoredIndex < 0 ||
                effect.authoredIndex >= static_cast<int>(state.topdown.authored.effectRegions.size())) {
                continue;
            }

            json j;
            j["id"] = state.topdown.authored.effectRegions[effect.authoredIndex].id;
            j["visible"] = effect.visible;
            j["opacity"] = effect.opacity;
            world["effectRegions"].push_back(j);
        }

        world["triggers"] = json::array();
        for (const TopdownRuntimeTrigger& trigger : state.topdown.runtime.triggers) {
            if (trigger.authoredIndex < 0 ||
                trigger.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
                continue;
            }

            json j;
            j["id"] = state.topdown.authored.triggers[trigger.authoredIndex].id;
            j["enabled"] = trigger.enabled;
            j["repeat"] = trigger.repeat;
            j["fired"] = trigger.fired;
            j["playerInside"] = trigger.playerInside;
            j["npcHandlesInside"] = json::array();
            for (TopdownCharacterHandle handle : trigger.npcHandlesInside) {
                j["npcHandlesInside"].push_back(handle);
            }
            world["triggers"].push_back(j);
        }

        world["emitters"] = json::array();
        for (const SoundEmitterInstance& emitter : state.audio.levelEmitters) {
            json j;
            j["id"] = emitter.id;
            j["enabled"] = emitter.enabled;
            j["volume"] = emitter.volume;
            world["emitters"].push_back(j);
        }

        world["bloodDecals"] = json::array();
        for (const TopdownBloodDecal& decal : state.topdown.runtime.render.bloodDecals) {
            if (!decal.active) {
                continue;
            }

            json j;
            j["active"] = decal.active;
            j["position"] = SerializeVector2(decal.position);
            j["radius"] = decal.radius;
            j["rotationRadians"] = decal.rotationRadians;
            j["opacity"] = decal.opacity;
            j["ageMs"] = decal.ageMs;
            j["fadeInMs"] = decal.fadeInMs;
            j["stretch"] = decal.stretch;
            j["useGeneratedStamp"] = decal.useGeneratedStamp;
            j["preferStreakStamp"] = decal.preferStreakStamp;
            j["stampIndex"] = decal.stampIndex;
            world["bloodDecals"].push_back(j);
        }

        outRoot["topdownWorld"] = world;
    }

    static SavedTopdownWorldRuntime DeserializeTopdownWorldRuntime(const json& root)
    {
        SavedTopdownWorldRuntime out;
        if (!root.contains("topdownWorld") || !root["topdownWorld"].is_object()) {
            return out;
        }

        const json& world = root["topdownWorld"];

        if (world.contains("doors") && world["doors"].is_array()) {
            for (const json& j : world["doors"]) {
                if (!j.is_object()) continue;
                SavedDoorRuntime door;
                door.id = j.value("id", "");
                door.visible = j.value("visible", true);
                door.locked = j.value("locked", false);
                door.angleRadians = j.value("angleRadians", 0.0f);
                door.angularVelocity = j.value("angularVelocity", 0.0f);
                door.wasNearClosed = j.value("wasNearClosed", true);
                door.openSoundPlayedThisSwing = j.value("openSoundPlayedThisSwing", false);
                out.doors.push_back(door);
            }
        }

        if (world.contains("windows") && world["windows"].is_array()) {
            for (const json& j : world["windows"]) {
                if (!j.is_object()) continue;
                SavedWindowRuntime window;
                window.id = j.value("id", "");
                window.visible = j.value("visible", true);
                window.broken = j.value("broken", false);
                out.windows.push_back(window);
            }
        }

        if (world.contains("props") && world["props"].is_array()) {
            for (const json& j : world["props"]) {
                if (!j.is_object()) continue;
                SavedPropRuntime prop;
                prop.id = j.value("id", "");
                prop.active = j.value("active", false);
                prop.visible = j.value("visible", true);
                if (j.contains("position") && j["position"].is_object()) prop.position = DeserializeVector2(j["position"]);
                prop.opacity = j.value("opacity", 1.0f);
                prop.currentAnimation = j.value("currentAnimation", "");
                prop.baseAnimation = j.value("baseAnimation", "");
                prop.animationTimeMs = j.value("animationTimeMs", 0.0f);
                prop.oneShotActive = j.value("oneShotActive", false);
                prop.oneShotAnimation = j.value("oneShotAnimation", "");
                prop.oneShotDurationMs = j.value("oneShotDurationMs", 0.0f);
                prop.moving = j.value("moving", false);
                if (j.contains("moveStart") && j["moveStart"].is_object()) prop.moveStart = DeserializeVector2(j["moveStart"]);
                if (j.contains("moveEnd") && j["moveEnd"].is_object()) prop.moveEnd = DeserializeVector2(j["moveEnd"]);
                prop.moveTimerMs = j.value("moveTimerMs", 0.0f);
                prop.moveDurationMs = j.value("moveDurationMs", 0.0f);
                prop.moveInterpolation = ToMoveInterpolation(j.value("moveInterpolation", 0));
                out.props.push_back(prop);
            }
        }

        if (world.contains("imageLayers") && world["imageLayers"].is_array()) {
            for (const json& j : world["imageLayers"]) {
                if (!j.is_object()) continue;
                SavedImageLayerRuntime layer;
                layer.name = j.value("name", "");
                layer.visible = j.value("visible", true);
                layer.opacity = j.value("opacity", 1.0f);
                out.imageLayers.push_back(layer);
            }
        }

        if (world.contains("effectRegions") && world["effectRegions"].is_array()) {
            for (const json& j : world["effectRegions"]) {
                if (!j.is_object()) continue;
                SavedEffectRegionRuntime effect;
                effect.id = j.value("id", "");
                effect.visible = j.value("visible", true);
                effect.opacity = j.value("opacity", 1.0f);
                out.effectRegions.push_back(effect);
            }
        }

        if (world.contains("triggers") && world["triggers"].is_array()) {
            for (const json& j : world["triggers"]) {
                if (!j.is_object()) continue;
                SavedTriggerRuntime trigger;
                trigger.id = j.value("id", "");
                trigger.enabled = j.value("enabled", true);
                trigger.repeat = j.value("repeat", false);
                trigger.fired = j.value("fired", false);
                trigger.playerInside = j.value("playerInside", false);
                if (j.contains("npcHandlesInside") && j["npcHandlesInside"].is_array()) {
                    for (const json& handle : j["npcHandlesInside"]) {
                        if (handle.is_number_integer()) {
                            trigger.npcHandlesInside.push_back(handle.get<TopdownCharacterHandle>());
                        }
                    }
                }
                out.triggers.push_back(trigger);
            }
        }

        if (world.contains("emitters") && world["emitters"].is_array()) {
            for (const json& j : world["emitters"]) {
                if (!j.is_object()) continue;
                SavedEmitterRuntime emitter;
                emitter.id = j.value("id", "");
                emitter.enabled = j.value("enabled", true);
                emitter.volume = j.value("volume", 1.0f);
                out.emitters.push_back(emitter);
            }
        }

        if (world.contains("bloodDecals") && world["bloodDecals"].is_array()) {
            for (const json& j : world["bloodDecals"]) {
                if (!j.is_object()) continue;
                SavedBloodDecalRuntime decal;
                decal.active = j.value("active", false);
                if (j.contains("position") && j["position"].is_object()) decal.position = DeserializeVector2(j["position"]);
                decal.radius = j.value("radius", 20.0f);
                decal.rotationRadians = j.value("rotationRadians", 0.0f);
                decal.opacity = j.value("opacity", 1.0f);
                decal.ageMs = j.value("ageMs", 0.0f);
                decal.fadeInMs = j.value("fadeInMs", 0.0f);
                decal.stretch = j.value("stretch", 1.0f);
                decal.useGeneratedStamp = j.value("useGeneratedStamp", false);
                decal.preferStreakStamp = j.value("preferStreakStamp", false);
                decal.stampIndex = j.value("stampIndex", -1);
                out.bloodDecals.push_back(decal);
            }
        }

        return out;
    }

    static void RestoreTopdownWorldRuntime(GameState& state, const SavedTopdownWorldRuntime& saved)
    {
        std::unordered_map<std::string, const SavedDoorRuntime*> doorsById;
        for (const SavedDoorRuntime& door : saved.doors) {
            if (!door.id.empty()) doorsById[door.id] = &door;
        }
        for (TopdownRuntimeDoor& door : state.topdown.runtime.doors) {
            auto it = doorsById.find(door.id);
            if (it == doorsById.end()) continue;
            const SavedDoorRuntime& savedDoor = *it->second;
            door.visible = savedDoor.visible;
            door.locked = savedDoor.locked;
            door.angleRadians = savedDoor.angleRadians;
            door.angularVelocity = savedDoor.angularVelocity;
            door.wasNearClosed = savedDoor.wasNearClosed;
            door.openSoundPlayedThisSwing = savedDoor.openSoundPlayedThisSwing;
        }
        TopdownRebuildWallOcclusionPolygons(state.topdown, true);

        std::unordered_map<std::string, const SavedWindowRuntime*> windowsById;
        for (const SavedWindowRuntime& window : saved.windows) {
            if (!window.id.empty()) windowsById[window.id] = &window;
        }
        for (TopdownRuntimeWindow& window : state.topdown.runtime.windows) {
            auto it = windowsById.find(window.id);
            if (it == windowsById.end()) continue;
            window.visible = it->second->visible;
            window.broken = it->second->broken;
        }

        std::unordered_map<std::string, const SavedPropRuntime*> propsById;
        for (const SavedPropRuntime& prop : saved.props) {
            if (!prop.id.empty()) propsById[prop.id] = &prop;
        }
        for (TopdownRuntimeProp& prop : state.topdown.runtime.props) {
            auto it = propsById.find(prop.id);
            if (it == propsById.end()) continue;
            const SavedPropRuntime& savedProp = *it->second;
            prop.active = savedProp.active;
            prop.visible = savedProp.visible;
            prop.position = savedProp.position;
            prop.opacity = savedProp.opacity;
            if (!savedProp.baseAnimation.empty() &&
                PropHasAnimationClip(state, prop, savedProp.baseAnimation)) {
                prop.baseAnimation = savedProp.baseAnimation;
            }
            if (!savedProp.currentAnimation.empty() &&
                PropHasAnimationClip(state, prop, savedProp.currentAnimation)) {
                prop.currentAnimation = savedProp.currentAnimation;
            }
            prop.animationTimeMs = savedProp.animationTimeMs;
            if (savedProp.oneShotActive && PropHasAnimationClip(state, prop, savedProp.oneShotAnimation)) {
                prop.oneShotActive = true;
                prop.oneShotAnimation = savedProp.oneShotAnimation;
                prop.oneShotDurationMs = savedProp.oneShotDurationMs;
            } else {
                prop.oneShotActive = false;
                prop.oneShotAnimation.clear();
                prop.oneShotDurationMs = 0.0f;
            }
            prop.moving = savedProp.moving;
            prop.moveStart = savedProp.moveStart;
            prop.moveEnd = savedProp.moveEnd;
            prop.moveTimerMs = savedProp.moveTimerMs;
            prop.moveDurationMs = savedProp.moveDurationMs;
            prop.moveInterpolation = savedProp.moveInterpolation;
        }

        std::unordered_map<std::string, const SavedImageLayerRuntime*> imageLayersByName;
        for (const SavedImageLayerRuntime& layer : saved.imageLayers) {
            if (!layer.name.empty()) imageLayersByName[layer.name] = &layer;
        }
        auto restoreLayer = [&](TopdownRuntimeImageLayer& layer) {
            if (layer.authoredIndex < 0 ||
                layer.authoredIndex >= static_cast<int>(state.topdown.authored.imageLayers.size())) {
                return;
            }
            const std::string& name = state.topdown.authored.imageLayers[layer.authoredIndex].name;
            auto it = imageLayersByName.find(name);
            if (it == imageLayersByName.end()) return;
            layer.visible = it->second->visible;
            layer.opacity = it->second->opacity;
        };
        for (TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.bottomLayers) {
            restoreLayer(layer);
        }
        for (TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.topLayers) {
            restoreLayer(layer);
        }

        std::unordered_map<std::string, const SavedEffectRegionRuntime*> effectsById;
        for (const SavedEffectRegionRuntime& effect : saved.effectRegions) {
            if (!effect.id.empty()) effectsById[effect.id] = &effect;
        }
        for (TopdownRuntimeEffectRegion& effect : state.topdown.runtime.render.effectRegions) {
            if (effect.authoredIndex < 0 ||
                effect.authoredIndex >= static_cast<int>(state.topdown.authored.effectRegions.size())) {
                continue;
            }
            const std::string& id = state.topdown.authored.effectRegions[effect.authoredIndex].id;
            auto it = effectsById.find(id);
            if (it == effectsById.end()) continue;
            effect.visible = it->second->visible;
            effect.opacity = it->second->opacity;
        }

        std::unordered_map<std::string, const SavedTriggerRuntime*> triggersById;
        for (const SavedTriggerRuntime& trigger : saved.triggers) {
            if (!trigger.id.empty()) triggersById[trigger.id] = &trigger;
        }
        for (TopdownRuntimeTrigger& trigger : state.topdown.runtime.triggers) {
            if (trigger.authoredIndex < 0 ||
                trigger.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
                continue;
            }
            const std::string& id = state.topdown.authored.triggers[trigger.authoredIndex].id;
            auto it = triggersById.find(id);
            if (it == triggersById.end()) continue;
            const SavedTriggerRuntime& savedTrigger = *it->second;
            trigger.pendingCalls.clear();
            trigger.enabled = savedTrigger.enabled;
            trigger.repeat = savedTrigger.repeat;
            trigger.fired = savedTrigger.fired;
            trigger.playerInside = savedTrigger.playerInside;
            trigger.npcHandlesInside = savedTrigger.npcHandlesInside;
        }

        std::unordered_map<std::string, const SavedEmitterRuntime*> emittersById;
        for (const SavedEmitterRuntime& emitter : saved.emitters) {
            if (!emitter.id.empty()) emittersById[emitter.id] = &emitter;
        }
        for (SoundEmitterInstance& emitter : state.audio.levelEmitters) {
            auto it = emittersById.find(emitter.id);
            if (it == emittersById.end()) continue;
            emitter.enabled = it->second->enabled;
            emitter.volume = it->second->volume;
        }

        state.topdown.runtime.render.bloodDecals.clear();
        state.topdown.runtime.render.bloodDecals.reserve(saved.bloodDecals.size());
        for (const SavedBloodDecalRuntime& savedDecal : saved.bloodDecals) {
            if (!savedDecal.active) {
                continue;
            }

            TopdownBloodDecal decal;
            decal.active = savedDecal.active;
            decal.position = savedDecal.position;
            decal.radius = savedDecal.radius;
            decal.targetRadius = savedDecal.radius;
            decal.rotationRadians = savedDecal.rotationRadians;
            decal.opacity = savedDecal.opacity;
            decal.spawnOpacity = savedDecal.opacity;
            decal.ageMs = savedDecal.ageMs;
            decal.fadeInMs = savedDecal.fadeInMs;
            decal.stretch = savedDecal.stretch;
            decal.useGeneratedStamp = savedDecal.useGeneratedStamp;
            decal.preferStreakStamp = savedDecal.preferStreakStamp;
            decal.stampIndex = savedDecal.stampIndex;
            state.topdown.runtime.render.bloodDecals.push_back(decal);
        }
        MarkTopdownBloodRenderTargetDirty(state);
    }

    static void SerializeScriptState(const GameState& state, json& outRoot)
    {
        json scriptState;
        scriptState["flags"] = json::object();
        scriptState["ints"] = json::object();
        scriptState["strings"] = json::object();

        for (const auto& [key, value] : state.script.flags) {
            scriptState["flags"][key] = value;
        }

        for (const auto& [key, value] : state.script.ints) {
            scriptState["ints"][key] = value;
        }

        for (const auto& [key, value] : state.script.strings) {
            scriptState["strings"][key] = value;
        }

        outRoot["scriptState"] = scriptState;
    }


    static const AudioDefinitionData* FindAudioDefinitionByMusicHandle(
            const GameState& state,
            int musicHandle)
    {
        for (const AudioDefinitionData& def : state.audio.definitions) {
            if (def.type == AudioType::Music && def.musicHandle == musicHandle) {
                return &def;
            }
        }

        return nullptr;
    }

    static void SerializeMusicSlotState(
            const GameState& state,
            const MusicPlaybackState& slot,
            json& outJson)
    {
        outJson = json::object();

        outJson["playing"] = slot.playing;
        outJson["volume"] = slot.volume;
        outJson["targetVolume"] = slot.targetVolume;
        outJson["fadeMode"] = static_cast<int>(slot.fadeMode);
        outJson["fadeElapsed"] = slot.fadeElapsed;
        outJson["fadeDuration"] = slot.fadeDuration;

        if (!slot.playing || slot.musicHandle < 0) {
            outJson["audioId"] = "";
            outJson["positionSeconds"] = 0.0f;
            return;
        }

        const AudioDefinitionData* def =
                FindAudioDefinitionByMusicHandle(state, slot.musicHandle);

        outJson["audioId"] = (def != nullptr) ? def->id : "";
        outJson["positionSeconds"] = GetMusicTimePlayed(*GetMusicResource(const_cast<GameState&>(state), slot.musicHandle));
    }

    static SavedMusicSlotState DeserializeMusicSlotState(const json& j)
    {
        SavedMusicSlotState out;
        out.audioId = j.value("audioId", "");
        out.playing = j.value("playing", false);
        out.positionSeconds = j.value("positionSeconds", 0.0f);
        out.volume = j.value("volume", 0.0f);
        out.targetVolume = j.value("targetVolume", 1.0f);
        out.fadeMode = j.value("fadeMode", 0);
        out.fadeElapsed = j.value("fadeElapsed", 0.0f);
        out.fadeDuration = j.value("fadeDuration", 0.0f);
        return out;
    }

    static void SerializeAudioState(const GameState& state, json& outRoot)
    {
        json audioState;
        audioState["musicAIsCurrent"] = state.audio.musicAIsCurrent;

        SerializeMusicSlotState(state, state.audio.musicA, audioState["musicA"]);
        SerializeMusicSlotState(state, state.audio.musicB, audioState["musicB"]);

        outRoot["audioState"] = audioState;
    }


    static bool ParseSaveFile(const fs::path& savePath, SaveRestoreData& outData)
    {
        outData = {};

        json root;
        {
            std::ifstream in(savePath);
            if (!in.is_open()) {
                TraceLog(LOG_ERROR, "Failed to open save file: %s", savePath.string().c_str());
                return false;
            }

            try {
                in >> root;
            } catch (const std::exception& ex) {
                TraceLog(LOG_ERROR,
                         "Failed parsing save file %s: %s",
                         savePath.string().c_str(),
                         ex.what());
                return false;
            }
        }

        const int version = root.value("version", 0);
        if (version != SAVE_VERSION) {
            TraceLog(LOG_ERROR,
                     "Unsupported save version %d in file: %s",
                     version,
                     savePath.string().c_str());
            return false;
        }

        outData.levelId = root.value("levelId", "");
        outData.saveName = root.value("saveName", "");
        outData.savedAt = root.value("savedAt", "");

        if (outData.levelId.empty()) {
            TraceLog(LOG_ERROR, "Save file missing levelId: %s", savePath.string().c_str());
            return false;
        }

        outData.controlsEnabled = root.value("controlsEnabled", true);


        if (root.contains("scriptState") && root["scriptState"].is_object()) {
            const json& scriptState = root["scriptState"];

            if (scriptState.contains("flags") && scriptState["flags"].is_object()) {
                for (auto it = scriptState["flags"].begin(); it != scriptState["flags"].end(); ++it) {
                    outData.flags[it.key()] = it.value().get<bool>();
                }
            }

            if (scriptState.contains("ints") && scriptState["ints"].is_object()) {
                for (auto it = scriptState["ints"].begin(); it != scriptState["ints"].end(); ++it) {
                    outData.ints[it.key()] = it.value().get<int>();
                }
            }

            if (scriptState.contains("strings") && scriptState["strings"].is_object()) {
                for (auto it = scriptState["strings"].begin(); it != scriptState["strings"].end(); ++it) {
                    outData.strings[it.key()] = it.value().get<std::string>();
                }
            }
        }

        if (root.contains("audioState") && root["audioState"].is_object()) {
            const json& audioState = root["audioState"];

            outData.audio.musicAIsCurrent = audioState.value("musicAIsCurrent", true);

            if (audioState.contains("musicA") && audioState["musicA"].is_object()) {
                outData.audio.musicA = DeserializeMusicSlotState(audioState["musicA"]);
            }

            if (audioState.contains("musicB") && audioState["musicB"].is_object()) {
                outData.audio.musicB = DeserializeMusicSlotState(audioState["musicB"]);
            }
        }

        if (!root.contains("topdownCore") || !root["topdownCore"].is_object()) {
            TraceLog(LOG_ERROR,
                     "Save file missing topdownCore: %s",
                     savePath.string().c_str());
            return false;
        }
        outData.topdownCore = DeserializeTopdownCoreRuntime(root);
        outData.controlsEnabled = outData.topdownCore.controlsEnabled;

        if (!root.contains("topdownNpcs") || !root["topdownNpcs"].is_object()) {
            TraceLog(LOG_ERROR,
                     "Save file missing topdownNpcs: %s",
                     savePath.string().c_str());
            return false;
        }
        outData.topdownNpcs = DeserializeTopdownNpcsRuntime(root);

        if (!root.contains("topdownWorld") || !root["topdownWorld"].is_object()) {
            TraceLog(LOG_ERROR,
                     "Save file missing topdownWorld: %s",
                     savePath.string().c_str());
            return false;
        }
        outData.topdownWorld = DeserializeTopdownWorldRuntime(root);

        return true;
    }

    static void RestoreScriptState(GameState& state, const SaveRestoreData& data)
    {
        state.script.flags = data.flags;
        state.script.ints = data.ints;
        state.script.strings = data.strings;
    }


    static bool RestoreMusicSlotState(
            GameState& state,
            const SavedMusicSlotState& saved,
            MusicPlaybackState& outSlot)
    {
        outSlot = {};

        if (!saved.playing || saved.audioId.empty()) {
            return true;
        }

        AudioDefinitionData* def = nullptr;
        auto it = state.audio.defIndexById.find(saved.audioId);
        if (it != state.audio.defIndexById.end()) {
            const int index = it->second;
            if (index >= 0 && index < static_cast<int>(state.audio.definitions.size())) {
                def = &state.audio.definitions[index];
            }
        }

        if (def == nullptr || def->type != AudioType::Music || def->musicHandle < 0) {
            TraceLog(LOG_WARNING,
                     "Failed restoring music slot, audio id not found or invalid: %s",
                     saved.audioId.c_str());
            return false;
        }

        Music* music = GetMusicResource(state, def->musicHandle);
        if (music == nullptr) {
            TraceLog(LOG_WARNING,
                     "Failed restoring music slot, resource missing: %s",
                     saved.audioId.c_str());
            return false;
        }

        PlayMusicStream(*music);

        if (saved.positionSeconds > 0.0f) {
            SeekMusicStream(*music, saved.positionSeconds);
        }

        outSlot.musicHandle = def->musicHandle;
        outSlot.playing = true;
        outSlot.volume = saved.volume;
        outSlot.targetVolume = saved.targetVolume;
        switch (saved.fadeMode) {
            case 1: outSlot.fadeMode = MusicFadeMode::FadeIn; break;
            case 2: outSlot.fadeMode = MusicFadeMode::FadeOut; break;
            default: outSlot.fadeMode = MusicFadeMode::None; break;
        }
        outSlot.fadeElapsed = saved.fadeElapsed;
        outSlot.fadeDuration = saved.fadeDuration;

        SetMusicVolume(*music, outSlot.volume * state.settings.musicVolume);
        return true;
    }

    static void RestoreAudioState(GameState& state, const SaveRestoreData& data)
    {
        // stop anything currently active first
        if (state.audio.musicA.playing && state.audio.musicA.musicHandle >= 0) {
            Music* music = GetMusicResource(state, state.audio.musicA.musicHandle);
            if (music != nullptr) {
                StopMusicStream(*music);
            }
        }

        if (state.audio.musicB.playing && state.audio.musicB.musicHandle >= 0) {
            Music* music = GetMusicResource(state, state.audio.musicB.musicHandle);
            if (music != nullptr) {
                StopMusicStream(*music);
            }
        }

        state.audio.musicA = {};
        state.audio.musicB = {};
        state.audio.musicAIsCurrent = data.audio.musicAIsCurrent;

        RestoreMusicSlotState(state, data.audio.musicA, state.audio.musicA);
        RestoreMusicSlotState(state, data.audio.musicB, state.audio.musicB);
    }

    static void UnloadCurrentTopdownLevelForSaveLoad(GameState& state)
    {
        if (state.topdown.runtime.levelActive) {
            TopdownRunLevelExitHook(state);
            ScriptSystemShutdown(state.script);
        }

        TopdownUnloadLevel(state);
    }

    static void RestorePersistentScriptState(GameState& state, const SaveRestoreData& data)
    {
        RestoreScriptState(state, data);
    }

    static bool ApplyPostLevelLoadSaveRestoreData(GameState& state, const SaveRestoreData& data)
    {
        RestoreAudioState(state, data);

        state.mode = GameMode::TopDown;
        return true;
    }
}

bool CanSaveGame(const GameState& state, std::string* outReason)
{
    auto fail = [&](const std::string& reason) {
        if (outReason != nullptr) {
            *outReason = reason;
        }
        return false;
    };

    const bool hasActiveTopdownLevel = state.topdown.runtime.levelActive;
    const bool isTopdownGameplay = state.mode == GameMode::TopDown;
    const bool isPausedTopdownMenu = state.mode == GameMode::Menu && hasActiveTopdownLevel;
    if (!isTopdownGameplay && !isPausedTopdownMenu) {
        return fail("Not in game");
    }

    if (!hasActiveTopdownLevel) {
        return fail("No active level");
    }

    if (state.topdown.currentLevelId.empty()) {
        return fail("No level id");
    }

    if (state.topdown.hasPendingLevelChange) {
        return fail("Cannot save during level transition");
    }

    if (state.topdown.runtime.returnToMenuRequested) {
        return fail("Cannot save now");
    }

    if (!state.topdown.runtime.controlsEnabled) {
        return fail("Cannot save now");
    }

    if (state.topdown.runtime.scriptedMove.active) {
        return fail("Cannot save now");
    }

    if (state.topdown.runtime.gameOverActive ||
        state.topdown.runtime.player.lifeState != TopdownPlayerLifeState::Alive) {
        return fail("Cannot save while dead");
    }

    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible || npc.dead || npc.corpse) {
            continue;
        }

        if (npc.engagementState == TopdownNpcEngagementState::Reacting ||
            npc.engagementState == TopdownNpcEngagementState::Engaged ||
            npc.combatState != TopdownNpcCombatState::None ||
            npc.hasPlayerTarget) {
            return fail("Cannot save in combat");
        }
    }

    if (outReason != nullptr) {
        outReason->clear();
    }
    return true;
}

bool SaveGameToSlot(GameState& state, int slotIndex)
{
    if (slotIndex < 1) {
        TraceLog(LOG_ERROR, "Invalid save slot index: %d", slotIndex);
        return false;
    }

    std::string reason;
    if (!CanSaveGame(state, &reason)) {
        TraceLog(LOG_WARNING, "Cannot save game: %s", reason.c_str());
        return false;
    }

    if (!EnsureSaveDirExists()) {
        TraceLog(LOG_ERROR, "Failed to create save directory");
        return false;
    }

    const std::string saveName =
            !state.topdown.currentLevelSaveName.empty()
            ? state.topdown.currentLevelSaveName
            : state.topdown.currentLevelId;

    json root;
    root["version"] = SAVE_VERSION;
    root["savedAt"] = BuildCurrentSaveTimestamp();
    root["levelId"] = state.topdown.currentLevelId;
    root["saveName"] = saveName;
    SerializeScriptState(state, root);
    SerializeAudioState(state, root);
    SerializeTopdownCoreRuntime(state, root);
    SerializeTopdownNpcsRuntime(state, root);
    SerializeTopdownWorldRuntime(state, root);

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    std::ofstream out(savePath);
    if (!out.is_open()) {
        TraceLog(LOG_ERROR, "Failed to open save slot for writing: %s", savePath.string().c_str());
        return false;
    }

    out << root.dump(4);

    TraceLog(LOG_INFO, "Saved game to slot %d: %s", slotIndex, savePath.string().c_str());
    return true;
}

bool LoadGameFromSlot(GameState& state, int slotIndex)
{
    if (slotIndex < 1) {
        TraceLog(LOG_ERROR, "Invalid load slot index: %d", slotIndex);
        return false;
    }

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    SaveRestoreData data;
    if (!ParseSaveFile(savePath, data)) {
        return false;
    }

    UnloadCurrentTopdownLevelForSaveLoad(state);

    RestorePersistentScriptState(state, data);

    if (!TopdownLoadLevelById(state, data.levelId.c_str())) {
        TraceLog(LOG_ERROR,
                 "Failed loading saved topdown level '%s' from slot %d",
                 data.levelId.c_str(),
                 slotIndex);
        state.mode = GameMode::Menu;
        return false;
    }

    RestoreTopdownCoreRuntime(state, data.topdownCore);
    RestoreTopdownNpcsRuntime(state, data.topdownNpcs);
    RestoreTopdownWorldRuntime(state, data.topdownWorld);

    return ApplyPostLevelLoadSaveRestoreData(state, data);
}

bool DoesSaveSlotExist(int slotIndex)
{
    if (slotIndex < 1) {
        return false;
    }

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    return fs::exists(savePath) && fs::is_regular_file(savePath);
}

std::string GetSaveSlotSummary(int slotIndex)
{
    if (slotIndex < 1) {
        return "Invalid";
    }

    const fs::path savePath = GetSaveSlotPath(slotIndex);
    if (!fs::exists(savePath) || !fs::is_regular_file(savePath)) {
        return "Empty";
    }

    json root;
    {
        std::ifstream in(savePath);
        if (!in.is_open()) {
            return "Unreadable";
        }

        try {
            in >> root;
        } catch (const std::exception&) {
            return "Corrupt";
        }
    }

    try {
        const std::string levelId = root.value("levelId", "");
        const std::string savedAt = root.value("savedAt", "");
        std::string saveName = root.value("saveName", "");
        if (saveName.empty()) {
            saveName = levelId;
        }
        if (saveName.empty()) {
            saveName = root.value("sceneId", "");
        }

        return FormatSaveSummary(saveName, savedAt);
    } catch (const std::exception&) {
        return "Corrupt";
    }
}
