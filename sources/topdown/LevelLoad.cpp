#include "topdown/LevelLoad.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>

#include "resources/TextureAsset.h"
#include "topdown/TopdownHelpers.h"
#include "utils/json.hpp"
#include "resources/Resources.h"
#include "LevelCamera.h"
#include "topdown/PlayerLoad.h"
#include "topdown/LevelScripting.h"
#include "scripting/ScriptSystem.h"
#include "nav/NavMeshBuild.h"
#include "topdown/NpcRegistry.h"
#include "BloodRenderTarget.h"
#include "TopdownRvo.h"
#include "LevelWindows.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static bool ParseTopdownEffectPlacement(
        const std::string& s,
        TopdownEffectPlacement& outPlacement)
{
    if (s == "after_bottom") {
        outPlacement = TopdownEffectPlacement::AfterBottom;
        return true;
    }

    if (s == "after_characters") {
        outPlacement = TopdownEffectPlacement::AfterCharacters;
        return true;
    }

    if (s == "final") {
        outPlacement = TopdownEffectPlacement::Final;
        return true;
    }

    return false;
}

static bool ParseEffectBlendModeString(
        const std::string& s,
        EffectBlendMode& outMode)
{
    if (s.empty() || s == "normal") {
        outMode = EffectBlendMode::Normal;
        return true;
    }

    if (s == "add") {
        outMode = EffectBlendMode::Add;
        return true;
    }

    if (s == "multiply") {
        outMode = EffectBlendMode::Multiply;
        return true;
    }

    return false;
}

static bool ParseEffectShaderTypeString(
        const std::string& s,
        EffectShaderType& outType)
{
    if (s.empty() || s == "none") {
        outType = EffectShaderType::None;
        return true;
    }

    if (s == "uv_scroll") {
        outType = EffectShaderType::UvScroll;
        return true;
    }

    if (s == "heat_shimmer") {
        outType = EffectShaderType::HeatShimmer;
        return true;
    }

    if (s == "region_grade") {
        outType = EffectShaderType::RegionGrade;
        return true;
    }

    if (s == "water_ripple") {
        outType = EffectShaderType::WaterRipple;
        return true;
    }

    if (s == "wind_sway") {
        outType = EffectShaderType::WindSway;
        return true;
    }

    if (s == "poly_clip") {
        outType = EffectShaderType::PolyClip;
        return true;
    }

    return false;
}

static bool ParseTopdownDoorHingeSide(
        const std::string& s,
        TopdownDoorHingeSide& outSide)
{
    if (s == "left") {
        outSide = TopdownDoorHingeSide::Left;
        return true;
    }

    if (s == "right") {
        outSide = TopdownDoorHingeSide::Right;
        return true;
    }

    if (s == "top") {
        outSide = TopdownDoorHingeSide::Top;
        return true;
    }

    if (s == "bottom") {
        outSide = TopdownDoorHingeSide::Bottom;
        return true;
    }

    return false;
}

static const char* TopdownDoorHingeSideToString(TopdownDoorHingeSide side)
{
    switch (side) {
        case TopdownDoorHingeSide::Left:   return "left";
        case TopdownDoorHingeSide::Right:  return "right";
        case TopdownDoorHingeSide::Top:    return "top";
        case TopdownDoorHingeSide::Bottom: return "bottom";
    }

    return "unknown";
}

static float ReadOptionalFloatProperty(const json& obj, const char* propertyName, float defaultValue)
{
    auto propsIt = obj.find("properties");
    if (propsIt == obj.end() || !propsIt->is_array()) {
        return defaultValue;
    }

    for (const auto& prop : *propsIt) {
        if (!prop.is_object()) {
            continue;
        }

        if (prop.value("name", std::string()) != propertyName) {
            continue;
        }

        if (prop.contains("value") && prop["value"].is_number()) {
            return prop["value"].get<float>();
        }
    }

    return defaultValue;
}

static const nlohmann::json* FindObjectProperty(
        const nlohmann::json& objectJson,
        const char* name)
{
    auto it = objectJson.find("properties");
    if (it == objectJson.end() || !it->is_array()) {
        return nullptr;
    }

    for (const auto& prop : *it) {
        if (prop.value("name", std::string()) == name) {
            return &prop;
        }
    }

    return nullptr;
}

static bool GetObjectPropertyBool(
        const nlohmann::json& objectJson,
        const char* name,
        bool defaultValue)
{
    const nlohmann::json* prop = FindObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", defaultValue);
}

static float GetObjectPropertyFloat(
        const nlohmann::json& objectJson,
        const char* name,
        float defaultValue)
{
    const nlohmann::json* prop = FindObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", defaultValue);
}

static std::string GetObjectPropertyString(
        const nlohmann::json& objectJson,
        const char* name,
        const std::string& defaultValue)
{
    const nlohmann::json* prop = FindObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", defaultValue);
}

static float Cross2D(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
}

static float DistanceSqr(Vector2 a, Vector2 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static bool IntersectRayWithSegment(
        Vector2 origin,
        Vector2 dir,
        const TopdownSegment& seg,
        float& outT,
        Vector2& outPoint)
{
    const Vector2 v1{
            seg.a.x - origin.x,
            seg.a.y - origin.y
    };

    const Vector2 v2{
            seg.b.x - seg.a.x,
            seg.b.y - seg.a.y
    };

    const float denom = Cross2D(dir, v2);
    if (std::fabs(denom) <= 0.000001f) {
        return false;
    }

    const float t = Cross2D(v1, v2) / denom;
    const float u = Cross2D(v1, dir) / denom;

    if (t < 0.0f) {
        return false;
    }

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    outT = t;
    outPoint = Vector2{
            origin.x + dir.x * t,
            origin.y + dir.y * t
    };
    return true;
}

static int HexDigitValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool ParseTiledColorString(const std::string& s, Color& outColor)
{
    // Support both:
    //   #RRGGBB
    //   #AARRGGBB

    if (s.empty() || s[0] != '#') {
        return false;
    }

    auto readByte = [&](int index, unsigned char& outByte) -> bool {
        const int hi = HexDigitValue(s[index]);
        const int lo = HexDigitValue(s[index + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        outByte = static_cast<unsigned char>((hi << 4) | lo);
        return true;
    };

    unsigned char a = 255;
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;

    if (s.size() == 7) {
        if (!readByte(1, r)) return false;
        if (!readByte(3, g)) return false;
        if (!readByte(5, b)) return false;

        outColor = Color{ r, g, b, 255 };
        return true;
    }

    if (s.size() == 9) {
        if (!readByte(1, a)) return false;
        if (!readByte(3, r)) return false;
        if (!readByte(5, g)) return false;
        if (!readByte(7, b)) return false;

        outColor = Color{ r, g, b, a };
        return true;
    }

    return false;
}

static Color GetLayerTintColor(const json& layer, Color defaultValue)
{
    const std::string value = layer.value("tintcolor", std::string());
    if (value.empty()) {
        return defaultValue;
    }

    Color parsed{};
    if (!ParseTiledColorString(value, parsed)) {
        return defaultValue;
    }

    return parsed;
}

static Color GetObjectPropertyColor(
        const nlohmann::json& objectJson,
        const char* name,
        Color defaultValue)
{
    const nlohmann::json* prop = FindObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }

    const std::string value = prop->value("value", std::string());
    Color parsed{};
    if (!ParseTiledColorString(value, parsed)) {
        return defaultValue;
    }

    return parsed;
}

static std::vector<Vector2> BuildWorldPolygonFromTiledPolygon(
        const json& obj,
        int baseAssetScale)
{
    std::vector<Vector2> points;

    if (!obj.contains("polygon") || !obj["polygon"].is_array()) {
        return points;
    }

    const float baseX = obj.value("x", 0.0f);
    const float baseY = obj.value("y", 0.0f);
    const float scale = static_cast<float>(baseAssetScale);

    for (const auto& p : obj["polygon"]) {
        Vector2 worldPoint{};
        worldPoint.x = (baseX + p.value("x", 0.0f)) * scale;
        worldPoint.y = (baseY + p.value("y", 0.0f)) * scale;
        points.push_back(worldPoint);
    }

    return points;
}

static std::vector<Vector2> BuildWorldPolygonFromTiledRect(
        const json& obj,
        int baseAssetScale)
{
    const float x = obj.value("x", 0.0f);
    const float y = obj.value("y", 0.0f);
    const float width = obj.value("width", 0.0f);
    const float height = obj.value("height", 0.0f);

    return TopdownBuildRectPolygon(x, y, width, height, static_cast<float>(baseAssetScale));
}

static Rectangle ComputeWorldBoundsFromPolygon(const std::vector<Vector2>& polygon)
{
    Rectangle bounds{};

    if (polygon.empty()) {
        return bounds;
    }

    float minX = polygon[0].x;
    float minY = polygon[0].y;
    float maxX = polygon[0].x;
    float maxY = polygon[0].y;

    for (const Vector2& p : polygon) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    bounds.x = minX;
    bounds.y = minY;
    bounds.width = maxX - minX;
    bounds.height = maxY - minY;
    return bounds;
}

static bool IsPointObject(const json& obj)
{
    auto it = obj.find("point");
    return it != obj.end() && it->is_boolean() && it->get<bool>();
}

static bool IsPolygonObject(const json& obj)
{
    auto it = obj.find("polygon");
    return it != obj.end() && it->is_array();
}

static bool IsRectObject(const json& obj)
{
    if (IsPointObject(obj) || IsPolygonObject(obj)) {
        return false;
    }

    return obj.contains("width") && obj.contains("height");
}

static bool ImportBoundaryLayer(
        TopdownData& topdown,
        const json& layer,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        TraceLog(LOG_ERROR, "LevelBoundary layer missing objects array");
        return false;
    }

    int polygonCount = 0;

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object()) {
            continue;
        }

        if (!IsPolygonObject(obj)) {
            continue;
        }

        std::vector<Vector2> points = BuildWorldPolygonFromTiledPolygon(obj, baseAssetScale);
        if (points.size() < 3) {
            continue;
        }

        topdown.authored.levelBoundary = points;
        ++polygonCount;
    }

    if (polygonCount != 1) {
        TraceLog(LOG_ERROR, "LevelBoundary must contain exactly one polygon, found %d", polygonCount);
        return false;
    }

    TopdownEnsureCounterClockwise(topdown.authored.levelBoundary);
    return true;
}

