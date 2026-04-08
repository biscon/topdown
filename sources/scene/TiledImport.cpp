#include "TiledImport.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <cctype>

#include "utils/json.hpp"
#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static float GetFloatOrDefault(const json& j, const char* key, float defaultValue)
{
    auto it = j.find(key);
    if (it == j.end()) {
        return defaultValue;
    }

    if (it->is_number_float() || it->is_number_integer()) {
        return it->get<float>();
    }

    return defaultValue;
}

static std::string NormalizePath(const fs::path& p)
{
    return p.lexically_normal().string();
}

static const json* FindProperty(const json& obj, const char* name)
{
    auto it = obj.find("properties");
    if (it == obj.end() || !it->is_array()) {
        return nullptr;
    }

    for (const auto& prop : *it) {
        if (prop.value("name", "") == name) {
            return &prop;
        }
    }

    return nullptr;
}

static std::string GetStringPropertyOrDefault(const json& obj, const char* name, const char* defaultValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", std::string(defaultValue));
}

static bool GetFloatProperty(const json& obj, const char* name, float& outValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return false;
    }

    if (prop->contains("value")) {
        const auto& v = (*prop)["value"];
        if (v.is_number_float() || v.is_number_integer()) {
            outValue = v.get<float>();
            return true;
        }
    }

    return false;
}

static bool GetBoolProperty(const json& obj, const char* name, bool& outValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return false;
    }

    if (prop->contains("value")) {
        const auto& v = (*prop)["value"];
        if (v.is_boolean()) {
            outValue = v.get<bool>();
            return true;
        }
    }

    return false;
}

static bool GetIntProperty(const json& obj, const char* name, int& outValue)
{
    const json* prop = FindProperty(obj, name);
    if (prop == nullptr) {
        return false;
    }

    if (prop->contains("value")) {
        const auto& v = (*prop)["value"];
        if (v.is_number_integer()) {
            outValue = v.get<int>();
            return true;
        }

        if (v.is_number_float()) {
            outValue = static_cast<int>(v.get<float>());
            return true;
        }
    }

    return false;
}

static bool ParseScenePropVisualType(const std::string& s, ScenePropVisualType& outType)
{
    if (s == "sprite") {
        outType = ScenePropVisualType::Sprite;
        return true;
    }
    if (s == "image") {
        outType = ScenePropVisualType::Image;
        return true;
    }
    return false;
}

static bool ParseScenePropDepthMode(const std::string& s, ScenePropDepthMode& outMode)
{
    if (s == "depthSorted") {
        outMode = ScenePropDepthMode::DepthSorted;
        return true;
    }
    if (s == "back") {
        outMode = ScenePropDepthMode::Back;
        return true;
    }
    if (s == "front") {
        outMode = ScenePropDepthMode::Front;
        return true;
    }
    return false;
}

static bool ParseEffectBlendMode(const std::string& s, EffectBlendMode& outMode)
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

static bool ParseEffectShaderType(const std::string& s, EffectShaderType& outType)
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

static const char* EffectShaderTypeToString(EffectShaderType type)
{
    switch (type) {
        case EffectShaderType::UvScroll:
            return "uv_scroll";
        case EffectShaderType::HeatShimmer:
            return "heat_shimmer";
        case EffectShaderType::RegionGrade:
            return "region_grade";
        case EffectShaderType::WaterRipple:
            return "water_ripple";
        case EffectShaderType::WindSway:
            return "wind_sway";
        case EffectShaderType::PolyClip:
            return "poly_clip";
        case EffectShaderType::None:
        default:
            return "none";
    }
}

static EffectShaderCategory GetEffectShaderCategory(EffectShaderType type)
{
    switch (type) {
        case EffectShaderType::UvScroll:
        case EffectShaderType::WindSway:
        case EffectShaderType::PolyClip:
            return EffectShaderCategory::SelfTexture;
        case EffectShaderType::HeatShimmer:
        case EffectShaderType::RegionGrade:
            return EffectShaderCategory::SceneSample;
        case EffectShaderType::WaterRipple:
            return EffectShaderCategory::SceneSample;
        case EffectShaderType::None:
        default:
            return EffectShaderCategory::None;
    }
}

static bool TryGetEffectGroupInfoFromGroupName(const std::string& groupName,
                                               ScenePropDepthMode& outMode,
                                               bool& outRenderAsOverlay)
{
    outRenderAsOverlay = false;

    if (groupName == "effects_back") {
        outMode = ScenePropDepthMode::Back;
        return true;
    }
    if (groupName == "effects_sorted") {
        outMode = ScenePropDepthMode::DepthSorted;
        return true;
    }
    if (groupName == "effects_front") {
        outMode = ScenePropDepthMode::Front;
        outRenderAsOverlay = false;
        return true;
    }
    if (groupName == "effects_overlay") {
        outMode = ScenePropDepthMode::Front;
        outRenderAsOverlay = true;
        return true;
    }
    return false;
}

