#include "AsepriteAsset.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <fstream>

#include "utils/json.hpp"
#include "resources/TextureAsset.h"
#include "ui/TalkColors.h"
#include "raylib.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct ParsedFrameKey {
    std::string spriteName;
    std::string layerName;
    std::string tagName;
    int frameNumber = -1;
};

struct TagMeta {
    AnimationPlaybackDirection direction = AnimationPlaybackDirection::Forward;
    int repeatCount = 0;
};

static std::string NormalizePath(const char* filePath)
{
    return fs::path(filePath).lexically_normal().string();
}

static bool ParseFrameKey(const std::string& key, ParsedFrameKey& out)
{
    out = {};

    // ---------------------------------------------------------------------
    // Format 1:
    //   {title} ({layer}) #{tag} {frame}
    // Example:
    //   player (body) #idle 0
    // ---------------------------------------------------------------------
    const size_t layerOpen = key.find(" (");
    const size_t layerClose = key.find(')', layerOpen == std::string::npos ? 0 : layerOpen + 2);
    const size_t hashPos = key.find(" #", layerClose == std::string::npos ? 0 : layerClose + 1);
    const size_t lastSpace = key.find_last_of(' ');

    const bool looksLikeLayerTagFormat =
            layerOpen != std::string::npos &&
            layerClose != std::string::npos &&
            hashPos != std::string::npos &&
            lastSpace != std::string::npos &&
            lastSpace > hashPos + 2;

    if (looksLikeLayerTagFormat) {
        out.spriteName = key.substr(0, layerOpen);
        out.layerName = key.substr(layerOpen + 2, layerClose - (layerOpen + 2));
        out.tagName = key.substr(hashPos + 2, lastSpace - (hashPos + 2));

        try {
            out.frameNumber = std::stoi(key.substr(lastSpace + 1));
        } catch (...) {
            return false;
        }

        return true;
    }

    // ---------------------------------------------------------------------
    // Format 2:
    //   {title} {frame}
    // Example:
    //   feet_idle 0
    //
    // For this simpler flat format, we synthesize:
    //   layerName = "Layer"
    //   tagName   = "Default"
    // ---------------------------------------------------------------------
    if (lastSpace == std::string::npos || lastSpace + 1 >= key.size()) {
        return false;
    }

    const std::string namePart = key.substr(0, lastSpace);
    const std::string framePart = key.substr(lastSpace + 1);

    if (namePart.empty() || framePart.empty()) {
        return false;
    }

    try {
        out.frameNumber = std::stoi(framePart);
    } catch (...) {
        return false;
    }

    out.spriteName = namePart;
    out.layerName = "Layer";
    out.tagName = "Default";
    return true;
}

static AnimationPlaybackDirection ParseDirection(const std::string& s)
{
    if (s == "reverse") {
        return AnimationPlaybackDirection::Reverse;
    }
    if (s == "pingpong") {
        return AnimationPlaybackDirection::PingPong;
    }
    return AnimationPlaybackDirection::Forward;
}

static int ParseOptionalRepeat(const json& tag)
{
    auto it = tag.find("repeat");
    if (it == tag.end()) {
        return 0;
    }

    if (it->is_number_integer()) {
        return it->get<int>();
    }

    if (it->is_string()) {
        try {
            return std::stoi(it->get<std::string>());
        } catch (...) {
            return 0;
        }
    }

    return 0;
}

static std::string MakeClipKey(const std::string& layerName, const std::string& tagName)
{
    return layerName + "::" + tagName;
}