static void ImportObstacleLayer(
        TopdownData& topdown,
        const json& layer,
        int baseAssetScale,
        TopdownObstacleKind kind)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object()) {
            continue;
        }

        std::vector<Vector2> points;

        if (IsPolygonObject(obj)) {
            points = BuildWorldPolygonFromTiledPolygon(obj, baseAssetScale);
        } else if (IsRectObject(obj)) {
            points = BuildWorldPolygonFromTiledRect(obj, baseAssetScale);
        } else {
            continue;
        }

        if (points.size() < 3) {
            continue;
        }

        TopdownAuthoredPolygon poly;
        poly.tiledObjectId = obj.value("id", -1);
        poly.kind = kind;
        poly.name = obj.value("name", std::string());
        poly.points = points;
        poly.visible = obj.value("visible", true);

        TopdownEnsureClockwise(poly.points);
        topdown.authored.obstacles.push_back(poly);
    }
}

static void ImportSpawnLayer(
        TopdownData& topdown,
        const json& layer,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);

    // Cache these to keep the loop clean
    const float layerOffX = layer.value("offsetx", 0.0f);
    const float layerOffY = layer.value("offsety", 0.0f);

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object() || !IsPointObject(obj)) {
            continue;
        }

        TopdownAuthoredSpawn spawn;
        spawn.tiledObjectId = obj.value("id", -1);
        spawn.id = obj.value("name", std::string());

        // FIX: Add the offset to the local coordinate FIRST, then scale the result
        spawn.position.x = (obj.value("x", 0.0f) + layerOffX) * scale;
        spawn.position.y = (obj.value("y", 0.0f) + layerOffY) * scale;

        spawn.orientationDegrees = ReadOptionalFloatProperty(obj, "orientation", 0.0f);
        spawn.visible = obj.value("visible", true);

        topdown.authored.spawns.push_back(spawn);
    }
}

static void ImportNpcLayer(
        TopdownData& topdown,
        const json& layer,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);
    const float layerOffX = layer.value("offsetx", 0.0f);
    const float layerOffY = layer.value("offsety", 0.0f);

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object() || !IsPointObject(obj)) {
            continue;
        }

        TopdownAuthoredNpc npc;
        npc.tiledObjectId = obj.value("id", -1);
        npc.id = obj.value("name", std::string());
        npc.visible = obj.value("visible", true);

        npc.position.x = (obj.value("x", 0.0f) + layerOffX) * scale;
        npc.position.y = (obj.value("y", 0.0f) + layerOffY) * scale;
        npc.orientationDegrees = ReadOptionalFloatProperty(obj, "orientation", 0.0f);
        npc.assetId = GetObjectPropertyString(obj, "assetId", "");
        npc.persistentChase = GetObjectPropertyBool(obj, "persistentChase", false);

        if (npc.id.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown NPC object with empty name");
            continue;
        }

        if (npc.assetId.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown NPC '%s': missing assetId property",
                     npc.id.c_str());
            continue;
        }

        topdown.authored.npcs.push_back(npc);
    }
}

static void ImportDoorLayer(
        TopdownData& topdown,
        const json& layer,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);
    const float layerOffX = layer.value("offsetx", 0.0f);
    const float layerOffY = layer.value("offsety", 0.0f);

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object() || !IsRectObject(obj)) {
            continue;
        }

        if (!obj.value("visible", true)) {
            continue;
        }

        TopdownAuthoredDoor door;
        door.tiledObjectId = obj.value("id", -1);
        door.id = obj.value("name", std::string());
        door.visible = true;

        if (door.id.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown door with empty name");
            continue;
        }

        const float width = obj.value("width", 0.0f);
        const float height = obj.value("height", 0.0f);

        if (width <= 0.0f || height <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown door '%s': invalid rect size %.3f x %.3f",
                     door.id.c_str(),
                     width,
                     height);
            continue;
        }

        door.rectPosition.x = (obj.value("x", 0.0f) + layerOffX) * scale;
        door.rectPosition.y = (obj.value("y", 0.0f) + layerOffY) * scale;
        door.rectSize.x = width * scale;
        door.rectSize.y = height * scale;

        const std::string hingeSideStr =
                GetObjectPropertyString(obj, "hingeSide", "");
        if (!ParseTopdownDoorHingeSide(hingeSideStr, door.hingeSide)) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown door '%s': invalid or missing hingeSide '%s'",
                     door.id.c_str(),
                     hingeSideStr.c_str());
            continue;
        }

        const bool horizontal = door.rectSize.x >= door.rectSize.y;
        const bool hingeValid =
                (horizontal &&
                 (door.hingeSide == TopdownDoorHingeSide::Left ||
                  door.hingeSide == TopdownDoorHingeSide::Right)) ||
                (!horizontal &&
                 (door.hingeSide == TopdownDoorHingeSide::Top ||
                  door.hingeSide == TopdownDoorHingeSide::Bottom));

        if (!hingeValid) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown door '%s': hingeSide '%s' incompatible with rect %.3f x %.3f",
                     door.id.c_str(),
                     TopdownDoorHingeSideToString(door.hingeSide),
                     door.rectSize.x,
                     door.rectSize.y);
            continue;
        }

        door.locked = GetObjectPropertyBool(obj, "locked", false);

        door.autoClose = GetObjectPropertyBool(obj, "autoClose", false);
        door.autoCloseStrength = GetObjectPropertyFloat(obj, "autoCloseStrength", 6.0f);
        door.damping = GetObjectPropertyFloat(obj, "damping", 5.0f);

        if (door.autoCloseStrength < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Topdown door '%s' has negative autoCloseStrength %.3f; clamping to 0",
                     door.id.c_str(),
                     door.autoCloseStrength);
            door.autoCloseStrength = 0.0f;
        }

        if (door.damping < 0.0f) {
            TraceLog(LOG_WARNING,
                     "Topdown door '%s' has negative damping %.3f; clamping to 0",
                     door.id.c_str(),
                     door.damping);
            door.damping = 0.0f;
        }

        door.swingMinDegrees = GetObjectPropertyFloat(obj, "swingMinDeg", -90.0f);
        door.swingMaxDegrees = GetObjectPropertyFloat(obj, "swingMaxDeg", 90.0f);

        if (door.swingMinDegrees > door.swingMaxDegrees) {
            TraceLog(LOG_WARNING,
                     "Topdown door '%s' has swingMinDeg > swingMaxDeg; swapping values",
                     door.id.c_str());
            std::swap(door.swingMinDegrees, door.swingMaxDegrees);
        }

        door.openSoundId = GetObjectPropertyString(obj, "openSoundId", "");
        door.closeSoundId = GetObjectPropertyString(obj, "closeSoundId", "");

        door.color = GetObjectPropertyColor(
                obj,
                "color",
                Color{92, 58, 34, 255});

        door.outlineColor = GetObjectPropertyColor(
                obj,
                "outlineColor",
                BLACK);

        topdown.authored.doors.push_back(door);
    }
}

