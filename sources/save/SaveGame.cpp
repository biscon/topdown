#include "save/SaveGame.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cstdio>

#include "utils/json.hpp"
#include "debug/DebugConsole.h"
#include "resources/Resources.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static constexpr int SAVE_VERSION = 1;


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
        bool controlsEnabled = true;

        std::unordered_map<std::string, bool> flags;
        std::unordered_map<std::string, int> ints;
        std::unordered_map<std::string, std::string> strings;

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

    static bool ApplySaveRestoreData(GameState& state, const SaveRestoreData& data)
    {

        RestoreScriptState(state, data);

        RestoreAudioState(state, data);

        state.topdown.runtime.controlsEnabled = data.controlsEnabled;
        state.mode = GameMode::TopDown;
        return true;
    }
}

bool SaveGameToSlot(GameState& state, int slotIndex)
{
    if (slotIndex < 1) {
        TraceLog(LOG_ERROR, "Invalid save slot index: %d", slotIndex);
        return false;
    }

    if (!EnsureSaveDirExists()) {
        TraceLog(LOG_ERROR, "Failed to create save directory");
        return false;
    }

    json root;
    root["version"] = SAVE_VERSION;
    SerializeScriptState(state, root);
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