static SpriteAssetHandle LoadSpriteAssetInternal(
        ResourceData& resources,
        const std::string& cacheKey,
        const std::string& sidecarPathOrEmpty,
        const std::string& asepriteJsonPathString,
        float baseDrawScale,
        bool useRightForLeft,
        bool hasExplicitOrigin,
        Vector2 origin,
        ResourceScope scope)
{
    auto existing = resources.spriteAssetHandleByPath.find(cacheKey);
    if (existing != resources.spriteAssetHandleByPath.end()) {
        return existing->second;
    }

    json aseprite;
    {
        std::ifstream in(asepriteJsonPathString);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open Aseprite JSON: %s", asepriteJsonPathString.c_str());
            return -1;
        }
        in >> aseprite;
    }

    SpriteAssetResource asset;
    asset.handle = resources.nextSpriteAssetHandle++;
    asset.cacheKey = cacheKey;
    asset.sidecarPath = sidecarPathOrEmpty;
    asset.asepriteJsonPath = asepriteJsonPathString;
    asset.baseDrawScale = baseDrawScale;
    asset.useRightForLeft = useRightForLeft;
    asset.hasExplicitOrigin = hasExplicitOrigin;
    asset.origin = origin;
    asset.scope = scope;

    const json& meta = aseprite["meta"];
    const fs::path asepriteJsonPath = fs::path(asepriteJsonPathString);
    const fs::path asepriteDir = asepriteJsonPath.parent_path();
    const std::string imageRel = meta.value("image", "");
    if (imageRel.empty()) {
        TraceLog(LOG_ERROR, "Aseprite JSON missing meta.image: %s", asepriteJsonPathString.c_str());
        return -1;
    }

    asset.imagePath = (asepriteDir / imageRel).lexically_normal().string();
    asset.textureHandle = LoadTextureAsset(resources, asset.imagePath.c_str(), scope);
    if (asset.textureHandle < 0) {
        TraceLog(LOG_ERROR, "Failed to load Aseprite atlas: %s", asset.imagePath.c_str());
        return -1;
    }

    if (meta.contains("layers") && meta["layers"].is_array()) {
        for (const auto& layer : meta["layers"]) {
            asset.layerNames.push_back(layer.value("name", ""));
        }
    }

    std::unordered_map<std::string, TagMeta> tagMetaByName;
    if (meta.contains("frameTags") && meta["frameTags"].is_array()) {
        for (const auto& tag : meta["frameTags"]) {
            TagMeta tm;
            tm.direction = ParseDirection(tag.value("direction", "forward"));
            tm.repeatCount = ParseOptionalRepeat(tag);
            tagMetaByName[tag.value("name", "")] = tm;
        }
    }

    std::unordered_map<std::string, std::vector<std::pair<int, int>>> tempClipFrameMap;

    const json& frames = aseprite["frames"];
    for (auto it = frames.begin(); it != frames.end(); ++it) {
        const std::string frameKey = it.key();
        const json& frameJson = it.value();

        ParsedFrameKey parsed;
        if (!ParseFrameKey(frameKey, parsed)) {
            TraceLog(LOG_ERROR, "Failed to parse Aseprite frame key: %s", frameKey.c_str());
            return -1;
        }

        SpriteFrame frame;
        frame.sourceRect.x = frameJson["frame"].value("x", 0.0f);
        frame.sourceRect.y = frameJson["frame"].value("y", 0.0f);
        frame.sourceRect.width = frameJson["frame"].value("w", 0.0f);
        frame.sourceRect.height = frameJson["frame"].value("h", 0.0f);

        frame.trimmed = frameJson.value("trimmed", false);

        frame.spriteSourcePos.x = frameJson["spriteSourceSize"].value("x", 0.0f);
        frame.spriteSourcePos.y = frameJson["spriteSourceSize"].value("y", 0.0f);
        frame.spriteSourceSize.x = frameJson["spriteSourceSize"].value("w", 0.0f);
        frame.spriteSourceSize.y = frameJson["spriteSourceSize"].value("h", 0.0f);

        frame.sourceSize.x = frameJson["sourceSize"].value("w", 0.0f);
        frame.sourceSize.y = frameJson["sourceSize"].value("h", 0.0f);

        frame.durationMs = frameJson.value("duration", 0.0f);

        const int frameIndex = static_cast<int>(asset.frames.size());
        asset.frames.push_back(frame);

        tempClipFrameMap[MakeClipKey(parsed.layerName, parsed.tagName)].push_back({ parsed.frameNumber, frameIndex });
    }

    for (auto& kv : tempClipFrameMap) {
        auto& clipFrames = kv.second;
        std::sort(clipFrames.begin(), clipFrames.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
    }

    for (auto& kv : tempClipFrameMap) {
        const std::string& clipKey = kv.first;
        const size_t split = clipKey.find("::");
        if (split == std::string::npos) {
            continue;
        }

        const std::string layerName = clipKey.substr(0, split);
        const std::string tagName = clipKey.substr(split + 2);

        SpriteClip clip;
        clip.layerName = layerName;
        clip.name = tagName;

        auto metaIt = tagMetaByName.find(tagName);
        if (metaIt != tagMetaByName.end()) {
            clip.direction = metaIt->second.direction;
            clip.repeatCount = metaIt->second.repeatCount;
        }

        for (const auto& pair : kv.second) {
            clip.frameIndices.push_back(pair.second);
        }

        const int clipIndex = static_cast<int>(asset.clips.size());
        asset.clips.push_back(clip);
        asset.clipIndexByKey[clipKey] = clipIndex;
    }

    asset.loaded = true;

    const size_t index = resources.spriteAssets.size();
    resources.spriteAssets.push_back(asset);
    resources.spriteAssetIndexByHandle[asset.handle] = index;
    resources.spriteAssetHandleByPath[cacheKey] = asset.handle;

    TraceLog(LOG_INFO, "Loaded sprite asset: %s", asepriteJsonPathString.c_str());
    return asset.handle;
}