static void ImportWindowLayer(
        TopdownData& topdown,
        const json& layer,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);
    const float layerOffX = layer.value("offsetx", 0.0f);
    const float layerOffY = layer.value("offsety", 0.0f);

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object() || !IsRectObject(obj)) {
            continue;
        }

        if (!obj.value("visible", true)) {
            continue;
        }

        TopdownAuthoredWindow window;
        window.tiledObjectId = obj.value("id", -1);
        window.id = obj.value("name", std::string());
        window.visible = true;

        if (window.id.empty()) {
            TraceLog(LOG_WARNING, "Skipping topdown window with empty name");
            continue;
        }

        const float width = obj.value("width", 0.0f);
        const float height = obj.value("height", 0.0f);

        if (width <= 0.0f || height <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown window '%s': invalid rect size %.3f x %.3f",
                     window.id.c_str(),
                     width,
                     height);
            continue;
        }

        window.rectPosition.x = (obj.value("x", 0.0f) + layerOffX) * scale;
        window.rectPosition.y = (obj.value("y", 0.0f) + layerOffY) * scale;
        window.rectSize.x = width * scale;
        window.rectSize.y = height * scale;
        window.horizontal = window.rectSize.x >= window.rectSize.y;

        window.color1 = GetObjectPropertyColor(
                obj,
                "color1",
                Color{138, 196, 195, 255});

        window.color2 = GetObjectPropertyColor(
                obj,
                "color2",
                Color{100, 135, 140, 255});

        window.outlineColor = GetObjectPropertyColor(
                obj,
                "outlineColor",
                Color{23, 24, 25, 255});

        window.breakSoundId = GetObjectPropertyString(obj, "breakSoundId", "");

        window.breakParticleCount = static_cast<int>(
                GetObjectPropertyFloat(obj, "breakParticleCount", static_cast<float>(window.breakParticleCount)));

        window.breakParticleSpeedMin = GetObjectPropertyFloat(
                obj,
                "breakParticleSpeedMin",
                window.breakParticleSpeedMin);

        window.breakParticleSpeedMax = GetObjectPropertyFloat(
                obj,
                "breakParticleSpeedMax",
                window.breakParticleSpeedMax);

        window.breakParticleLifetimeMsMin = GetObjectPropertyFloat(
                obj,
                "breakParticleLifetimeMsMin",
                window.breakParticleLifetimeMsMin);

        window.breakParticleLifetimeMsMax = GetObjectPropertyFloat(
                obj,
                "breakParticleLifetimeMsMax",
                window.breakParticleLifetimeMsMax);

        window.breakParticleSizeMin = GetObjectPropertyFloat(
                obj,
                "breakParticleSizeMin",
                window.breakParticleSizeMin);

        window.breakParticleSizeMax = GetObjectPropertyFloat(
                obj,
                "breakParticleSizeMax",
                window.breakParticleSizeMax);

        window.breakParticleSpreadAlongWindow = GetObjectPropertyFloat(
                obj,
                "breakParticleSpreadAlongWindow",
                window.breakParticleSpreadAlongWindow);

        window.breakParticleColor1 = GetObjectPropertyColor(
                obj,
                "breakParticleColor1",
                window.breakParticleColor1);

        window.breakParticleColor2 = GetObjectPropertyColor(
                obj,
                "breakParticleColor2",
                window.breakParticleColor2);

        if (window.breakParticleCount < 0) {
            window.breakParticleCount = 0;
        }

        if (window.breakParticleSpeedMin > window.breakParticleSpeedMax) {
            std::swap(window.breakParticleSpeedMin, window.breakParticleSpeedMax);
        }

        if (window.breakParticleLifetimeMsMin > window.breakParticleLifetimeMsMax) {
            std::swap(window.breakParticleLifetimeMsMin, window.breakParticleLifetimeMsMax);
        }

        if (window.breakParticleSizeMin > window.breakParticleSizeMax) {
            std::swap(window.breakParticleSizeMin, window.breakParticleSizeMax);
        }

        topdown.authored.windows.push_back(window);
    }
}

static void ImportEffectRegionLayer(
        GameState& state,
        const json& layer,
        const fs::path& levelDir,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);

    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object()) {
            continue;
        }

        TopdownAuthoredEffectRegion effect;
        effect.tiledObjectId = obj.value("id", -1);
        effect.id = obj.value("name", std::string());

        if (effect.id.empty()) {
            TraceLog(LOG_ERROR, "Topdown effect region missing name");
            continue;
        }

        const bool hasPolygon =
                obj.contains("polygon") &&
                obj["polygon"].is_array() &&
                !obj["polygon"].empty();

        if (hasPolygon) {
            effect.usePolygon = true;
            effect.polygon = BuildWorldPolygonFromTiledPolygon(obj, baseAssetScale);

            if (effect.polygon.size() < 3) {
                TraceLog(LOG_ERROR,
                         "Topdown effect region polygon invalid: %s",
                         effect.id.c_str());
                continue;
            }

            effect.worldRect = ComputeWorldBoundsFromPolygon(effect.polygon);

            if (effect.worldRect.width <= 0.0f || effect.worldRect.height <= 0.0f) {
                TraceLog(LOG_ERROR,
                         "Topdown effect region polygon bounds invalid: %s",
                         effect.id.c_str());
                continue;
            }

            if (effect.polygon.size() > 32) {
                TraceLog(LOG_ERROR,
                         "Topdown effect region polygon has too many vertices (%d > 32): %s",
                         static_cast<int>(effect.polygon.size()),
                         effect.id.c_str());
                continue;
            }
        } else if (IsRectObject(obj)) {
            effect.usePolygon = false;

            effect.worldRect.x = obj.value("x", 0.0f) * scale;
            effect.worldRect.y = obj.value("y", 0.0f) * scale;
            effect.worldRect.width = obj.value("width", 0.0f) * scale;
            effect.worldRect.height = obj.value("height", 0.0f) * scale;

            if (effect.worldRect.width <= 0.0f || effect.worldRect.height <= 0.0f) {
                TraceLog(LOG_ERROR,
                         "Topdown effect region rect invalid: %s",
                         effect.id.c_str());
                continue;
            }
        } else {
            TraceLog(LOG_ERROR,
                     "Topdown effect region must be rect or polygon: %s",
                     effect.id.c_str());
            continue;
        }

        effect.visible = obj.value("visible", true);
        effect.opacity = obj.value("opacity", 1.0f);

        effect.tint = GetObjectPropertyColor(obj, "tintColor", WHITE);

        const std::string placementStr =
                GetObjectPropertyString(obj, "placement", "after_bottom");
        if (!ParseTopdownEffectPlacement(placementStr, effect.placement)) {
            TraceLog(LOG_WARNING,
                     "Invalid topdown effect region placement '%s' on '%s', defaulting to after_bottom",
                     placementStr.c_str(),
                     effect.id.c_str());
            effect.placement = TopdownEffectPlacement::AfterBottom;
        }

        effect.sortIndex = static_cast<int>(
                GetObjectPropertyFloat(obj, "sortIndex", 0.0f));

        const std::string blendModeStr =
                GetObjectPropertyString(obj, "blendMode", "normal");
        if (!ParseEffectBlendModeString(blendModeStr, effect.blendMode)) {
            TraceLog(LOG_WARNING,
                     "Invalid topdown effect region blendMode '%s' on '%s', defaulting to normal",
                     blendModeStr.c_str(),
                     effect.id.c_str());
            effect.blendMode = EffectBlendMode::Normal;
        }

        effect.occludedByWalls =
                GetObjectPropertyBool(obj, "occludedByWalls", false);

        effect.shaderIdString =
                GetObjectPropertyString(obj, "shaderId", "");
        if (!ParseEffectShaderTypeString(effect.shaderIdString, effect.shaderType)) {
            TraceLog(LOG_WARNING,
                     "Invalid topdown effect region shaderId '%s' on '%s', defaulting to none",
                     effect.shaderIdString.c_str(),
                     effect.id.c_str());
            effect.shaderType = EffectShaderType::None;
            effect.shaderIdString.clear();
        }

        const std::string assetRel =
                GetObjectPropertyString(obj, "asset", "");
        if (!assetRel.empty()) {
            const fs::path resolved = (levelDir / assetRel).lexically_normal();
            effect.imagePath = NormalizePath(resolved);

            TextureLoadSettings settings{};
            settings.premultiplyAlpha = true;
            settings.filter = TextureFilterMode::Point;
            settings.wrap = TextureWrapMode::Clamp;

            if (effect.shaderType == EffectShaderType::UvScroll ||
                effect.shaderType == EffectShaderType::WindSway) {
                settings.filter = TextureFilterMode::Bilinear;
                settings.wrap = TextureWrapMode::Repeat;
            } else if (effect.shaderType == EffectShaderType::PolyClip) {
                settings.filter = TextureFilterMode::Bilinear;
                settings.wrap = TextureWrapMode::Clamp;
            }

            effect.textureHandle = LoadTextureAsset(
                    state.resources,
                    effect.imagePath.c_str(),
                    settings,
                    ResourceScope::Scene);
        }

        const float occlusionOriginX =
                GetObjectPropertyFloat(obj, "occlusionOriginX", 0.0f);
        const float occlusionOriginY =
                GetObjectPropertyFloat(obj, "occlusionOriginY", 0.0f);

        const bool hasOcclusionOriginX = FindObjectProperty(obj, "occlusionOriginX") != nullptr;
        const bool hasOcclusionOriginY = FindObjectProperty(obj, "occlusionOriginY") != nullptr;

        if (hasOcclusionOriginX != hasOcclusionOriginY) {
            TraceLog(LOG_WARNING,
                     "Topdown effect region '%s' has only one of occlusionOriginX / occlusionOriginY; ignoring override",
                     effect.id.c_str());
        } else if (hasOcclusionOriginX && hasOcclusionOriginY) {
            effect.hasOcclusionOriginOverride = true;
            effect.occlusionOrigin.x = occlusionOriginX * scale;
            effect.occlusionOrigin.y = occlusionOriginY * scale;
        }

        effect.shaderParams.scrollSpeed.x = GetObjectPropertyFloat(obj, "scrollSpeedX", effect.shaderParams.scrollSpeed.x);
        effect.shaderParams.scrollSpeed.y = GetObjectPropertyFloat(obj, "scrollSpeedY", effect.shaderParams.scrollSpeed.y);
        effect.shaderParams.uvScale.x = GetObjectPropertyFloat(obj, "uvScaleX", effect.shaderParams.uvScale.x);
        effect.shaderParams.uvScale.y = GetObjectPropertyFloat(obj, "uvScaleY", effect.shaderParams.uvScale.y);
        effect.shaderParams.distortionAmount.x = GetObjectPropertyFloat(obj, "distortionX", effect.shaderParams.distortionAmount.x);
        effect.shaderParams.distortionAmount.y = GetObjectPropertyFloat(obj, "distortionY", effect.shaderParams.distortionAmount.y);
        effect.shaderParams.noiseScrollSpeed.x = GetObjectPropertyFloat(obj, "noiseSpeedX", effect.shaderParams.noiseScrollSpeed.x);
        effect.shaderParams.noiseScrollSpeed.y = GetObjectPropertyFloat(obj, "noiseSpeedY", effect.shaderParams.noiseScrollSpeed.y);
        effect.shaderParams.intensity = GetObjectPropertyFloat(obj, "intensity", effect.shaderParams.intensity);
        effect.shaderParams.phaseOffset = GetObjectPropertyFloat(obj, "phaseOffset", effect.shaderParams.phaseOffset);
        effect.shaderParams.brightness = GetObjectPropertyFloat(obj, "brightness", effect.shaderParams.brightness);
        effect.shaderParams.contrast = GetObjectPropertyFloat(obj, "contrast", effect.shaderParams.contrast);
        effect.shaderParams.saturation = GetObjectPropertyFloat(obj, "saturation", effect.shaderParams.saturation);
        effect.shaderParams.tintR = GetObjectPropertyFloat(obj, "tintR", effect.shaderParams.tintR);
        effect.shaderParams.tintG = GetObjectPropertyFloat(obj, "tintG", effect.shaderParams.tintG);
        effect.shaderParams.tintB = GetObjectPropertyFloat(obj, "tintB", effect.shaderParams.tintB);
        effect.shaderParams.softness = GetObjectPropertyFloat(obj, "softness", effect.shaderParams.softness);

        state.topdown.authored.effectRegions.push_back(effect);
    }
}

