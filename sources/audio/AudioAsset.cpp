#include "audio/AudioAsset.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "raylib.h"
#include "utils/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static bool ParseAudioType(const std::string& s, AudioType& outType)
{
    if (s == "sound") {
        outType = AudioType::Sound;
        return true;
    }
    if (s == "music") {
        outType = AudioType::Music;
        return true;
    }
    return false;
}

static bool ParseAudioGroup(const std::string& s, AudioGroup& outGroup)
{
    if (s == "sound") {
        outGroup = AudioGroup::Sound;
        return true;
    }
    if (s == "music") {
        outGroup = AudioGroup::Music;
        return true;
    }
    return false;
}

bool LoadAudioDefinitions(const std::string& path, std::vector<AudioDefinitionData>& outDefs)
{
    outDefs.clear();

    const fs::path jsonPath = fs::path(path).lexically_normal();
    const fs::path jsonDir = jsonPath.parent_path();

    json root;
    {
        std::ifstream in(jsonPath);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open audio definition file: %s", jsonPath.string().c_str());
            return false;
        }
        in >> root;
    }

    if (!root.contains("audio") || !root["audio"].is_array()) {
        TraceLog(LOG_ERROR, "Audio definition file missing 'audio' array: %s", jsonPath.string().c_str());
        return false;
    }

    std::unordered_set<std::string> seenIds;

    for (const auto& entry : root["audio"]) {
        AudioDefinitionData def;

        def.id = entry.value("id", "");
        if (def.id.empty()) {
            TraceLog(LOG_ERROR, "Audio definition missing id in file: %s", jsonPath.string().c_str());
            return false;
        }

        if (!seenIds.insert(def.id).second) {
            TraceLog(LOG_ERROR,
                     "Duplicate audio id '%s' in file: %s",
                     def.id.c_str(),
                     jsonPath.string().c_str());
            return false;
        }

        const std::string typeStr = entry.value("type", "");
        if (!ParseAudioType(typeStr, def.type)) {
            TraceLog(LOG_ERROR,
                     "Invalid audio type '%s' for id '%s' in file: %s",
                     typeStr.c_str(),
                     def.id.c_str(),
                     jsonPath.string().c_str());
            return false;
        }

        const std::string groupStr = entry.value(
                "group",
                def.type == AudioType::Music ? "music" : "sound");

        if (!ParseAudioGroup(groupStr, def.group)) {
            TraceLog(LOG_ERROR,
                     "Invalid audio group '%s' for id '%s' in file: %s",
                     groupStr.c_str(),
                     def.id.c_str(),
                     jsonPath.string().c_str());
            return false;
        }

        const std::string fileRel = entry.value("file", "");
        if (fileRel.empty()) {
            TraceLog(LOG_ERROR,
                     "Audio definition missing file for id '%s' in file: %s",
                     def.id.c_str(),
                     jsonPath.string().c_str());
            return false;
        }

        def.filePath = NormalizePath(jsonDir / fileRel);
        def.volume = entry.value("volume", 1.0f);
        def.loop = entry.value("loop", def.type == AudioType::Music);

        outDefs.push_back(def);
    }

    TraceLog(LOG_INFO,
             "Loaded audio definitions: %d from %s",
             static_cast<int>(outDefs.size()),
             jsonPath.string().c_str());

    return true;
}
