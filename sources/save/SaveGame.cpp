#include "save/SaveGame.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cstdio>

#include "utils/json.hpp"
#include "scene/SceneLoad.h"
#include "adventure/AdventureHelpers.h"
#include "adventure/Inventory.h"
#include "adventure/Dialogue.h"
#include "debug/DebugConsole.h"
#include "resources/Resources.h"
#include "adventure/AdventureCamera.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static constexpr int SAVE_VERSION = 2;

    struct SavedInventoryData {
        std::string actorId;
        std::vector<std::string> itemIds;
        std::string heldItemId;
        int pageStartIndex = 0;
    };

    struct SavedActorState {
        std::string actorId;
        Vector2 feetPos{};
        std::string facing;
        bool visible = true;
        bool activeInScene = true;
        std::string currentAnimation;
        bool flipX = false;
        float animationTimeMs = 0.0f;
    };

    struct SavedPropState {
        std::string id;
        Vector2 feetPos{};
        bool visible = true;
        bool flipX = false;
        std::string currentAnimation;
        float animationTimeMs = 0.0f;
    };

    struct SavedEffectSpriteState {
        std::string id;
        bool visible = true;
        float opacity = 1.0f;
        Color tint = WHITE;
    };

    struct SavedEffectRegionState {
        std::string id;
        bool visible = true;
        float opacity = 1.0f;
    };

    struct SavedSoundEmitterState {
        std::string id;
        bool enabled = true;
        float volume = 1.0f;
    };

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

    struct SaveRestoreData {
        std::string sceneId;
        std::string controlledActorId;
        bool controlsEnabled = true;

        std::unordered_map<std::string, bool> flags;
        std::unordered_map<std::string, int> ints;
        std::unordered_map<std::string, std::string> strings;

        std::vector<SavedInventoryData> inventories;
        std::vector<SavedActorState> actors;
        std::vector<SavedPropState> props;
        std::vector<SavedEffectSpriteState> effectSprites;
        std::vector<SavedEffectRegionState> effectRegions;
        std::vector<SavedSoundEmitterState> soundEmitters;
        SavedAudioState audio;
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

    static const char* FacingToString(ActorFacing facing)
    {
        switch (facing) {
            case ActorFacing::Left:  return "left";
            case ActorFacing::Right: return "right";
            case ActorFacing::Back:  return "back";
            case ActorFacing::Front:
            default:                 return "front";
        }
    }

    static ActorFacing StringToActorFacing(const std::string& s)
    {
        if (s == "left") {
            return ActorFacing::Left;
        }
        if (s == "right") {
            return ActorFacing::Right;
        }
        if (s == "back") {
            return ActorFacing::Back;
        }
        return ActorFacing::Front;
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

    static void SerializeInventories(const GameState& state, json& outRoot)
    {
        outRoot["inventories"] = json::array();

        for (const ActorInventoryData& inv : state.adventure.actorInventories) {
            json j;
            j["actorId"] = inv.actorId;
            j["itemIds"] = inv.itemIds;
            j["heldItemId"] = inv.heldItemId;
            j["pageStartIndex"] = inv.pageStartIndex;
            outRoot["inventories"].push_back(j);
        }
    }

    static void SerializeActors(const GameState& state, json& outRoot)
    {
        outRoot["actors"] = json::array();

        for (const ActorInstance& actor : state.adventure.actors) {
            json j;
            j["actorId"] = actor.actorId;
            j["feetPos"] = SerializeVector2(actor.feetPos);
            j["facing"] = FacingToString(actor.facing);
            j["visible"] = actor.visible;
            j["activeInScene"] = actor.activeInScene;
            j["currentAnimation"] = actor.currentAnimation;
            j["flipX"] = actor.flipX;
            j["animationTimeMs"] = actor.animationTimeMs;
            outRoot["actors"].push_back(j);
        }
    }

    static void SerializeProps(const GameState& state, json& outRoot)
    {
        outRoot["props"] = json::array();

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.props.size()),
                static_cast<int>(state.adventure.props.size()));

        for (int i = 0; i < count; ++i) {
            const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
            const PropInstance& prop = state.adventure.props[i];

            json j;
            j["id"] = sceneProp.id;
            j["feetPos"] = SerializeVector2(prop.feetPos);
            j["visible"] = prop.visible;
            j["flipX"] = prop.flipX;
            j["currentAnimation"] = prop.currentAnimation;
            j["animationTimeMs"] = prop.animationTimeMs;
            outRoot["props"].push_back(j);
        }
    }

    static void SerializeEffectSprites(const GameState& state, json& outRoot)
    {
        outRoot["effectSprites"] = json::array();

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.effectSprites.size()),
                static_cast<int>(state.adventure.effectSprites.size()));

        for (int i = 0; i < count; ++i) {
            const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
            const EffectSpriteInstance& effect = state.adventure.effectSprites[i];

            json j;
            j["id"] = sceneEffect.id;
            j["visible"] = effect.visible;
            j["opacity"] = effect.opacity;
            j["tint"] = SerializeColor(effect.tint);
            outRoot["effectSprites"].push_back(j);
        }
    }

    static void SerializeEffectRegions(const GameState& state, json& outRoot)
    {
        outRoot["effectRegions"] = json::array();

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.effectRegions.size()),
                static_cast<int>(state.adventure.effectRegions.size()));

        for (int i = 0; i < count; ++i) {
            const SceneEffectRegionData& sceneEffectRegion = state.adventure.currentScene.effectRegions[i];
            const EffectRegionInstance& effectRegion = state.adventure.effectRegions[i];

            json j;
            j["id"] = sceneEffectRegion.id;
            j["visible"] = effectRegion.visible;
            j["opacity"] = effectRegion.opacity;
            outRoot["effectRegions"].push_back(j);
        }
    }

    static void SerializeSoundEmitters(const GameState& state, json& outRoot)
    {
        outRoot["soundEmitters"] = json::array();

        const int count = std::min(
                static_cast<int>(state.adventure.currentScene.soundEmitters.size()),
                static_cast<int>(state.audio.sceneEmitters.size()));

        for (int i = 0; i < count; ++i) {
            const SceneSoundEmitterData& sceneEmitter = state.adventure.currentScene.soundEmitters[i];
            const SoundEmitterInstance& emitter = state.audio.sceneEmitters[i];

            json j;
            j["id"] = sceneEmitter.id;
            j["enabled"] = emitter.enabled;
            j["volume"] = emitter.volume;
            outRoot["soundEmitters"].push_back(j);
        }
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
            in >> root;
        }

        const int version = root.value("version", 0);
        if (version != SAVE_VERSION) {
            TraceLog(LOG_ERROR,
                     "Unsupported save version %d in file: %s",
                     version,
                     savePath.string().c_str());
            return false;
        }

        outData.sceneId = root.value("sceneId", "");
        outData.controlledActorId = root.value("controlledActorId", "");
        outData.controlsEnabled = root.value("controlsEnabled", true);

        if (outData.sceneId.empty()) {
            TraceLog(LOG_ERROR, "Save file missing sceneId: %s", savePath.string().c_str());
            return false;
        }

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

        if (root.contains("inventories") && root["inventories"].is_array()) {
            for (const json& j : root["inventories"]) {
                SavedInventoryData inv;
                inv.actorId = j.value("actorId", "");
                inv.itemIds = j.value("itemIds", std::vector<std::string>{});
                inv.heldItemId = j.value("heldItemId", "");
                inv.pageStartIndex = j.value("pageStartIndex", 0);

                if (!inv.actorId.empty()) {
                    outData.inventories.push_back(inv);
                }
            }
        }

        if (root.contains("actors") && root["actors"].is_array()) {
            for (const json& j : root["actors"]) {
                SavedActorState actor;
                actor.actorId = j.value("actorId", "");
                actor.feetPos = j.contains("feetPos") ? DeserializeVector2(j["feetPos"]) : Vector2{};
                actor.facing = j.value("facing", "front");
                actor.visible = j.value("visible", true);
                actor.activeInScene = j.value("activeInScene", true);
                actor.currentAnimation = j.value("currentAnimation", "");
                actor.flipX = j.value("flipX", false);
                actor.animationTimeMs = j.value("animationTimeMs", 0.0f);

                if (!actor.actorId.empty()) {
                    outData.actors.push_back(actor);
                }
            }
        }

        if (root.contains("props") && root["props"].is_array()) {
            for (const json& j : root["props"]) {
                SavedPropState prop;
                prop.id = j.value("id", "");
                prop.feetPos = j.contains("feetPos") ? DeserializeVector2(j["feetPos"]) : Vector2{};
                prop.visible = j.value("visible", true);
                prop.flipX = j.value("flipX", false);
                prop.currentAnimation = j.value("currentAnimation", "");
                prop.animationTimeMs = j.value("animationTimeMs", 0.0f);

                if (!prop.id.empty()) {
                    outData.props.push_back(prop);
                }
            }
        }

        if (root.contains("effectSprites") && root["effectSprites"].is_array()) {
            for (const json& j : root["effectSprites"]) {
                SavedEffectSpriteState effect;
                effect.id = j.value("id", "");
                effect.visible = j.value("visible", true);
                effect.opacity = j.value("opacity", 1.0f);
                effect.tint = j.contains("tint") ? DeserializeColor(j["tint"]) : WHITE;

                if (!effect.id.empty()) {
                    outData.effectSprites.push_back(effect);
                }
            }
        }

        if (root.contains("effectRegions") && root["effectRegions"].is_array()) {
            for (const json& j : root["effectRegions"]) {
                SavedEffectRegionState effectRegion;
                effectRegion.id = j.value("id", "");
                effectRegion.visible = j.value("visible", true);
                effectRegion.opacity = j.value("opacity", 1.0f);

                if (!effectRegion.id.empty()) {
                    outData.effectRegions.push_back(effectRegion);
                }
            }
        }

        if (root.contains("soundEmitters") && root["soundEmitters"].is_array()) {
            for (const json& j : root["soundEmitters"]) {
                SavedSoundEmitterState emitter;
                emitter.id = j.value("id", "");
                emitter.enabled = j.value("enabled", true);
                emitter.volume = j.value("volume", 1.0f);

                if (!emitter.id.empty()) {
                    outData.soundEmitters.push_back(emitter);
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

        return true;
    }

    static void ClearTransientUiAndRuntimeState(GameState& state)
    {
        state.adventure.pendingInteraction = {};
        state.adventure.actionQueue = {};
        state.adventure.speechUi = {};
        state.adventure.ambientSpeechUis.clear();
        state.adventure.hoverUi = {};
        state.adventure.dialogueUi = {};

        state.adventure.inventoryUi.open = false;
        state.adventure.inventoryUi.openAmount = 0.0f;
        state.adventure.inventoryUi.closeDelayRemainingMs = 0.0f;
        state.adventure.inventoryUi.hoveringInventory = false;
        state.adventure.inventoryUi.hoveredSlotIndex = -1;
        state.adventure.inventoryUi.hoveringPrevPage = false;
        state.adventure.inventoryUi.hoveringNextPage = false;
        state.adventure.inventoryUi.pickupPopup = {};

        state.adventure.hasLastClickWorldPos = false;
        state.adventure.hasLastResolvedTargetPos = false;
        state.adventure.debugTrianglePath.clear();
    }

    static void RestoreScriptState(GameState& state, const SaveRestoreData& data)
    {
        state.script.flags = data.flags;
        state.script.ints = data.ints;
        state.script.strings = data.strings;
    }

    static void RestoreInventories(GameState& state, const SaveRestoreData& data)
    {
        state.adventure.actorInventories.clear();

        for (const SavedInventoryData& saved : data.inventories) {
            ActorInventoryData inv;
            inv.actorId = saved.actorId;
            inv.itemIds = saved.itemIds;
            inv.heldItemId = saved.heldItemId;
            inv.pageStartIndex = saved.pageStartIndex;
            state.adventure.actorInventories.push_back(inv);
        }
    }

    static void RestoreControlledActor(GameState& state, const SaveRestoreData& data)
    {
        if (data.controlledActorId.empty()) {
            return;
        }

        const int actorIndex = FindActorInstanceIndexById(state, data.controlledActorId);
        if (actorIndex >= 0) {
            state.adventure.controlledActorIndex = actorIndex;
        }
    }

    static void RestoreCameraFromControlledActor(GameState& state)
    {
        ActorInstance* controlledActor = GetControlledActor(state);
        if (controlledActor == nullptr) {
            return;
        }

        state.adventure.camera.mode = CameraModeData::FollowControlledActor;
        state.adventure.camera.followedActor = -1;
        state.adventure.camera.moving = false;
        state.adventure.camera.moveStart = {};
        state.adventure.camera.moveTarget = {};
        state.adventure.camera.moveElapsedMs = 0.0f;
        state.adventure.camera.moveDurationMs = 0.0f;
        state.adventure.camera.biasLatch = CameraBiasLatch::None;
        state.adventure.camera.currentBiasShiftX = 0.0f;
        state.adventure.camera.position =
                GetImmediateCenteredCameraPosition(state, *controlledActor);
    }

    static void RestoreActors(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedActorState& saved : data.actors) {
            const int actorIndex = FindActorInstanceIndexById(state, saved.actorId);
            if (actorIndex < 0 ||
                actorIndex >= static_cast<int>(state.adventure.actors.size())) {
                continue;
            }

            ActorInstance& actor = state.adventure.actors[actorIndex];
            actor.feetPos = saved.feetPos;
            actor.facing = StringToActorFacing(saved.facing);
            actor.visible = saved.visible;
            actor.activeInScene = saved.activeInScene;
            actor.flipX = saved.flipX;
            actor.animationTimeMs = saved.animationTimeMs;

            if (!saved.currentAnimation.empty()) {
                actor.currentAnimation = saved.currentAnimation;
            }

            actor.path = {};
            actor.scriptAnimationActive = false;
            actor.scriptAnimationDurationMs = 0.0f;
        }
    }

    static int FindScenePropIndexById(const GameState& state, const std::string& propId)
    {
        for (int i = 0; i < static_cast<int>(state.adventure.currentScene.props.size()); ++i) {
            if (state.adventure.currentScene.props[i].id == propId) {
                return i;
            }
        }
        return -1;
    }

    static int FindSceneEffectSpriteIndexById(const GameState& state, const std::string& effectId)
    {
        for (int i = 0; i < static_cast<int>(state.adventure.currentScene.effectSprites.size()); ++i) {
            if (state.adventure.currentScene.effectSprites[i].id == effectId) {
                return i;
            }
        }
        return -1;
    }

    static int FindSceneEffectRegionIndexById(const GameState& state, const std::string& effectRegionId)
    {
        for (int i = 0; i < static_cast<int>(state.adventure.currentScene.effectRegions.size()); ++i) {
            if (state.adventure.currentScene.effectRegions[i].id == effectRegionId) {
                return i;
            }
        }
        return -1;
    }

    static void RestoreProps(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedPropState& saved : data.props) {
            const int propIndex = FindScenePropIndexById(state, saved.id);
            if (propIndex < 0 ||
                propIndex >= static_cast<int>(state.adventure.props.size())) {
                continue;
            }

            PropInstance& prop = state.adventure.props[propIndex];
            prop.feetPos = saved.feetPos;
            prop.visible = saved.visible;
            prop.flipX = saved.flipX;
            prop.animationTimeMs = saved.animationTimeMs;

            if (!saved.currentAnimation.empty()) {
                prop.currentAnimation = saved.currentAnimation;
            }

            prop.oneShotActive = false;
            prop.oneShotDurationMs = 0.0f;
            prop.moveActive = false;
            prop.moveStartPos = prop.feetPos;
            prop.moveTargetPos = prop.feetPos;
            prop.moveElapsedMs = 0.0f;
            prop.moveDurationMs = 0.0f;
        }
    }

    static void RestoreEffectSprites(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedEffectSpriteState& saved : data.effectSprites) {
            const int effectIndex = FindSceneEffectSpriteIndexById(state, saved.id);
            if (effectIndex < 0 ||
                effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
                continue;
            }

            EffectSpriteInstance& effect = state.adventure.effectSprites[effectIndex];
            effect.visible = saved.visible;
            effect.opacity = saved.opacity;
            effect.tint = saved.tint;
        }
    }

    static void RestoreEffectRegions(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedEffectRegionState& saved : data.effectRegions) {
            const int effectRegionIndex = FindSceneEffectRegionIndexById(state, saved.id);
            if (effectRegionIndex < 0 ||
                effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
                continue;
            }

            EffectRegionInstance& effectRegion = state.adventure.effectRegions[effectRegionIndex];
            effectRegion.visible = saved.visible;
            effectRegion.opacity = saved.opacity;
        }
    }

    static int FindSceneSoundEmitterIndexById(const GameState& state, const std::string& emitterId)
    {
        for (int i = 0; i < static_cast<int>(state.adventure.currentScene.soundEmitters.size()); ++i) {
            if (state.adventure.currentScene.soundEmitters[i].id == emitterId) {
                return i;
            }
        }
        return -1;
    }

    static void RestoreSoundEmitters(GameState& state, const SaveRestoreData& data)
    {
        for (const SavedSoundEmitterState& saved : data.soundEmitters) {
            const int emitterIndex = FindSceneSoundEmitterIndexById(state, saved.id);
            if (emitterIndex < 0 ||
                emitterIndex >= static_cast<int>(state.audio.sceneEmitters.size())) {
                continue;
            }

            SoundEmitterInstance& emitter = state.audio.sceneEmitters[emitterIndex];
            emitter.enabled = saved.enabled;

            float volume = saved.volume;
            if (volume < 0.0f) volume = 0.0f;
            if (volume > 1.0f) volume = 1.0f;
            emitter.volume = volume;
        }
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

    static bool ApplySaveRestoreData(GameState& state, const SaveRestoreData& data)
    {
        if (!LoadSceneById(state, data.sceneId.c_str(), SceneLoadMode::FromSave)) {
            TraceLog(LOG_ERROR, "Failed to load save scene: %s", data.sceneId.c_str());
            return false;
        }

        RestoreScriptState(state, data);
        RestoreInventories(state, data);
        RestoreActors(state, data);
        RestoreProps(state, data);
        RestoreEffectSprites(state, data);
        RestoreEffectRegions(state, data);
        RestoreSoundEmitters(state, data);
        RestoreAudioState(state, data);
        RestoreControlledActor(state, data);
        RestoreCameraFromControlledActor(state);

        state.adventure.controlsEnabled = data.controlsEnabled;
        ClearTransientUiAndRuntimeState(state);
        state.mode = GameMode::Game;
        return true;
    }
}