static void ImportImageGroup(
        GameState& state,
        const json& groupLayer,
        const fs::path& sceneDir,
        int baseAssetScale,
        TopdownImageLayerKind kind)
{
    if (!groupLayer.contains("layers") || !groupLayer["layers"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);

    for (const auto& layer : groupLayer["layers"]) {
        if (!layer.is_object()) {
            continue;
        }

        if (layer.value("type", std::string()) != "imagelayer") {
            continue;
        }

        const std::string imageRel = layer.value("image", std::string());

        TopdownAuthoredImageLayer img;
        img.tiledLayerId = layer.value("id", -1);
        img.kind = kind;
        img.name = layer.value("name", std::string());
        img.visible = layer.value("visible", true);
        img.opacity = layer.value("opacity", 1.0f);
        img.tint = GetLayerTintColor(layer, WHITE);

        const std::string blendModeStr = layer.value("mode", std::string());
        if (!ParseEffectBlendModeString(blendModeStr, img.blendMode)) {
            TraceLog(LOG_WARNING,
                     "Unsupported blend mode '%s' on topdown image layer '%s', falling back to normal",
                     blendModeStr.c_str(),
                     img.name.c_str());
            img.blendMode = EffectBlendMode::Normal;
        }

        img.shaderIdString = GetObjectPropertyString(layer, "shaderId", "");
        if (!ParseEffectShaderTypeString(img.shaderIdString, img.shaderType)) {
            TraceLog(LOG_WARNING,
                     "Unsupported shaderId '%s' on topdown image layer '%s', falling back to none",
                     img.shaderIdString.c_str(),
                     img.name.c_str());
            img.shaderType = EffectShaderType::None;
            img.shaderIdString.clear();
        }

        img.position.x = (layer.value("x", 0.0f) + layer.value("offsetx", 0.0f)) * scale;
        img.position.y = (layer.value("y", 0.0f) + layer.value("offsety", 0.0f)) * scale;

        img.imageSize.x = layer.value("imagewidth", 0.0f) * scale;
        img.imageSize.y = layer.value("imageheight", 0.0f) * scale;

        img.scale = GetObjectPropertyFloat(layer, "scale", 1.0f);
        if (img.scale <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "Topdown image layer '%s' has invalid scale %.3f, falling back to 1.0",
                     img.name.c_str(),
                     img.scale);
            img.scale = 1.0f;
        }

        img.shaderParams.scrollSpeed.x = GetObjectPropertyFloat(layer, "scrollSpeedX", img.shaderParams.scrollSpeed.x);
        img.shaderParams.scrollSpeed.y = GetObjectPropertyFloat(layer, "scrollSpeedY", img.shaderParams.scrollSpeed.y);
        img.shaderParams.uvScale.x = GetObjectPropertyFloat(layer, "uvScaleX", img.shaderParams.uvScale.x);
        img.shaderParams.uvScale.y = GetObjectPropertyFloat(layer, "uvScaleY", img.shaderParams.uvScale.y);
        img.shaderParams.distortionAmount.x = GetObjectPropertyFloat(layer, "distortionX", img.shaderParams.distortionAmount.x);
        img.shaderParams.distortionAmount.y = GetObjectPropertyFloat(layer, "distortionY", img.shaderParams.distortionAmount.y);
        img.shaderParams.noiseScrollSpeed.x = GetObjectPropertyFloat(layer, "noiseSpeedX", img.shaderParams.noiseScrollSpeed.x);
        img.shaderParams.noiseScrollSpeed.y = GetObjectPropertyFloat(layer, "noiseSpeedY", img.shaderParams.noiseScrollSpeed.y);
        img.shaderParams.intensity = GetObjectPropertyFloat(layer, "intensity", img.shaderParams.intensity);
        img.shaderParams.phaseOffset = GetObjectPropertyFloat(layer, "phaseOffset", img.shaderParams.phaseOffset);
        img.shaderParams.brightness = GetObjectPropertyFloat(layer, "brightness", img.shaderParams.brightness);
        img.shaderParams.contrast = GetObjectPropertyFloat(layer, "contrast", img.shaderParams.contrast);
        img.shaderParams.saturation = GetObjectPropertyFloat(layer, "saturation", img.shaderParams.saturation);
        img.shaderParams.tintR = GetObjectPropertyFloat(layer, "tintR", img.shaderParams.tintR);
        img.shaderParams.tintG = GetObjectPropertyFloat(layer, "tintG", img.shaderParams.tintG);
        img.shaderParams.tintB = GetObjectPropertyFloat(layer, "tintB", img.shaderParams.tintB);
        img.shaderParams.softness = GetObjectPropertyFloat(layer, "softness", img.shaderParams.softness);

        if (!imageRel.empty()) {
            const fs::path resolved = (sceneDir / imageRel).lexically_normal();
            img.imagePath = NormalizePath(resolved);

            TextureLoadSettings settings{};
            settings.premultiplyAlpha = true;
            settings.filter = TextureFilterMode::Point;
            settings.wrap = TextureWrapMode::Clamp;

            if (img.shaderType == EffectShaderType::UvScroll ||
                img.shaderType == EffectShaderType::WindSway) {
                settings.filter = TextureFilterMode::Bilinear;
                settings.wrap = TextureWrapMode::Repeat;
            }

            img.textureHandle = LoadTextureAsset(
                    state.resources,
                    img.imagePath.c_str(),
                    settings,
                    ResourceScope::Scene);
        }

        state.topdown.authored.imageLayers.push_back(img);
    }
}

