#include "cutscene/CutsceneRegistry.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>

#include "utils/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    static std::string NormalizePath(const fs::path& p)
    {
        return p.lexically_normal().string();
    }

    static fs::path ResolvePathRelativeToDirectory(const fs::path& dir, const std::string& path)
    {
        const fs::path sourcePath(path);
        if (sourcePath.is_absolute()) {
            return sourcePath.lexically_normal();
        }
        return (dir / sourcePath).lexically_normal();
    }

    static fs::path FindDescriptorPath(const fs::path& cutsceneDir)
    {
        const std::string directoryName = cutsceneDir.filename().string();
        const fs::path preferredPath = (cutsceneDir / (directoryName + ".json")).lexically_normal();
        if (fs::exists(preferredPath) && fs::is_regular_file(preferredPath)) {
            return preferredPath;
        }

        for (const auto& entry : fs::directory_iterator(cutsceneDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const fs::path path = entry.path();
            if (path.extension() == ".json") {
                return path.lexically_normal();
            }
        }

        return {};
    }

    static bool LoadCutsceneDescriptor(const fs::path& cutsceneDir,
                                       const fs::path& descriptorPath,
                                       CutsceneDefinition& outDefinition)
    {
        json root;
        {
            std::ifstream in(descriptorPath);
            if (!in.is_open()) {
                TraceLog(LOG_WARNING,
                         "Skipping cutscene directory '%s': failed opening descriptor %s",
                         cutsceneDir.string().c_str(),
                         descriptorPath.string().c_str());
                return false;
            }

            try {
                in >> root;
            } catch (const std::exception& e) {
                TraceLog(LOG_WARNING,
                         "Skipping cutscene directory '%s': failed parsing descriptor %s: %s",
                         cutsceneDir.string().c_str(),
                         descriptorPath.string().c_str(),
                         e.what());
                return false;
            }
        }

        if (!root.is_object()) {
            TraceLog(LOG_WARNING,
                     "Skipping cutscene directory '%s': descriptor root must be an object",
                     cutsceneDir.string().c_str());
            return false;
        }

        const std::string directoryName = cutsceneDir.filename().string();

        outDefinition = CutsceneDefinition{};
        outDefinition.cutsceneId = root.value("cutsceneId", std::string());
        if (outDefinition.cutsceneId.empty()) {
            TraceLog(LOG_WARNING,
                     "Cutscene descriptor %s missing cutsceneId; using directory name '%s'",
                     descriptorPath.string().c_str(),
                     directoryName.c_str());
            outDefinition.cutsceneId = directoryName;
        }

        outDefinition.descriptorPath = NormalizePath(descriptorPath);
        outDefinition.directoryPath = NormalizePath(cutsceneDir);

        outDefinition.baseAssetScale = root.value("baseAssetScale", 1);
        if (outDefinition.baseAssetScale < 1) {
            TraceLog(LOG_WARNING,
                     "Cutscene '%s' baseAssetScale %d is invalid; clamping to 1",
                     outDefinition.cutsceneId.c_str(),
                     outDefinition.baseAssetScale);
            outDefinition.baseAssetScale = 1;
        }

        const std::string scriptPath = root.value("script", std::string());
        if (!scriptPath.empty()) {
            outDefinition.scriptPath = NormalizePath(ResolvePathRelativeToDirectory(cutsceneDir, scriptPath));
        }

        if (root.contains("skip")) {
            const json& skip = root["skip"];
            if (skip.is_object()) {
                outDefinition.skipLevelId = skip.value("levelId", std::string());
                outDefinition.skipSpawnId = skip.value("spawnId", std::string());
            } else {
                TraceLog(LOG_WARNING,
                         "Cutscene '%s' skip field must be an object; ignoring skip data",
                         outDefinition.cutsceneId.c_str());
            }
        }

        if (root.contains("images")) {
            const json& images = root["images"];
            if (!images.is_object()) {
                TraceLog(LOG_WARNING,
                         "Cutscene '%s' images field must be an object; ignoring images",
                         outDefinition.cutsceneId.c_str());
            } else {
                for (auto it = images.begin(); it != images.end(); ++it) {
                    if (!it.value().is_string()) {
                        TraceLog(LOG_WARNING,
                                 "Cutscene '%s' image '%s' path must be a string; skipping image",
                                 outDefinition.cutsceneId.c_str(),
                                 it.key().c_str());
                        continue;
                    }

                    CutsceneImageDefinition image;
                    image.id = it.key();
                    image.path = NormalizePath(ResolvePathRelativeToDirectory(cutsceneDir, it.value().get<std::string>()));
                    outDefinition.images.push_back(image);
                }
            }
        }

        return true;
    }

    static void InsertOrReplaceCutsceneDefinition(GameState& state, const CutsceneDefinition& definition)
    {
        for (CutsceneDefinition& existing : state.cutscene.registry) {
            if (existing.cutsceneId == definition.cutsceneId) {
                TraceLog(LOG_WARNING,
                         "Duplicate cutsceneId '%s' from %s overrides previous descriptor %s",
                         definition.cutsceneId.c_str(),
                         definition.descriptorPath.c_str(),
                         existing.descriptorPath.c_str());
                existing = definition;
                return;
            }
        }

        state.cutscene.registry.push_back(definition);
    }
}

const CutsceneDefinition* FindCutsceneDefinitionById(const GameState& state, const std::string& cutsceneId)
{
    if (cutsceneId.empty()) {
        return nullptr;
    }

    for (const CutsceneDefinition& definition : state.cutscene.registry) {
        if (definition.cutsceneId == cutsceneId) {
            return &definition;
        }
    }

    return nullptr;
}

bool CutsceneScanRegistry(GameState& state)
{
    state.cutscene.registry.clear();

    const fs::path cutscenesDir = fs::path(ASSETS_PATH "cutscenes");
    if (!fs::exists(cutscenesDir) || !fs::is_directory(cutscenesDir)) {
        TraceLog(LOG_WARNING, "Cutscene directory missing: %s", cutscenesDir.string().c_str());
        return false;
    }

    for (const auto& entry : fs::directory_iterator(cutscenesDir)) {
        if (!entry.is_directory()) {
            continue;
        }

        const fs::path cutsceneDir = entry.path().lexically_normal();
        const fs::path descriptorPath = FindDescriptorPath(cutsceneDir);
        if (descriptorPath.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping cutscene directory '%s': missing json descriptor",
                     cutsceneDir.string().c_str());
            continue;
        }

        CutsceneDefinition definition;
        if (!LoadCutsceneDescriptor(cutsceneDir, descriptorPath, definition)) {
            continue;
        }

        InsertOrReplaceCutsceneDefinition(state, definition);
    }

    std::sort(
            state.cutscene.registry.begin(),
            state.cutscene.registry.end(),
            [](const CutsceneDefinition& a, const CutsceneDefinition& b) {
                return a.cutsceneId < b.cutsceneId;
            });

    TraceLog(LOG_INFO,
             "Scanned cutscene registry: %d cutscenes",
             static_cast<int>(state.cutscene.registry.size()));

    for (const CutsceneDefinition& definition : state.cutscene.registry) {
        TraceLog(LOG_INFO,
                 "  cutscene=%s scale=%d images=%d script=%s descriptor=%s",
                 definition.cutsceneId.c_str(),
                 definition.baseAssetScale,
                 static_cast<int>(definition.images.size()),
                 definition.scriptPath.empty() ? "<none>" : definition.scriptPath.c_str(),
                 definition.descriptorPath.c_str());
    }

    return true;
}
