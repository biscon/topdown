#pragma once

#include <string>

#include "raylib.h"
#include "utils/json.hpp"

const nlohmann::json* TopdownFindTiledObjectProperty(
        const nlohmann::json& objectJson,
        const char* name);
bool TopdownGetTiledObjectPropertyBool(
        const nlohmann::json& objectJson,
        const char* name,
        bool defaultValue);
float TopdownGetTiledObjectPropertyFloat(
        const nlohmann::json& objectJson,
        const char* name,
        float defaultValue);
std::string TopdownGetTiledObjectPropertyString(
        const nlohmann::json& objectJson,
        const char* name,
        const std::string& defaultValue = std::string());
bool TopdownParseTiledColorString(const std::string& text, Color& outColor);
Color TopdownGetTiledLayerTintColor(
        const nlohmann::json& layer,
        Color defaultValue);
Color TopdownGetTiledObjectPropertyColor(
        const nlohmann::json& objectJson,
        const char* name,
        Color defaultValue);