static const TopdownAuthoredSpawn* FindSpawnById(const TopdownData& topdown, const char* spawnId)
{
    for (const TopdownAuthoredSpawn& spawn : topdown.authored.spawns) {
        if (spawn.id == spawnId) {
            return &spawn;
        }
    }
    return nullptr;
}

static void BuildRuntimeNpcsFromAuthored(GameState& state)
{
    TopdownData& topdown = state.topdown;
    topdown.runtime.npcs.clear();

    for (const TopdownAuthoredNpc& authored : topdown.authored.npcs) {
        if (!TopdownSpawnNpcRuntime(
                state,
                authored.id,
                authored.assetId,
                authored.position,
                authored.orientationDegrees,
                authored.visible,
                authored.persistentChase)) {
            TraceLog(LOG_WARNING,
                     "Skipping NPC '%s': failed spawning asset '%s'",
                     authored.id.c_str(),
                     authored.assetId.c_str());
        }
    }
}

static void BuildSortedEffectRegionBuckets(TopdownData& topdown)
{
    topdown.runtime.render.afterBottomEffectRegionIndices.clear();
    topdown.runtime.render.afterCharactersEffectRegionIndices.clear();
    topdown.runtime.render.finalEffectRegionIndices.clear();

    const int count = static_cast<int>(topdown.authored.effectRegions.size());

    for (int i = 0; i < count; ++i) {
        const TopdownAuthoredEffectRegion& authored = topdown.authored.effectRegions[i];

        switch (authored.placement) {
            case TopdownEffectPlacement::AfterBottom:
                topdown.runtime.render.afterBottomEffectRegionIndices.push_back(i);
                break;

            case TopdownEffectPlacement::AfterCharacters:
                topdown.runtime.render.afterCharactersEffectRegionIndices.push_back(i);
                break;

            case TopdownEffectPlacement::Final:
                topdown.runtime.render.finalEffectRegionIndices.push_back(i);
                break;
        }
    }

    auto stableSortBucket = [&](std::vector<int>& bucket) {
        std::stable_sort(
                bucket.begin(),
                bucket.end(),
                [&](int a, int b) {
                    const TopdownAuthoredEffectRegion& ea = topdown.authored.effectRegions[a];
                    const TopdownAuthoredEffectRegion& eb = topdown.authored.effectRegions[b];
                    return ea.sortIndex < eb.sortIndex;
                });
    };

    stableSortBucket(topdown.runtime.render.afterBottomEffectRegionIndices);
    stableSortBucket(topdown.runtime.render.afterCharactersEffectRegionIndices);
    stableSortBucket(topdown.runtime.render.finalEffectRegionIndices);
}

struct TopdownOcclusionHitPoint {
    float angle = 0.0f;
    Vector2 point{};
};

struct TopdownOcclusionSegmentCache {
    std::vector<TopdownSegment> segments;
    std::vector<Rectangle> bounds;
};

static Rectangle BuildSegmentBounds(const TopdownSegment& seg)
{
    const float minX = std::min(seg.a.x, seg.b.x);
    const float minY = std::min(seg.a.y, seg.b.y);
    const float maxX = std::max(seg.a.x, seg.b.x);
    const float maxY = std::max(seg.a.y, seg.b.y);

    return Rectangle{
            minX,
            minY,
            maxX - minX,
            maxY - minY
    };
}

static bool RectsOverlap(const Rectangle& a, const Rectangle& b)
{
    return !(a.x + a.width < b.x ||
             b.x + b.width < a.x ||
             a.y + a.height < b.y ||
             b.y + b.height < a.y);
}

static Rectangle BuildOcclusionInterestBounds(
        const TopdownAuthoredEffectRegion& effect,
        Vector2 origin)
{
    Rectangle r = effect.worldRect;

    if (effect.usePolygon && !effect.polygon.empty()) {
        r = TopdownComputePolygonBounds(effect.polygon);
    }

    const float minX = std::min(r.x, origin.x);
    const float minY = std::min(r.y, origin.y);
    const float maxX = std::max(r.x + r.width, origin.x);
    const float maxY = std::max(r.y + r.height, origin.y);

    static constexpr float kMargin = 96.0f;
    return Rectangle{
            minX - kMargin,
            minY - kMargin,
            (maxX - minX) + kMargin * 2.0f,
            (maxY - minY) + kMargin * 2.0f
    };
}

static void BuildWallOcclusionSegments(
        const TopdownData& topdown,
        TopdownOcclusionSegmentCache& outCache)
{
    outCache.segments.clear();
    outCache.bounds.clear();

    const size_t targetCount =
            topdown.runtime.collision.visionSegments.size() +
            topdown.runtime.collision.boundarySegments.size() +
            topdown.runtime.doors.size() * 4;

    if (outCache.segments.capacity() < targetCount) {
        outCache.segments.reserve(targetCount);
    }
    if (outCache.bounds.capacity() < targetCount) {
        outCache.bounds.reserve(targetCount);
    }

    auto pushSegment = [&](const TopdownSegment& seg) {
        outCache.segments.push_back(seg);
        outCache.bounds.push_back(BuildSegmentBounds(seg));
    };

    for (const TopdownSegment& seg : topdown.runtime.collision.visionSegments) {
        pushSegment(seg);
    }

    for (const TopdownSegment& seg : topdown.runtime.collision.boundarySegments) {
        pushSegment(seg);
    }

    for (const TopdownRuntimeDoor& door : topdown.runtime.doors) {
        if (!door.visible) {
            continue;
        }

        Vector2 a{};
        Vector2 b{};
        Vector2 c{};
        Vector2 d{};
        TopdownBuildDoorCorners(door, a, b, c, d);

        pushSegment(TopdownSegment{a, b});
        pushSegment(TopdownSegment{b, c});
        pushSegment(TopdownSegment{c, d});
        pushSegment(TopdownSegment{d, a});
    }
}