bool SaveGameToSlot(GameState& state, int slotIndex)
{
    if (slotIndex < 1) {
        TraceLog(LOG_ERROR, "Invalid save slot index: %d", slotIndex);
        return false;
    }

    if (!state.adventure.currentScene.loaded) {
        TraceLog(LOG_ERROR, "Cannot save without a loaded scene");
        return false;
    }

    if (!EnsureSaveDirExists()) {
        TraceLog(LOG_ERROR, "Failed to create save directory");
        return false;
    }

    json root;
    root["version"] = SAVE_VERSION;
    root["sceneId"] = state.adventure.currentScene.sceneId;
    root["saveName"] = !state.adventure.currentScene.saveName.empty()
                       ? state.adventure.currentScene.saveName
                       : state.adventure.currentScene.sceneId;
    root["savedAt"] = BuildCurrentSaveTimestamp();
    root["controlsEnabled"] = state.adventure.controlsEnabled;

    const ActorInstance* controlledActor = GetControlledActor(state);
    root["controlledActorId"] = controlledActor != nullptr ? controlledActor->actorId : "";

    SerializeScriptState(state, root);
    SerializeInventories(state, root);
    SerializeActors(state, root);
    SerializeProps(state, root);
    SerializeEffectSprites(state, root);
    SerializeEffectRegions(state, root);
    SerializeSoundEmitters(state, root);
    SerializeAudioState(state, root);

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

    return ApplySaveRestoreData(state, data);
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
        in >> root;
    }

    const std::string saveName = root.value("saveName", root.value("sceneId", ""));
    const std::string savedAt = root.value("savedAt", "");

    return FormatSaveSummary(saveName, savedAt);
}