static int HexNibbleToInt(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool ParseHexByte(const std::string& s, size_t index, unsigned char& outValue)
{
    if (index + 1 >= s.size()) {
        return false;
    }

    const int hi = HexNibbleToInt(s[index]);
    const int lo = HexNibbleToInt(s[index + 1]);
    if (hi < 0 || lo < 0) {
        return false;
    }

    outValue = static_cast<unsigned char>((hi << 4) | lo);
    return true;
}

static bool ParseTiledTintColor(const std::string& s, Color& outColor)
{
    if (s.empty()) {
        outColor = WHITE;
        return true;
    }

    if (s.size() == 7 && s[0] == '#') {
        unsigned char r = 255;
        unsigned char g = 255;
        unsigned char b = 255;
        if (!ParseHexByte(s, 1, r) ||
            !ParseHexByte(s, 3, g) ||
            !ParseHexByte(s, 5, b)) {
            return false;
        }

        outColor = Color{r, g, b, 255};
        return true;
    }

    if (s.size() == 9 && s[0] == '#') {
        unsigned char a = 255;
        unsigned char r = 255;
        unsigned char g = 255;
        unsigned char b = 255;
        if (!ParseHexByte(s, 1, a) ||
            !ParseHexByte(s, 3, r) ||
            !ParseHexByte(s, 5, g) ||
            !ParseHexByte(s, 7, b)) {
            return false;
        }

        outColor = Color{r, g, b, a};
        return true;
    }

    return false;
}

static bool ParseFacing(const std::string& s, SceneFacing& outFacing)
{
    if (s == "left") {
        outFacing = SceneFacing::Left;
        return true;
    }
    if (s == "right") {
        outFacing = SceneFacing::Right;
        return true;
    }
    if (s == "front") {
        outFacing = SceneFacing::Front;
        return true;
    }
    if (s == "back") {
        outFacing = SceneFacing::Back;
        return true;
    }
    return false;
}

static bool LoadFacingProperty(const json& obj, SceneFacing& outFacing)
{
    const std::string facingStr = GetStringPropertyOrDefault(obj, "facing", "");
    if (facingStr.empty()) {
        return false;
    }
    return ParseFacing(facingStr, outFacing);
}

static ScenePolygon BuildWorldPolygon(
        const json& obj,
        float totalOffsetX,
        float totalOffsetY,
        int baseAssetScale)
{
    ScenePolygon poly;

    const float objX = GetFloatOrDefault(obj, "x", 0.0f);
    const float objY = GetFloatOrDefault(obj, "y", 0.0f);

    if (!obj.contains("polygon") || !obj["polygon"].is_array()) {
        return poly;
    }

    for (const auto& pt : obj["polygon"]) {
        Vector2 v{};
        v.x = (totalOffsetX + objX + pt.value("x", 0.0f)) * static_cast<float>(baseAssetScale);
        v.y = (totalOffsetY + objY + pt.value("y", 0.0f)) * static_cast<float>(baseAssetScale);
        poly.vertices.push_back(v);
    }

    return poly;
}

static void ProcessLayerRecursive(
        const json& layer,
        const fs::path& tiledDir,
        SceneData& scene,
        ResourceData& resources,
        const std::string& currentGroup,
        float parentOffsetX,
        float parentOffsetY,
        float parentParallaxX,
        float parentParallaxY)
{
    const std::string type = layer.value("type", "");
    const std::string name = layer.value("name", "");

    const float layerX = GetFloatOrDefault(layer, "x", 0.0f);
    const float layerY = GetFloatOrDefault(layer, "y", 0.0f);
    const float offsetX = GetFloatOrDefault(layer, "offsetx", 0.0f);
    const float offsetY = GetFloatOrDefault(layer, "offsety", 0.0f);

    const float totalOffsetX = parentOffsetX + layerX + offsetX;
    const float totalOffsetY = parentOffsetY + layerY + offsetY;

    const float parallaxX = parentParallaxX * GetFloatOrDefault(layer, "parallaxx", 1.0f);
    const float parallaxY = parentParallaxY * GetFloatOrDefault(layer, "parallaxy", 1.0f);

    if (type == "group") {
        const std::string nextGroup = name.empty() ? currentGroup : name;

        if (layer.contains("layers") && layer["layers"].is_array()) {
            for (const auto& child : layer["layers"]) {
                ProcessLayerRecursive(
                        child,
                        tiledDir,
                        scene,
                        resources,
                        nextGroup,
                        totalOffsetX,
                        totalOffsetY,
                        parallaxX,
                        parallaxY);
            }
        }
        return;
    }

    if (type == "imagelayer" && (currentGroup == "background" || currentGroup == "foreground")) {
        SceneImageLayer img;
        img.name = name;
        img.visible = layer.value("visible", true);
        img.opacity = GetFloatOrDefault(layer, "opacity", 1.0f);
        img.parallaxX = parallaxX;
        img.parallaxY = parallaxY;

        const std::string imageRel = layer.value("image", "");
        if (imageRel.empty()) {
            TraceLog(LOG_WARNING, "Image layer without image in group %s", currentGroup.c_str());
            return;
        }

        const fs::path imagePath = (tiledDir / imageRel).lexically_normal();
        img.imagePath = NormalizePath(imagePath);
        img.textureHandle = LoadTextureAsset(resources, img.imagePath.c_str());

        img.sourceSize.x = GetFloatOrDefault(layer, "imagewidth", 0.0f);
        img.sourceSize.y = GetFloatOrDefault(layer, "imageheight", 0.0f);

        img.worldPos.x = totalOffsetX * static_cast<float>(scene.baseAssetScale);
        img.worldPos.y = totalOffsetY * static_cast<float>(scene.baseAssetScale);

        img.worldSize.x = img.sourceSize.x * static_cast<float>(scene.baseAssetScale);
        img.worldSize.y = img.sourceSize.y * static_cast<float>(scene.baseAssetScale);

        if (currentGroup == "background") {
            scene.backgroundLayers.push_back(img);
        } else {
            scene.foregroundLayers.push_back(img);
        }
        return;
    }

    ScenePropDepthMode effectDepthMode = ScenePropDepthMode::DepthSorted;
    bool effectRenderAsOverlay = false;
    if (type == "imagelayer" &&
        TryGetEffectGroupInfoFromGroupName(currentGroup, effectDepthMode, effectRenderAsOverlay)) {
        SceneEffectSpriteData effect;
        effect.id = name;
        if (effect.id.empty()) {
            TraceLog(LOG_ERROR, "Effect image layer missing name in group %s", currentGroup.c_str());
            return;
        }

        const std::string imageRel = layer.value("image", "");
        if (imageRel.empty()) {
            TraceLog(LOG_ERROR, "Effect image layer missing image: %s", effect.id.c_str());
            return;
        }

        effect.visible = layer.value("visible", true);
        effect.opacity = GetFloatOrDefault(layer, "opacity", 1.0f);
        effect.depthMode = effectDepthMode;
        effect.renderAsOverlay = effectRenderAsOverlay;

        const std::string modeStr = layer.value("mode", "");
        if (!ParseEffectBlendMode(modeStr, effect.blendMode)) {
            TraceLog(LOG_WARNING,
                     "Unsupported effect blend mode '%s' on effect '%s', falling back to normal",
                     modeStr.c_str(),
                     effect.id.c_str());
            effect.blendMode = EffectBlendMode::Normal;
        }

        const std::string tintStr = layer.value("tintcolor", "");
        if (!ParseTiledTintColor(tintStr, effect.tint)) {
            TraceLog(LOG_WARNING,
                     "Invalid tintcolor '%s' on effect '%s', falling back to white",
                     tintStr.c_str(),
                     effect.id.c_str());
            effect.tint = WHITE;
        }

        effect.shaderIdString = GetStringPropertyOrDefault(layer, "shaderId", "");
        if (!ParseEffectShaderType(effect.shaderIdString, effect.shaderType)) {
            TraceLog(LOG_WARNING,
                     "Unsupported effect shaderId '%s' on effect '%s', falling back to none",
                     effect.shaderIdString.c_str(),
                     effect.id.c_str());
            effect.shaderType = EffectShaderType::None;
            effect.shaderIdString.clear();
        }

        TextureLoadSettings textureSettings{};
        if (effect.shaderType == EffectShaderType::UvScroll ||
            effect.shaderType == EffectShaderType::WindSway) {
            textureSettings.filter = TextureFilterMode::Bilinear;
            textureSettings.wrap = TextureWrapMode::Repeat;
        } else if (effect.shaderType == EffectShaderType::PolyClip) {
            textureSettings.filter = TextureFilterMode::Bilinear;
            textureSettings.wrap = TextureWrapMode::Clamp;
        }

        const fs::path imagePath = (tiledDir / imageRel).lexically_normal();
        effect.imagePath = NormalizePath(imagePath);
        effect.textureHandle = LoadTextureAsset(resources, effect.imagePath.c_str(), textureSettings);
        if (effect.textureHandle < 0) {
            TraceLog(LOG_ERROR,
                     "Failed loading effect image for '%s': %s",
                     effect.id.c_str(),
                     effect.imagePath.c_str());
            return;
        }

        effect.sourceSize.x = GetFloatOrDefault(layer, "imagewidth", 0.0f);
        effect.sourceSize.y = GetFloatOrDefault(layer, "imageheight", 0.0f);

        effect.worldPos.x = totalOffsetX * static_cast<float>(scene.baseAssetScale);
        effect.worldPos.y = totalOffsetY * static_cast<float>(scene.baseAssetScale);

        effect.worldSize.x = effect.sourceSize.x * static_cast<float>(scene.baseAssetScale);
        effect.worldSize.y = effect.sourceSize.y * static_cast<float>(scene.baseAssetScale);

        GetFloatProperty(layer, "scrollSpeedX", effect.shaderParams.scrollSpeed.x);
        GetFloatProperty(layer, "scrollSpeedY", effect.shaderParams.scrollSpeed.y);
        GetFloatProperty(layer, "uvScaleX", effect.shaderParams.uvScale.x);
        GetFloatProperty(layer, "uvScaleY", effect.shaderParams.uvScale.y);
        GetFloatProperty(layer, "distortionX", effect.shaderParams.distortionAmount.x);
        GetFloatProperty(layer, "distortionY", effect.shaderParams.distortionAmount.y);
        GetFloatProperty(layer, "noiseSpeedX", effect.shaderParams.noiseScrollSpeed.x);
        GetFloatProperty(layer, "noiseSpeedY", effect.shaderParams.noiseScrollSpeed.y);
        GetFloatProperty(layer, "intensity", effect.shaderParams.intensity);
        GetFloatProperty(layer, "phaseOffset", effect.shaderParams.phaseOffset);
        GetFloatProperty(layer, "brightness", effect.shaderParams.brightness);
        GetFloatProperty(layer, "contrast", effect.shaderParams.contrast);
        GetFloatProperty(layer, "saturation", effect.shaderParams.saturation);
        GetFloatProperty(layer, "tintR", effect.shaderParams.tintR);
        GetFloatProperty(layer, "tintG", effect.shaderParams.tintG);
        GetFloatProperty(layer, "tintB", effect.shaderParams.tintB);
        GetFloatProperty(layer, "softness", effect.shaderParams.softness);

        scene.effectSprites.push_back(effect);
        return;
    }

    if (type == "objectgroup" && currentGroup == "navigation" && name == "navmesh") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            NavPolygon poly;
            const ScenePolygon worldPoly = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            poly.vertices = worldPoly.vertices;

            if (poly.vertices.size() >= 3) {
                scene.navMesh.sourcePolygons.push_back(poly);
            }
        }
        return;
    }

    if (type == "objectgroup" && currentGroup == "navigation" && name == "blockers") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            NavPolygon poly;
            const ScenePolygon worldPoly = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            poly.vertices = worldPoly.vertices;

            if (poly.vertices.size() >= 3) {
                scene.navMesh.blockerPolygons.push_back(poly);
            }
        }
        return;
    }

    if (type == "objectgroup" && name == "spawns") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true) || !obj.value("point", false)) {
                continue;
            }

            SceneSpawnPoint spawn;
            spawn.id = obj.value("name", "");
            if (spawn.id.empty()) {
                TraceLog(LOG_ERROR, "Spawn missing name");
                continue;
            }

            if (!LoadFacingProperty(obj, spawn.facing)) {
                TraceLog(LOG_ERROR, "Spawn missing/invalid facing: %s", spawn.id.c_str());
                continue;
            }

            spawn.position.x = (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) * static_cast<float>(scene.baseAssetScale);
            spawn.position.y = (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) * static_cast<float>(scene.baseAssetScale);

            scene.spawns.push_back(spawn);
        }
        return;
    }

    if (type == "objectgroup" && name == "hotspots") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            SceneHotspot hotspot;
            hotspot.id = obj.value("name", "");
            if (hotspot.id.empty()) {
                TraceLog(LOG_ERROR, "Hotspot missing name");
                continue;
            }

            hotspot.displayName = GetStringPropertyOrDefault(obj, "displayName", "");
            hotspot.lookText = GetStringPropertyOrDefault(obj, "lookText", "");

            float walkToX = 0.0f;
            float walkToY = 0.0f;
            const bool hasWalkToX = GetFloatProperty(obj, "walkToX", walkToX);
            const bool hasWalkToY = GetFloatProperty(obj, "walkToY", walkToY);

            if (hotspot.displayName.empty() || hotspot.lookText.empty() ||
                !hasWalkToX || !hasWalkToY || !LoadFacingProperty(obj, hotspot.facing)) {
                TraceLog(LOG_ERROR, "Hotspot missing required properties: %s", hotspot.id.c_str());
                continue;
            }

            hotspot.walkTo.x = walkToX * static_cast<float>(scene.baseAssetScale);
            hotspot.walkTo.y = walkToY * static_cast<float>(scene.baseAssetScale);

            hotspot.shape = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            if (hotspot.shape.vertices.size() < 3) {
                TraceLog(LOG_ERROR, "Hotspot polygon invalid: %s", hotspot.id.c_str());
                continue;
            }

            scene.hotspots.push_back(hotspot);
        }
        return;
    }

    if (type == "objectgroup" && name == "exits") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true)) {
                continue;
            }

            SceneExit exitObj;
            exitObj.id = obj.value("name", "");
            if (exitObj.id.empty()) {
                TraceLog(LOG_ERROR, "Exit missing name");
                continue;
            }

            exitObj.displayName = GetStringPropertyOrDefault(obj, "displayName", "");
            exitObj.lookText = GetStringPropertyOrDefault(obj, "lookText", "");
            exitObj.targetScene = GetStringPropertyOrDefault(obj, "targetScene", "");
            exitObj.targetSpawn = GetStringPropertyOrDefault(obj, "targetSpawn", "");

            float walkToX = 0.0f;
            float walkToY = 0.0f;
            const bool hasWalkToX = GetFloatProperty(obj, "walkToX", walkToX);
            const bool hasWalkToY = GetFloatProperty(obj, "walkToY", walkToY);

            if (exitObj.displayName.empty() || exitObj.lookText.empty() ||
                exitObj.targetScene.empty() || exitObj.targetSpawn.empty() ||
                !hasWalkToX || !hasWalkToY || !LoadFacingProperty(obj, exitObj.facing)) {
                TraceLog(LOG_ERROR, "Exit missing required properties: %s", exitObj.id.c_str());
                continue;
            }

            exitObj.walkTo.x = walkToX * static_cast<float>(scene.baseAssetScale);
            exitObj.walkTo.y = walkToY * static_cast<float>(scene.baseAssetScale);

            exitObj.shape = BuildWorldPolygon(obj, totalOffsetX, totalOffsetY, scene.baseAssetScale);
            if (exitObj.shape.vertices.size() < 3) {
                TraceLog(LOG_ERROR, "Exit polygon invalid: %s", exitObj.id.c_str());
                continue;
            }

            scene.exits.push_back(exitObj);
        }
        return;
    }

    if (type == "objectgroup" && name == "props") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("point", false)) {
                continue;
            }

            ScenePropData prop;
            prop.id = obj.value("name", "");
            if (prop.id.empty()) {
                TraceLog(LOG_ERROR, "Prop missing name");
                continue;
            }

            const std::string visualTypeStr = GetStringPropertyOrDefault(obj, "visualType", "");
            if (!ParseScenePropVisualType(visualTypeStr, prop.visualType)) {
                TraceLog(LOG_ERROR, "Prop missing/invalid visualType: %s", prop.id.c_str());
                continue;
            }

            const std::string depthModeStr = GetStringPropertyOrDefault(obj, "depthMode", "depthSorted");
            if (!ParseScenePropDepthMode(depthModeStr, prop.depthMode)) {
                TraceLog(LOG_ERROR, "Prop missing/invalid depthMode: %s", prop.id.c_str());
                continue;
            }

            const std::string assetRel = GetStringPropertyOrDefault(obj, "asset", "");
            if (assetRel.empty()) {
                TraceLog(LOG_ERROR, "Prop missing asset property: %s", prop.id.c_str());
                continue;
            }

            const fs::path assetPath = (tiledDir / assetRel).lexically_normal();

            if (prop.visualType == ScenePropVisualType::Sprite) {
                prop.spriteAssetHandle = LoadSpriteAsset(resources, assetPath.string().c_str());
                if (prop.spriteAssetHandle < 0) {
                    TraceLog(LOG_ERROR, "Failed loading sprite prop asset for %s: %s",
                             prop.id.c_str(), assetPath.string().c_str());
                    continue;
                }

                prop.defaultAnimation = GetStringPropertyOrDefault(obj, "animation", "");
                if (prop.defaultAnimation.empty()) {
                    TraceLog(LOG_ERROR, "Sprite prop missing animation property: %s", prop.id.c_str());
                    continue;
                }
            } else if (prop.visualType == ScenePropVisualType::Image) {
                prop.textureHandle = LoadTextureAsset(resources, assetPath.string().c_str());
                if (prop.textureHandle < 0) {
                    TraceLog(LOG_ERROR, "Failed loading image prop asset for %s: %s",
                             prop.id.c_str(), assetPath.string().c_str());
                    continue;
                }
            }

            bool flipX = false;
            if (GetBoolProperty(obj, "flipX", flipX)) {
                prop.flipX = flipX;
            }

            bool depthScaling = false;
            if (GetBoolProperty(obj, "depthScaling", depthScaling)) {
                prop.depthScaling = depthScaling;
            }

            prop.visible = obj.value("visible", true);

            prop.feetPos.x =
                    (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);
            prop.feetPos.y =
                    (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);

            scene.props.push_back(prop);
        }
        return;
    }

    if (type == "objectgroup" && name == "actors") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true) || !obj.value("point", false)) {
                continue;
            }

            SceneActorPlacement placement;
            placement.actorId = obj.value("name", "");
            if (placement.actorId.empty()) {
                TraceLog(LOG_ERROR, "Actor placement missing name");
                continue;
            }

            if (!LoadFacingProperty(obj, placement.facing)) {
                TraceLog(LOG_ERROR, "Actor placement missing/invalid facing: %s", placement.actorId.c_str());
                continue;
            }

            placement.visible = obj.value("visible", true);

            placement.position.x =
                    (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);
            placement.position.y =
                    (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);

            scene.actorPlacements.push_back(placement);
        }
        return;
    }


    if (type == "objectgroup" && name == "effect_regions") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            /* Do not skip loading of invisible ones, just set the flag and let the engine hide them
            if (!obj.value("visible", true)) {
                continue;
            }
            */

            SceneEffectRegionData effect;
            effect.id = obj.value("name", "");
            if (effect.id.empty()) {
                TraceLog(LOG_ERROR, "Effect region missing name");
                continue;
            }

            const bool hasPolygon =
                    obj.contains("polygon") &&
                    obj["polygon"].is_array() &&
                    !obj["polygon"].empty();

            if (hasPolygon) {
                effect.usePolygon = true;
                effect.polygon = BuildWorldPolygon(
                        obj,
                        totalOffsetX,
                        totalOffsetY,
                        scene.baseAssetScale);

                if (effect.polygon.vertices.size() < 3) {
                    TraceLog(LOG_ERROR,
                             "Effect region polygon invalid (need at least 3 vertices): %s",
                             effect.id.c_str());
                    continue;
                }

                float minX = effect.polygon.vertices[0].x;
                float minY = effect.polygon.vertices[0].y;
                float maxX = effect.polygon.vertices[0].x;
                float maxY = effect.polygon.vertices[0].y;

                for (const Vector2& v : effect.polygon.vertices) {
                    if (v.x < minX) minX = v.x;
                    if (v.y < minY) minY = v.y;
                    if (v.x > maxX) maxX = v.x;
                    if (v.y > maxY) maxY = v.y;
                }

                effect.worldRect.x = minX;
                effect.worldRect.y = minY;
                effect.worldRect.width = maxX - minX;
                effect.worldRect.height = maxY - minY;

                if (effect.worldRect.width <= 0.0f || effect.worldRect.height <= 0.0f) {
                    TraceLog(LOG_ERROR,
                             "Effect region polygon has invalid bounds: %s",
                             effect.id.c_str());
                    continue;
                }

                if (effect.polygon.vertices.size() > 32) {
                    TraceLog(LOG_ERROR,
                             "Effect region polygon has too many vertices (%d > 32): %s",
                             static_cast<int>(effect.polygon.vertices.size()),
                             effect.id.c_str());
                    continue;
                }
            } else {
                const float width = GetFloatOrDefault(obj, "width", 0.0f);
                const float height = GetFloatOrDefault(obj, "height", 0.0f);
                if (width <= 0.0f || height <= 0.0f) {
                    TraceLog(LOG_ERROR,
                             "Effect region must be a rectangle with non-zero size, or a polygon: %s",
                             effect.id.c_str());
                    continue;
                }

                effect.usePolygon = false;
                effect.worldRect.x =
                        (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) *
                        static_cast<float>(scene.baseAssetScale);
                effect.worldRect.y =
                        (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) *
                        static_cast<float>(scene.baseAssetScale);
                effect.worldRect.width = width * static_cast<float>(scene.baseAssetScale);
                effect.worldRect.height = height * static_cast<float>(scene.baseAssetScale);
            }

            const std::string blendModeStr = GetStringPropertyOrDefault(obj, "blendMode", "normal");
            if (!ParseEffectBlendMode(blendModeStr, effect.blendMode)) {
                TraceLog(LOG_WARNING,
                         "Unsupported blendMode '%s' on effect region '%s', falling back to normal",
                         blendModeStr.c_str(),
                         effect.id.c_str());
                effect.blendMode = EffectBlendMode::Normal;
            }

            const std::string depthModeStr = GetStringPropertyOrDefault(obj, "depthMode", "depthSorted");
            if (!ParseScenePropDepthMode(depthModeStr, effect.depthMode)) {
                TraceLog(LOG_WARNING,
                         "Unsupported depthMode '%s' on effect region '%s', falling back to depthSorted",
                         depthModeStr.c_str(),
                         effect.id.c_str());
                effect.depthMode = ScenePropDepthMode::DepthSorted;
            }

            bool renderAsOverlay = false;
            if (GetBoolProperty(obj, "overlay", renderAsOverlay)) {
                effect.renderAsOverlay = renderAsOverlay;
            }

            effect.opacity = GetFloatOrDefault(obj, "opacity", 1.0f);
            effect.visible = obj.value("visible", true);
            GetIntProperty(obj, "sortOrder", effect.sortOrder);

            effect.shaderIdString = GetStringPropertyOrDefault(obj, "shaderId", "");
            if (!ParseEffectShaderType(effect.shaderIdString, effect.shaderType)) {
                TraceLog(LOG_WARNING,
                         "Unsupported effect shaderId '%s' on effect region '%s', falling back to none",
                         effect.shaderIdString.c_str(),
                         effect.id.c_str());
                effect.shaderType = EffectShaderType::None;
                effect.shaderIdString.clear();
            }

            const EffectShaderCategory shaderCategory =
                    GetEffectShaderCategory(effect.shaderType);

            const std::string assetRel = GetStringPropertyOrDefault(obj, "asset", "");
            if (shaderCategory == EffectShaderCategory::SelfTexture && assetRel.empty()) {
                TraceLog(LOG_ERROR,
                         "Effect region missing asset property for self-texture shader: %s",
                         effect.id.c_str());
                continue;
            }

            TextureLoadSettings textureSettings{};

            if (effect.shaderType == EffectShaderType::UvScroll ||
                effect.shaderType == EffectShaderType::WindSway) {
                textureSettings.filter = TextureFilterMode::Bilinear;
                textureSettings.wrap = TextureWrapMode::Repeat;
            } else if (effect.shaderType == EffectShaderType::PolyClip) {
                textureSettings.filter = TextureFilterMode::Bilinear;
                textureSettings.wrap = TextureWrapMode::Clamp;
            }

            if (!assetRel.empty()) {
                const fs::path assetPath = (tiledDir / assetRel).lexically_normal();
                effect.imagePath = NormalizePath(assetPath);
                effect.textureHandle = LoadTextureAsset(resources, effect.imagePath.c_str(), textureSettings);
                if (effect.textureHandle < 0) {
                    TraceLog(LOG_ERROR,
                             "Failed loading effect region image for '%s': %s",
                             effect.id.c_str(),
                             effect.imagePath.c_str());
                    continue;
                }
            } else {
                effect.imagePath.clear();
                effect.textureHandle = -1;
            }

            GetFloatProperty(obj, "scrollSpeedX", effect.shaderParams.scrollSpeed.x);
            GetFloatProperty(obj, "scrollSpeedY", effect.shaderParams.scrollSpeed.y);
            GetFloatProperty(obj, "uvScaleX", effect.shaderParams.uvScale.x);
            GetFloatProperty(obj, "uvScaleY", effect.shaderParams.uvScale.y);
            GetFloatProperty(obj, "distortionX", effect.shaderParams.distortionAmount.x);
            GetFloatProperty(obj, "distortionY", effect.shaderParams.distortionAmount.y);
            GetFloatProperty(obj, "noiseSpeedX", effect.shaderParams.noiseScrollSpeed.x);
            GetFloatProperty(obj, "noiseSpeedY", effect.shaderParams.noiseScrollSpeed.y);
            GetFloatProperty(obj, "intensity", effect.shaderParams.intensity);
            GetFloatProperty(obj, "phaseOffset", effect.shaderParams.phaseOffset);
            GetFloatProperty(obj, "brightness", effect.shaderParams.brightness);
            GetFloatProperty(obj, "contrast", effect.shaderParams.contrast);
            GetFloatProperty(obj, "saturation", effect.shaderParams.saturation);
            GetFloatProperty(obj, "tintR", effect.shaderParams.tintR);
            GetFloatProperty(obj, "tintG", effect.shaderParams.tintG);
            GetFloatProperty(obj, "tintB", effect.shaderParams.tintB);
            GetFloatProperty(obj, "softness", effect.shaderParams.softness);

            scene.effectRegions.push_back(effect);
        }
        return;
    }


    if (type == "objectgroup" && name == "sound_emitters") {
        if (!layer.contains("objects") || !layer["objects"].is_array()) {
            return;
        }

        for (const auto& obj : layer["objects"]) {
            if (!obj.value("visible", true) || !obj.value("point", false)) {
                continue;
            }

            SceneSoundEmitterData emitter;
            emitter.id = obj.value("name", "");
            if (emitter.id.empty()) {
                TraceLog(LOG_ERROR, "Sound emitter missing name");
                continue;
            }

            emitter.soundId = GetStringPropertyOrDefault(obj, "sound", "");
            if (emitter.soundId.empty()) {
                TraceLog(LOG_ERROR, "Sound emitter missing sound property: %s", emitter.id.c_str());
                continue;
            }

            if (!GetFloatProperty(obj, "radius", emitter.radius)) {
                TraceLog(LOG_ERROR, "Sound emitter missing radius property: %s", emitter.id.c_str());
                continue;
            }

            float volume = 1.0f;
            if (GetFloatProperty(obj, "volume", volume)) {
                emitter.volume = volume;
            }

            bool enabled = true;
            if (GetBoolProperty(obj, "enabled", enabled)) {
                emitter.enabled = enabled;
            }

            bool pan = true;
            if (GetBoolProperty(obj, "pan", pan)) {
                emitter.pan = pan;
            }

            bool loop = true;
            if (GetBoolProperty(obj, "loop", loop)) {
                emitter.loop = loop;
            }

            emitter.position.x =
                    (totalOffsetX + GetFloatOrDefault(obj, "x", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);
            emitter.position.y =
                    (totalOffsetY + GetFloatOrDefault(obj, "y", 0.0f)) *
                    static_cast<float>(scene.baseAssetScale);

            scene.soundEmitters.push_back(emitter);
        }
        return;
    }
}