static bool BuildWallOcclusionPolygon(
        const TopdownOcclusionSegmentCache& segmentCache,
        const TopdownAuthoredEffectRegion& effect,
        std::vector<Vector2>& outPolygon)
{
    static constexpr int kMaxOcclusionPolygonPoints = 256;
    static thread_local std::vector<float> rayAngles;
    static thread_local std::vector<TopdownOcclusionHitPoint> hits;
    static thread_local std::vector<Vector2> reducedPolygon;
    static thread_local std::vector<int> candidateSegmentIndices;

    outPolygon.clear();

    const Vector2 origin = TopdownComputeEffectRegionOcclusionOrigin(effect);
    const Rectangle interestBounds = BuildOcclusionInterestBounds(effect, origin);

    candidateSegmentIndices.clear();
    const size_t segmentCount = segmentCache.segments.size();
    if (segmentCount != segmentCache.bounds.size() || segmentCount == 0) {
        return false;
    }

    if (candidateSegmentIndices.capacity() < segmentCount) {
        candidateSegmentIndices.reserve(segmentCount);
    }

    for (size_t i = 0; i < segmentCount; ++i) {
        if (!RectsOverlap(interestBounds, segmentCache.bounds[i])) {
            continue;
        }
        candidateSegmentIndices.push_back(static_cast<int>(i));
    }

    if (candidateSegmentIndices.empty()) {
        return false;
    }

    rayAngles.clear();
    const size_t targetAngleCount = candidateSegmentIndices.size() * 6;
    if (rayAngles.capacity() < targetAngleCount) {
        rayAngles.reserve(targetAngleCount);
    }

    static constexpr float kAngleEpsilon = 0.0002f;

    for (int candidateIndex : candidateSegmentIndices) {
        const TopdownSegment& seg = segmentCache.segments[candidateIndex];
        const Vector2 endpoints[2] = { seg.a, seg.b };

        for (const Vector2& endpoint : endpoints) {
            const float angle = std::atan2(endpoint.y - origin.y, endpoint.x - origin.x);
            rayAngles.push_back(angle - kAngleEpsilon);
            rayAngles.push_back(angle);
            rayAngles.push_back(angle + kAngleEpsilon);
        }
    }

    hits.clear();
    if (hits.capacity() < rayAngles.size()) {
        hits.reserve(rayAngles.size());
    }

    for (float angle : rayAngles) {
        const Vector2 dir{
                std::cos(angle),
                std::sin(angle)
        };

        bool foundHit = false;
        float bestT = 0.0f;
        Vector2 bestPoint{};

        for (int candidateIndex : candidateSegmentIndices) {
            const TopdownSegment& seg = segmentCache.segments[candidateIndex];
            float t = 0.0f;
            Vector2 point{};

            if (!IntersectRayWithSegment(origin, dir, seg, t, point)) {
                continue;
            }

            if (!foundHit || t < bestT) {
                foundHit = true;
                bestT = t;
                bestPoint = point;
            }
        }

        if (foundHit) {
            TopdownOcclusionHitPoint hit;
            hit.angle = angle;
            hit.point = bestPoint;
            hits.push_back(hit);
        }
    }

    if (hits.size() < 3) {
        return false;
    }

    std::sort(
            hits.begin(),
            hits.end(),
            [](const TopdownOcclusionHitPoint& a, const TopdownOcclusionHitPoint& b) {
                return a.angle < b.angle;
            });

    outPolygon.reserve(hits.size());

    static constexpr float kPointMergeDistanceSqr = 1.0f;

    for (const TopdownOcclusionHitPoint& hit : hits) {
        if (!outPolygon.empty() &&
            DistanceSqr(outPolygon.back(), hit.point) <= kPointMergeDistanceSqr) {
            continue;
        }

        outPolygon.push_back(hit.point);
    }

    if (outPolygon.size() >= 2 &&
        DistanceSqr(outPolygon.front(), outPolygon.back()) <= kPointMergeDistanceSqr) {
        outPolygon.pop_back();
    }

    if (outPolygon.size() < 3) {
        outPolygon.clear();
        return false;
    }

    if (outPolygon.size() > static_cast<size_t>(kMaxOcclusionPolygonPoints)) {
        reducedPolygon.clear();
        reducedPolygon.reserve(kMaxOcclusionPolygonPoints);

        const float step = static_cast<float>(outPolygon.size()) /
                           static_cast<float>(kMaxOcclusionPolygonPoints);

        for (int i = 0; i < kMaxOcclusionPolygonPoints; ++i) {
            const int sourceIndex = static_cast<int>(std::floor(step * static_cast<float>(i)));
            const int clampedIndex =
                    std::clamp(sourceIndex, 0, static_cast<int>(outPolygon.size()) - 1);
            reducedPolygon.push_back(outPolygon[clampedIndex]);
        }

        TraceLog(LOG_WARNING,
                 "Wall occlusion polygon for effect '%s' has %d points, downsampling to %d",
                 effect.id.c_str(),
                 static_cast<int>(outPolygon.size()),
                 kMaxOcclusionPolygonPoints);

        outPolygon.swap(reducedPolygon);
    }

    return true;
}

static bool IsEffectRegionNearCameraView(
        const TopdownData& topdown,
        const TopdownAuthoredEffectRegion& authored)
{
    const float margin = 128.0f;
    const Rectangle view{
            topdown.runtime.camera.position.x - margin,
            topdown.runtime.camera.position.y - margin,
            topdown.camera.viewportWidth + margin * 2.0f,
            topdown.camera.viewportHeight + margin * 2.0f
    };

    const Rectangle& r = authored.worldRect;
    return !(r.x + r.width < view.x ||
             view.x + view.width < r.x ||
             r.y + r.height < view.y ||
             view.y + view.height < r.y);
}

void TopdownRebuildWallOcclusionPolygons(TopdownData& topdown, bool forceFullRebuild)
{
    TopdownRenderWorld& render = topdown.runtime.render;
    const Vector2 cameraPos = topdown.runtime.camera.position;

    bool cameraMoved = true;
    if (render.hasOcclusionRebuildCameraCache) {
        const Vector2 delta = TopdownSub(cameraPos, render.occlusionRebuildLastCamera);
        cameraMoved = TopdownLengthSqr(delta) > 0.25f;
    }

    bool anyDoorChanged = false;
    const size_t doorCount = topdown.runtime.doors.size();

    if (render.occlusionRebuildLastDoorAngles.size() != doorCount) {
        render.occlusionRebuildLastDoorAngles.assign(doorCount, 0.0f);
        anyDoorChanged = true;
    }

    for (size_t i = 0; i < doorCount; ++i) {
        const TopdownRuntimeDoor& door = topdown.runtime.doors[i];
        const float prev = render.occlusionRebuildLastDoorAngles[i];
        const float current = door.angleRadians;

        if (std::fabs(current - prev) > 0.0005f) {
            anyDoorChanged = true;
        }
        render.occlusionRebuildLastDoorAngles[i] = current;
    }

    if (!forceFullRebuild && !cameraMoved && !anyDoorChanged) {
        return;
    }

    static thread_local TopdownOcclusionSegmentCache wallOcclusionCache;
    BuildWallOcclusionSegments(topdown, wallOcclusionCache);

    for (int i = 0; i < static_cast<int>(topdown.authored.effectRegions.size()); ++i) {
        if (i >= static_cast<int>(topdown.runtime.render.effectRegions.size())) {
            break;
        }

        const TopdownAuthoredEffectRegion& authored = topdown.authored.effectRegions[i];
        TopdownRuntimeEffectRegion& runtime = topdown.runtime.render.effectRegions[i];

        runtime.occludedByWalls = authored.occludedByWalls;
        runtime.hasWallOcclusionPolygon = false;
        runtime.wallOcclusionPolygon.clear();

        if (!runtime.occludedByWalls) {
            continue;
        }

        if (!forceFullRebuild && !IsEffectRegionNearCameraView(topdown, authored)) {
            continue;
        }

        runtime.hasWallOcclusionPolygon =
                BuildWallOcclusionPolygon(wallOcclusionCache, authored, runtime.wallOcclusionPolygon);

        if (!runtime.hasWallOcclusionPolygon) {
            TraceLog(LOG_WARNING,
                     "Failed building wall occlusion polygon for effect region '%s'",
                     authored.id.c_str());
        }
    }

    render.occlusionRebuildLastCamera = cameraPos;
    render.hasOcclusionRebuildCameraCache = true;
}

static TopdownRuntimeDoor BuildRuntimeDoorFromAuthored(
        const TopdownAuthoredDoor& authored)
{
    TopdownRuntimeDoor runtime;
    runtime.tiledObjectId = authored.tiledObjectId;
    runtime.id = authored.id;
    runtime.visible = authored.visible;

    runtime.locked = authored.locked;

    runtime.autoClose = authored.autoClose;
    runtime.autoCloseStrength = authored.autoCloseStrength;
    runtime.damping = authored.damping;

    runtime.swingMinRadians = authored.swingMinDegrees * DEG2RAD;
    runtime.swingMaxRadians = authored.swingMaxDegrees * DEG2RAD;

    runtime.openSoundId = authored.openSoundId;
    runtime.closeSoundId = authored.closeSoundId;

    runtime.color = authored.color;
    runtime.outlineColor = authored.outlineColor;

    runtime.wasNearClosed = true;
    runtime.openSoundPlayedThisSwing = false;

    const Rectangle rect{
            authored.rectPosition.x,
            authored.rectPosition.y,
            authored.rectSize.x,
            authored.rectSize.y
    };

    const bool horizontal = rect.width >= rect.height;

    runtime.length = horizontal ? rect.width : rect.height;
    runtime.thickness = horizontal ? rect.height : rect.width;

    const float centerX = rect.x + rect.width * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;

    switch (authored.hingeSide) {
        case TopdownDoorHingeSide::Left:
            runtime.hinge = Vector2{ rect.x, centerY };
            runtime.closedAngleRadians = 0.0f;
            break;

        case TopdownDoorHingeSide::Right:
            runtime.hinge = Vector2{ rect.x + rect.width, centerY };
            runtime.closedAngleRadians = PI;
            break;

        case TopdownDoorHingeSide::Top:
            runtime.hinge = Vector2{ centerX, rect.y };
            runtime.closedAngleRadians = PI * 0.5f;
            break;

        case TopdownDoorHingeSide::Bottom:
            runtime.hinge = Vector2{ centerX, rect.y + rect.height };
            runtime.closedAngleRadians = -PI * 0.5f;
            break;
    }

    runtime.angleRadians = runtime.closedAngleRadians;
    runtime.angularVelocity = 0.0f;

    return runtime;
}