SpriteAssetHandle LoadSpriteAsset(
        ResourceData& resources,
        const char* sidecarPath,
        ResourceScope scope)
{
    const std::string sidecarNorm = NormalizePath(sidecarPath);

    json sidecar;
    {
        std::ifstream in(sidecarNorm);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open actor sidecar: %s", sidecarNorm.c_str());
            return -1;
        }
        in >> sidecar;
    }

    const fs::path sidecarDir = fs::path(sidecarNorm).parent_path();

    const std::string asepriteJsonRel = sidecar.value("asepriteJson", "");
    if (asepriteJsonRel.empty()) {
        TraceLog(LOG_ERROR, "Actor sidecar missing asepriteJson: %s", sidecarNorm.c_str());
        return -1;
    }

    const fs::path asepriteJsonPath = (sidecarDir / asepriteJsonRel).lexically_normal();

    Vector2 origin{};
    bool hasExplicitOrigin = false;

    if (sidecar.contains("origin") && sidecar["origin"].is_object()) {
        const auto& originJson = sidecar["origin"];
        origin.x = originJson.value("x", 0.0f);
        origin.y = originJson.value("y", 0.0f);
        hasExplicitOrigin = true;
    } else if (sidecar.contains("feetPivot") && sidecar["feetPivot"].is_object()) {
        // Temporary backward compatibility for old adventure data.
        const auto& feet = sidecar["feetPivot"];
        origin.x = feet.value("x", 0.0f);
        origin.y = feet.value("y", 0.0f);
        hasExplicitOrigin = true;
    }

    return LoadSpriteAssetInternal(
            resources,
            sidecarNorm,
            sidecarNorm,
            asepriteJsonPath.string(),
            sidecar.value("baseDrawScale", 1.0f),
            sidecar.value("useRightForLeft", true),
            hasExplicitOrigin,
            origin,
            scope);
}

SpriteAssetHandle LoadSpriteAssetFromAsepriteJson(
        ResourceData& resources,
        const char* asepriteJsonPath,
        float baseDrawScale,
        ResourceScope scope)
{
    const std::string asepriteNorm = NormalizePath(asepriteJsonPath);
    const std::string cacheKey =
            asepriteNorm +
            "|topdown|scale=" + std::to_string(baseDrawScale);

    return LoadSpriteAssetInternal(
            resources,
            cacheKey,
            "",
            asepriteNorm,
            baseDrawScale,
            false,
            false,
            Vector2{},
            scope);
}

SpriteAssetHandle LoadSpriteAssetFromAsepriteJsonWithOrigin(
        ResourceData& resources,
        const char* asepriteJsonPath,
        float baseDrawScale,
        Vector2 origin,
        ResourceScope scope)
{
    const std::string asepriteNorm = NormalizePath(asepriteJsonPath);
    const std::string cacheKey =
            asepriteNorm +
            "|topdown|scale=" + std::to_string(baseDrawScale) +
            "|origin=" + std::to_string(origin.x) + "," + std::to_string(origin.y);

    return LoadSpriteAssetInternal(
            resources,
            cacheKey,
            "",
            asepriteNorm,
            baseDrawScale,
            false,
            true,
            origin,
            scope);
}

SpriteAssetResource* FindSpriteAssetResource(ResourceData& resources, SpriteAssetHandle handle)
{
    auto it = resources.spriteAssetIndexByHandle.find(handle);
    if (it == resources.spriteAssetIndexByHandle.end()) {
        return nullptr;
    }
    return &resources.spriteAssets[it->second];
}

const SpriteAssetResource* FindSpriteAssetResource(const ResourceData& resources, SpriteAssetHandle handle)
{
    auto it = resources.spriteAssetIndexByHandle.find(handle);
    if (it == resources.spriteAssetIndexByHandle.end()) {
        return nullptr;
    }
    return &resources.spriteAssets.at(it->second);
}

int FindClipIndex(const SpriteAssetResource& asset, const std::string& layerName, const std::string& tagName)
{
    const std::string key = layerName + "::" + tagName;
    auto it = asset.clipIndexByKey.find(key);
    if (it == asset.clipIndexByKey.end()) {
        return -1;
    }
    return it->second;
}

static int GetEffectiveRepeatCount(const SpriteClip& clip)
{
    return (clip.repeatCount > 0) ? clip.repeatCount : 1;
}