bool ImportTiledSceneIntoSceneData(SceneData& scene, ResourceData& resources, const char* tiledFilePath)
{
    scene.backgroundLayers.clear();
    scene.foregroundLayers.clear();
    scene.effectSprites.clear();
    scene.effectRegions.clear();
    scene.navMesh = {};
    scene.spawns.clear();
    scene.hotspots.clear();
    scene.exits.clear();
    scene.props.clear();
    scene.actorPlacements.clear();
    scene.soundEmitters.clear();

    const fs::path tiledPath = fs::path(tiledFilePath).lexically_normal();
    scene.tiledFilePath = tiledPath.string();

    json root;
    {
        std::ifstream in(tiledPath);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open Tiled file: %s", scene.tiledFilePath.c_str());
            return false;
        }
        in >> root;
    }

    if (!root.contains("layers") || !root["layers"].is_array()) {
        TraceLog(LOG_ERROR, "Tiled file missing layers array: %s", scene.tiledFilePath.c_str());
        return false;
    }

    const fs::path tiledDir = tiledPath.parent_path();

    for (const auto& layer : root["layers"]) {
        ProcessLayerRecursive(
                layer,
                tiledDir,
                scene,
                resources,
                "",
                0.0f,
                0.0f,
                1.0f,
                1.0f);
    }

    TraceLog(LOG_INFO,
             "Imported Tiled scene: %s (bg=%d fg=%d effects=%d effectRegions=%d navPolys=%d spawns=%d hotspots=%d exits=%d props=%d actors=%d emitters=%d)",
             scene.tiledFilePath.c_str(),
             static_cast<int>(scene.backgroundLayers.size()),
             static_cast<int>(scene.foregroundLayers.size()),
             static_cast<int>(scene.effectSprites.size()),
             static_cast<int>(scene.effectRegions.size()),
             static_cast<int>(scene.navMesh.sourcePolygons.size()),
             static_cast<int>(scene.spawns.size()),
             static_cast<int>(scene.hotspots.size()),
             static_cast<int>(scene.exits.size()),
             static_cast<int>(scene.props.size()),
             static_cast<int>(scene.actorPlacements.size()),
             static_cast<int>(scene.soundEmitters.size()));

    return true;
}