static void BuildRuntimeFromAuthored(TopdownData& topdown)
{
    topdown.runtime = {};
    topdown.runtime.levelActive = true;

    topdown.runtime.nav.levelBoundary = topdown.authored.levelBoundary;
    topdown.runtime.nav.agentRadius = topdown.runtime.player.radius;
    topdown.runtime.nav.valid = false;

    topdown.runtime.nav.navMesh = {};
    topdown.runtime.nav.navMesh.sourcePolygons.clear();
    topdown.runtime.nav.navMesh.blockerPolygons.clear();

    if (!topdown.authored.levelBoundary.empty()) {
        NavPolygon boundaryPoly;
        boundaryPoly.vertices = topdown.authored.levelBoundary;
        topdown.runtime.nav.navMesh.sourcePolygons.push_back(boundaryPoly);
    }

    topdown.runtime.nav.navMesh = {};

    if (!topdown.authored.levelBoundary.empty()) {
        NavPolygon sourcePoly;
        sourcePoly.vertices = topdown.authored.levelBoundary;
        topdown.runtime.nav.navMesh.sourcePolygons.push_back(sourcePoly);
    }

    topdown.runtime.collision.boundarySegments =
            TopdownBuildSegmentsFromPolygon(topdown.authored.levelBoundary);

    for (const TopdownAuthoredPolygon& authored : topdown.authored.obstacles) {
        TopdownRuntimeObstacle runtime;
        runtime.handle = topdown.runtime.collision.nextObstacleHandle++;
        runtime.tiledObjectId = authored.tiledObjectId;
        runtime.kind = authored.kind;
        runtime.name = authored.name;
        runtime.polygon = authored.points;
        runtime.edges = TopdownBuildSegmentsFromPolygon(runtime.polygon);
        runtime.bounds = TopdownComputePolygonBounds(runtime.polygon);
        runtime.visible = authored.visible;

        const int obstacleIndex = static_cast<int>(topdown.runtime.collision.obstacles.size());
        topdown.runtime.collision.obstacles.push_back(runtime);

        topdown.runtime.nav.holePolygons.push_back(authored.points);

        NavPolygon blockerPoly;
        blockerPoly.vertices = authored.points;
        topdown.runtime.nav.navMesh.blockerPolygons.push_back(blockerPoly);

        for (const TopdownSegment& seg : runtime.edges) {
            topdown.runtime.collision.movementSegments.push_back(seg);

            if (runtime.kind == TopdownObstacleKind::MovementAndVision) {
                topdown.runtime.collision.visionSegments.push_back(seg);
            }
        }

        (void)obstacleIndex;
    }

    for (const TopdownAuthoredDoor& authored : topdown.authored.doors) {
        topdown.runtime.doors.push_back(BuildRuntimeDoorFromAuthored(authored));
    }

    for (const TopdownAuthoredWindow& authored : topdown.authored.windows) {
        TopdownRuntimeWindow runtimeWindow =
                TopdownBuildRuntimeWindowFromAuthored(authored);

        if (!TopdownGenerateWindowTextureAtlas(
                runtimeWindow,
                topdown.currentLevelBaseAssetScale > 0
                ? topdown.currentLevelBaseAssetScale
                : topdown.authored.baseAssetScale)) {
            TraceLog(LOG_WARNING,
                     "Failed generating texture atlas for topdown window '%s'",
                     authored.id.c_str());
        }

        topdown.runtime.windows.push_back(runtimeWindow);

        TopdownRuntimeObstacle runtimeObstacle;
        runtimeObstacle.handle = topdown.runtime.collision.nextObstacleHandle++;
        runtimeObstacle.tiledObjectId = authored.tiledObjectId;
        runtimeObstacle.kind = TopdownObstacleKind::MovementOnly;
        runtimeObstacle.name = authored.id;
        runtimeObstacle.polygon = runtimeWindow.polygon;
        runtimeObstacle.edges = runtimeWindow.edges;
        runtimeObstacle.bounds = TopdownComputePolygonBounds(runtimeWindow.polygon);
        runtimeObstacle.visible = authored.visible;

        topdown.runtime.collision.obstacles.push_back(runtimeObstacle);

        topdown.runtime.nav.holePolygons.push_back(runtimeWindow.polygon);

        NavPolygon blockerPoly;
        blockerPoly.vertices = runtimeWindow.polygon;
        topdown.runtime.nav.navMesh.blockerPolygons.push_back(blockerPoly);

        for (const TopdownSegment& seg : runtimeWindow.edges) {
            topdown.runtime.collision.movementSegments.push_back(seg);
        }
    }

    if (!topdown.runtime.nav.navMesh.sourcePolygons.empty()) {
        if (!BuildNavMesh(topdown.runtime.nav.navMesh, topdown.runtime.nav.agentRadius)) {
            TraceLog(LOG_WARNING,
                     "Failed building topdown navmesh with agent radius %.2f",
                     topdown.runtime.nav.agentRadius);
        }
    }
    topdown.runtime.nav.valid = topdown.runtime.nav.navMesh.built;

    for (int i = 0; i < static_cast<int>(topdown.authored.imageLayers.size()); ++i) {
        const TopdownAuthoredImageLayer& authored = topdown.authored.imageLayers[i];

        TopdownRuntimeImageLayer runtime;
        runtime.handle = topdown.runtime.render.nextImageLayerHandle++;
        runtime.authoredIndex = i;
        runtime.kind = authored.kind;
        runtime.textureHandle = authored.textureHandle;
        runtime.position = authored.position;
        runtime.imageSize = authored.imageSize;
        runtime.scale = authored.scale;
        runtime.opacity = authored.opacity;
        runtime.visible = authored.visible;
        runtime.tint = authored.tint;
        runtime.blendMode = authored.blendMode;
        runtime.shaderType = authored.shaderType;
        runtime.shaderParams = authored.shaderParams;

        if (runtime.kind == TopdownImageLayerKind::Bottom) {
            topdown.runtime.render.bottomLayers.push_back(runtime);
        } else {
            topdown.runtime.render.topLayers.push_back(runtime);
        }
    }

    for (int i = 0; i < static_cast<int>(topdown.authored.effectRegions.size()); ++i) {
        const TopdownAuthoredEffectRegion& authored = topdown.authored.effectRegions[i];

        TopdownRuntimeEffectRegion runtime;
        runtime.handle = topdown.runtime.render.nextEffectRegionHandle++;
        runtime.authoredIndex = i;
        runtime.visible = authored.visible;
        runtime.opacity = authored.opacity;
        runtime.tint = authored.tint;
        runtime.shaderType = authored.shaderType;
        runtime.shaderParams = authored.shaderParams;

        topdown.runtime.render.effectRegions.push_back(runtime);
    }

    TopdownRebuildWallOcclusionPolygons(topdown, true);

    BuildSortedEffectRegionBuckets(topdown);

    const TopdownAuthoredSpawn* spawn = nullptr;

    if (!topdown.pendingSpawnId.empty()) {
        spawn = FindSpawnById(topdown, topdown.pendingSpawnId.c_str());

        if (spawn == nullptr) {
            TraceLog(LOG_WARNING,
                     "Requested topdown spawn '%s' not found in level '%s'",
                     topdown.pendingSpawnId.c_str(),
                     topdown.authored.levelId.c_str());
        }
    }

    if (spawn == nullptr) {
        spawn = FindSpawnById(topdown, "default");
    }

    if (spawn != nullptr) {
        topdown.runtime.player.position = spawn->position;

        const float radians = spawn->orientationDegrees * DEG2RAD;
        topdown.runtime.player.facing.x = std::cos(radians);
        topdown.runtime.player.facing.y = std::sin(radians);
    } else if (!topdown.authored.levelBoundary.empty()) {
        topdown.runtime.player.position = topdown.authored.levelBoundary[0];
    }
}

