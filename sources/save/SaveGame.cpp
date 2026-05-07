#include "save/SaveGame.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <exception>
#include <unordered_map>

#include "utils/json.hpp"
#include "debug/DebugConsole.h"
#include "resources/Resources.h"
#include "scripting/ScriptSystem.h"
#include "topdown/LevelLoad.h"
#include "topdown/LevelRegistry.h"
#include "topdown/LevelScripting.h"
#include "topdown/PlayerRegistry.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static constexpr int SAVE_VERSION = 3;


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
            TraceLog(LOG_WARNING,
                     "Save file missing topdownCore, using defaults: %s",
                     savePath.string().c_str());
        }
        outData.topdownCore = DeserializeTopdownCoreRuntime(root);
        outData.controlsEnabled = outData.topdownCore.controlsEnabled;

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

    if (!state.topdown.runtime.controlsEnabled) {
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
