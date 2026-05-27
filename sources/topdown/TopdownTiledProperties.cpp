#include "topdown/TopdownTiledProperties.h"

const nlohmann::json* TopdownFindTiledObjectProperty(
        const nlohmann::json& objectJson,
        const char* name)
{
    auto it = objectJson.find("properties");
    if (it == objectJson.end() || !it->is_array()) {
        return nullptr;
    }

    for (const nlohmann::json& prop : *it) {
        if (prop.is_object() && prop.value("name", std::string()) == name) {
            return &prop;
        }
    }

    return nullptr;
}

bool TopdownGetTiledObjectPropertyBool(
        const nlohmann::json& objectJson,
        const char* name,
        bool defaultValue)
{
    const nlohmann::json* prop = TopdownFindTiledObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", defaultValue);
}

float TopdownGetTiledObjectPropertyFloat(
        const nlohmann::json& objectJson,
        const char* name,
        float defaultValue)
{
    const nlohmann::json* prop = TopdownFindTiledObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", defaultValue);
}

std::string TopdownGetTiledObjectPropertyString(
        const nlohmann::json& objectJson,
        const char* name,
        const std::string& defaultValue)
{
    const nlohmann::json* prop = TopdownFindTiledObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }
    return prop->value("value", defaultValue);
}

static int HexDigitValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool TopdownParseTiledColorString(const std::string& text, Color& outColor)
{
    if (text.empty() || text[0] != '#') {
        return false;
    }

    auto readByte = [&](int index, unsigned char& outByte) -> bool {
        const int hi = HexDigitValue(text[index]);
        const int lo = HexDigitValue(text[index + 1]);
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

    if (text.size() == 7) {
        if (!readByte(1, r)) return false;
        if (!readByte(3, g)) return false;
        if (!readByte(5, b)) return false;

        outColor = Color{ r, g, b, 255 };
        return true;
    }

    if (text.size() == 9) {
        if (!readByte(1, a)) return false;
        if (!readByte(3, r)) return false;
        if (!readByte(5, g)) return false;
        if (!readByte(7, b)) return false;

        outColor = Color{ r, g, b, a };
        return true;
    }

    return false;
}

Color TopdownGetTiledLayerTintColor(const nlohmann::json& layer, Color defaultValue)
{
    const std::string value = layer.value("tintcolor", std::string());
    if (value.empty()) {
        return defaultValue;
    }

    Color parsed{};
    if (!TopdownParseTiledColorString(value, parsed)) {
        return defaultValue;
    }

    return parsed;
}

Color TopdownGetTiledObjectPropertyColor(
        const nlohmann::json& objectJson,
        const char* name,
        Color defaultValue)
{
    const nlohmann::json* prop = TopdownFindTiledObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }

    const std::string value = prop->value("value", std::string());
    Color parsed{};
    if (!TopdownParseTiledColorString(value, parsed)) {
        return defaultValue;
    }

    return parsed;
}