bool TopdownLoadLevel(GameState& state, const char* tiledFilePath, int baseAssetScale)
{
    // If a level is already active, run exit hook + kill VM
    if (state.topdown.runtime.levelActive) {
        TopdownRunLevelExitHook(state);
        ScriptSystemShutdown(state.script);
    }

    TopdownUnloadLevel(state);

    if (tiledFilePath == nullptr || tiledFilePath[0] == '\0') {
        TraceLog(LOG_ERROR, "TopdownLoadLevel called with empty tiledFilePath");
        return false;
    }

    const fs::path tmjPath = fs::path(tiledFilePath).lexically_normal();
    const std::string tmjNorm = NormalizePath(tmjPath);
    const fs::path sceneDir = tmjPath.parent_path();

    json root;
    {
        std::ifstream in(tmjNorm);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open topdown level: %s", tmjNorm.c_str());
            return false;
        }
        in >> root;
    }

    state.topdown.authored = {};
    state.topdown.runtime = {};

    state.topdown.authored.tiledFilePath = tmjNorm;
    state.topdown.authored.levelId = tmjPath.stem().string();
    state.topdown.authored.saveName = state.topdown.authored.levelId;
    state.topdown.authored.baseAssetScale = baseAssetScale;

    state.topdown.currentLevelId = state.topdown.authored.levelId;
    state.topdown.currentLevelSaveName = state.topdown.authored.saveName;
    state.topdown.currentLevelTiledFilePath = tmjNorm;
    state.topdown.currentLevelBaseAssetScale = baseAssetScale;

    {
        const fs::path tmjPathObj = fs::path(state.topdown.authored.tiledFilePath);
        const fs::path levelDir = tmjPathObj.parent_path();
        const fs::path scriptPath = levelDir / (state.topdown.authored.levelId + ".lua");
        state.topdown.currentLevelScriptFilePath = NormalizePath(scriptPath);
    }

    const TopdownLevelRegistryEntry* reg = nullptr;
    for (const TopdownLevelRegistryEntry& entry : state.topdown.levelRegistry) {
        if (entry.tiledFilePath == tmjNorm) {
            reg = &entry;
            break;
        }
    }

    if (reg != nullptr) {
        state.topdown.authored.levelId = reg->levelId;
        state.topdown.authored.saveName = reg->saveName;
        state.topdown.authored.baseAssetScale = reg->baseAssetScale;

        state.topdown.currentLevelId = reg->levelId;
        state.topdown.currentLevelSaveName = reg->saveName;
        state.topdown.currentLevelBaseAssetScale = reg->baseAssetScale;
    }

    bool foundBoundary = false;

    if (!root.contains("layers") || !root["layers"].is_array()) {
        TraceLog(LOG_ERROR, "Topdown level missing layers array: %s", tmjNorm.c_str());
        return false;
    }

    for (const auto& layer : root["layers"]) {
        if (!layer.is_object()) {
            continue;
        }

        const std::string layerName = layer.value("name", std::string());
        const std::string layerType = layer.value("type", std::string());

        if (layerName == "LevelBoundary" && layerType == "objectgroup") {
            if (!ImportBoundaryLayer(state.topdown, layer, state.topdown.authored.baseAssetScale)) {
                return false;
            }
            foundBoundary = true;
            continue;
        }

        if (layerName == "Blockers" && layerType == "objectgroup") {
            ImportObstacleLayer(state.topdown, layer, state.topdown.authored.baseAssetScale, TopdownObstacleKind::MovementAndVision);
            continue;
        }

        if (layerName == "MovementBlockers" && layerType == "objectgroup") {
            ImportObstacleLayer(state.topdown, layer, state.topdown.authored.baseAssetScale, TopdownObstacleKind::MovementOnly);
            continue;
        }

        if (layerName == "Spawns" && layerType == "objectgroup") {
            ImportSpawnLayer(state.topdown, layer, state.topdown.authored.baseAssetScale);
            continue;
        }

        if (layerName == "Bottom" && layerType == "group") {
            ImportImageGroup(state, layer, sceneDir, state.topdown.authored.baseAssetScale, TopdownImageLayerKind::Bottom);
            continue;
        }

        if (layerName == "Top" && layerType == "group") {
            ImportImageGroup(state, layer, sceneDir, state.topdown.authored.baseAssetScale, TopdownImageLayerKind::Top);
            continue;
        }

        if (layerName == "EffectRegions" && layerType == "objectgroup") {
            ImportEffectRegionLayer(state, layer, sceneDir, state.topdown.authored.baseAssetScale);
            continue;
        }

        if (layerName == "Npcs" && layerType == "objectgroup") {
            ImportNpcLayer(state.topdown, layer, state.topdown.authored.baseAssetScale);
            continue;
        }

        if (layerName == "Doors" && layerType == "objectgroup") {
            ImportDoorLayer(state.topdown, layer, state.topdown.authored.baseAssetScale);
            continue;
        }

        if (layerName == "Windows" && layerType == "objectgroup") {
            ImportWindowLayer(state.topdown, layer, state.topdown.authored.baseAssetScale);
            continue;
        }
    }

    if (!foundBoundary) {
        TraceLog(LOG_ERROR, "Topdown level missing LevelBoundary layer: %s", tmjNorm.c_str());
        TopdownUnloadLevel(state);
        return false;
    }

    state.topdown.authored.loaded = true;

    BuildRuntimeFromAuthored(state.topdown);

    InitializeTopdownPlayerCharacterRuntime(state);
    state.topdown.runtime.aiFrozen = false;
    state.topdown.runtime.godMode = false;
    state.topdown.runtime.gameOverActive = false;
    state.topdown.runtime.gameOverElapsedMs = 0.0f;
    state.topdown.runtime.returnToMenuRequested = false;

    BuildRuntimeNpcsFromAuthored(state);

    TopdownRvoInit(state);
    TopdownRvoRequestRebuild(state);
    TopdownRvoEnsureReady(state);

    TopdownInitCamera(state);

    // Fresh scripting VM per level
    ScriptSystemInit(state);

    if (!TopdownLoadLevelScript(state)) {
        TraceLog(LOG_ERROR, "Failed loading topdown level script");
        TopdownUnloadLevel(state);
        return false;
    }

    TopdownRunLevelEnterHook(state);

    TraceLog(LOG_INFO, "Loaded topdown level: %s", tmjNorm.c_str());
    TraceLog(LOG_INFO, "  levelId: %s", state.topdown.currentLevelId.c_str());
    TraceLog(LOG_INFO, "  saveName: %s", state.topdown.currentLevelSaveName.c_str());
    TraceLog(LOG_INFO, "  baseAssetScale: %d", state.topdown.currentLevelBaseAssetScale);
    TraceLog(LOG_INFO, "  obstacles: %d", static_cast<int>(state.topdown.authored.obstacles.size()));
    TraceLog(LOG_INFO, "  image layers: %d", static_cast<int>(state.topdown.authored.imageLayers.size()));
    TraceLog(LOG_INFO, "  spawns: %d", static_cast<int>(state.topdown.authored.spawns.size()));
    TraceLog(LOG_INFO, "  movement segments: %d", static_cast<int>(state.topdown.runtime.collision.movementSegments.size()));
    TraceLog(LOG_INFO, "  vision segments: %d", static_cast<int>(state.topdown.runtime.collision.visionSegments.size()));
    TraceLog(LOG_INFO, "  effect regions: %d", static_cast<int>(state.topdown.authored.effectRegions.size()));
    TraceLog(LOG_INFO, "  effect buckets: after_bottom=%d after_characters=%d final=%d",
             static_cast<int>(state.topdown.runtime.render.afterBottomEffectRegionIndices.size()),
             static_cast<int>(state.topdown.runtime.render.afterCharactersEffectRegionIndices.size()),
             static_cast<int>(state.topdown.runtime.render.finalEffectRegionIndices.size()));
    TraceLog(LOG_INFO, "  navmesh built: %s", state.topdown.runtime.nav.navMesh.built ? "yes" : "no");
    TraceLog(LOG_INFO, "  nav agent radius: %.2f", state.topdown.runtime.nav.agentRadius);
    TraceLog(LOG_INFO, "  nav vertices: %d", static_cast<int>(state.topdown.runtime.nav.navMesh.vertices.size()));
    TraceLog(LOG_INFO, "  nav triangles: %d", static_cast<int>(state.topdown.runtime.nav.navMesh.triangles.size()));
    TraceLog(LOG_INFO, "  authored npcs: %d", static_cast<int>(state.topdown.authored.npcs.size()));
    TraceLog(LOG_INFO, "  runtime npcs: %d", static_cast<int>(state.topdown.runtime.npcs.size()));
    TraceLog(LOG_INFO, "  authored doors: %d", static_cast<int>(state.topdown.authored.doors.size()));
    TraceLog(LOG_INFO, "  runtime doors: %d", static_cast<int>(state.topdown.runtime.doors.size()));
    TraceLog(LOG_INFO, "  authored windows: %d", static_cast<int>(state.topdown.authored.windows.size()));
    TraceLog(LOG_INFO, "  runtime windows: %d", static_cast<int>(state.topdown.runtime.windows.size()));

    return true;
}

void TopdownUnloadLevel(GameState& state)
{
    UnloadTopdownBloodRenderTarget(state);
    TopdownUnloadWindowResources(state.topdown);
    UnloadSceneResources(state.resources);
    TopdownRvoShutdown(state);
    state.topdown.authored = {};
    state.topdown.runtime = {};
    state.topdown.npcAssets.clear();
}