static void AppendDirectionalPass(
        const SpriteClip& clip,
        bool reverse,
        std::vector<int>& outSequence)
{
    if (clip.frameIndices.empty()) {
        return;
    }

    if (!reverse) {
        for (int frameIndex : clip.frameIndices) {
            if (!outSequence.empty() && outSequence.back() == frameIndex) {
                continue;
            }
            outSequence.push_back(frameIndex);
        }
        return;
    }

    for (int i = static_cast<int>(clip.frameIndices.size()) - 1; i >= 0; --i) {
        const int frameIndex = clip.frameIndices[i];
        if (!outSequence.empty() && outSequence.back() == frameIndex) {
            continue;
        }
        outSequence.push_back(frameIndex);
    }
}

static void BuildOneShotSequence(const SpriteClip& clip, std::vector<int>& outSequence)
{
    outSequence.clear();

    if (clip.frameIndices.empty()) {
        return;
    }

    const int repeatCount = GetEffectiveRepeatCount(clip);

    switch (clip.direction) {
        case AnimationPlaybackDirection::Forward:
        {
            for (int i = 0; i < repeatCount; ++i) {
                AppendDirectionalPass(clip, false, outSequence);
            }
            break;
        }

        case AnimationPlaybackDirection::Reverse:
        {
            for (int i = 0; i < repeatCount; ++i) {
                AppendDirectionalPass(clip, true, outSequence);
            }
            break;
        }

        case AnimationPlaybackDirection::PingPong:
        {
            for (int i = 0; i < repeatCount; ++i) {
                const bool reverse = (i % 2) != 0;
                AppendDirectionalPass(clip, reverse, outSequence);
            }
            break;
        }
    }
}

float GetOneShotClipDurationMs(const SpriteAssetResource& asset, const SpriteClip& clip)
{
    std::vector<int> sequence;
    BuildOneShotSequence(clip, sequence);

    if (sequence.empty()) {
        return 0.0f;
    }

    float totalDuration = 0.0f;
    for (int frameIndex : sequence) {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(asset.frames.size())) {
            continue;
        }
        totalDuration += asset.frames[frameIndex].durationMs;
    }

    return totalDuration;
}

int GetOneShotFrameIndex(const SpriteAssetResource& asset, const SpriteClip& clip, float timeMs)
{
    std::vector<int> sequence;
    BuildOneShotSequence(clip, sequence);

    if (sequence.empty()) {
        return -1;
    }

    float totalDuration = 0.0f;
    for (int frameIndex : sequence) {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(asset.frames.size())) {
            continue;
        }
        totalDuration += asset.frames[frameIndex].durationMs;
    }

    if (totalDuration <= 0.0f) {
        return sequence.back();
    }

    float t = timeMs;
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t >= totalDuration) {
        return sequence.back();
    }

    for (int frameIndex : sequence) {
        if (frameIndex < 0 || frameIndex >= static_cast<int>(asset.frames.size())) {
            continue;
        }

        const float dur = asset.frames[frameIndex].durationMs;
        if (t < dur) {
            return frameIndex;
        }
        t -= dur;
    }

    return sequence.back();
}

int GetLoopingFrameIndex(const SpriteAssetResource& asset, const SpriteClip& clip, float timeMs)
{
    if (clip.frameIndices.empty()) {
        return -1;
    }

    std::vector<int> sequence;

    switch (clip.direction) {
        case AnimationPlaybackDirection::Forward:
        {
            sequence = clip.frameIndices;
            break;
        }

        case AnimationPlaybackDirection::Reverse:
        {
            sequence.reserve(clip.frameIndices.size());
            for (int i = static_cast<int>(clip.frameIndices.size()) - 1; i >= 0; --i) {
                sequence.push_back(clip.frameIndices[i]);
            }
            break;
        }

        case AnimationPlaybackDirection::PingPong:
        {
            sequence = clip.frameIndices;

            if (clip.frameIndices.size() >= 2) {
                for (int i = static_cast<int>(clip.frameIndices.size()) - 2; i > 0; --i) {
                    sequence.push_back(clip.frameIndices[i]);
                }
            }
            break;
        }
    }

    if (sequence.empty()) {
        return -1;
    }

    float totalDuration = 0.0f;
    for (int frameIndex : sequence) {
        totalDuration += asset.frames[frameIndex].durationMs;
    }

    if (totalDuration <= 0.0f) {
        return sequence.front();
    }

    float t = std::fmod(timeMs, totalDuration);
    if (t < 0.0f) {
        t += totalDuration;
    }

    for (int frameIndex : sequence) {
        const float dur = asset.frames[frameIndex].durationMs;
        if (t < dur) {
            return frameIndex;
        }
        t -= dur;
    }

    return sequence.back();
}

