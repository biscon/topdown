#include "TextureAsset.h"

#include <filesystem>
#include <sstream>
#include "raylib.h"

static std::string NormalizePath(const char* filePath)
{
    return std::filesystem::path(filePath).lexically_normal().string();
}

static int ToRaylibTextureFilter(TextureFilterMode mode)
{
    switch (mode) {
        case TextureFilterMode::Bilinear:
            return TEXTURE_FILTER_BILINEAR;

        case TextureFilterMode::Point:
        default:
            return TEXTURE_FILTER_POINT;
    }
}

static int ToRaylibTextureWrap(TextureWrapMode mode)
{
    switch (mode) {
        case TextureWrapMode::Repeat:
            return TEXTURE_WRAP_REPEAT;

        case TextureWrapMode::Clamp:
        default:
            return TEXTURE_WRAP_CLAMP;
    }
}

static std::string BuildTextureCacheKey(
        const std::string& normalizedPath,
        const TextureLoadSettings& settings)
{
    std::ostringstream out;
    out << normalizedPath
        << "|pma=" << (settings.premultiplyAlpha ? 1 : 0)
        << "|filter=" << static_cast<int>(settings.filter)
        << "|wrap=" << static_cast<int>(settings.wrap);
    return out.str();
}

static Texture2D LoadTextureWithSettings(
        const char* fileName,
        const TextureLoadSettings& settings)
{
    Image img = LoadImage(fileName);
    if (img.data == nullptr) {
        TraceLog(LOG_ERROR, "Failed to load image: %s", fileName);
        return Texture2D{};
    }

    if (settings.premultiplyAlpha) {
        ImageAlphaPremultiply(&img);
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    if (tex.id != 0) {
        SetTextureFilter(tex, ToRaylibTextureFilter(settings.filter));
        SetTextureWrap(tex, ToRaylibTextureWrap(settings.wrap));
    }

    return tex;
}

TextureHandle LoadTextureAsset(
        ResourceData& resources,
        const char* filePath,
        ResourceScope scope)
{
    const TextureLoadSettings defaultSettings{};
    return LoadTextureAsset(resources, filePath, defaultSettings, scope);
}

TextureHandle LoadTextureAsset(
        ResourceData& resources,
        const char* filePath,
        const TextureLoadSettings& settings,
        ResourceScope scope)
{
    const std::string normPath = NormalizePath(filePath);
    const std::string cacheKey = BuildTextureCacheKey(normPath, settings);

    auto existing = resources.textureHandleByPath.find(cacheKey);
    if (existing != resources.textureHandleByPath.end()) {
        return existing->second;
    }

    Texture2D tex = LoadTextureWithSettings(normPath.c_str(), settings);
    if (tex.id == 0) {
        return -1;
    }

    TextureResource res;
    res.handle = resources.nextTextureHandle++;
    res.path = normPath;
    res.texture = tex;
    res.loaded = true;
    res.scope = scope;
    res.premultiplyAlpha = settings.premultiplyAlpha;
    res.filterMode = settings.filter;
    res.wrapMode = settings.wrap;

    const size_t index = resources.textures.size();
    resources.textures.push_back(res);
    resources.textureIndexByHandle[res.handle] = index;
    resources.textureHandleByPath[cacheKey] = res.handle;

    TraceLog(LOG_INFO, "Loaded texture: %s", normPath.c_str());
    return res.handle;
}

TextureResource* FindTextureResource(ResourceData& resources, TextureHandle handle)
{
    auto it = resources.textureIndexByHandle.find(handle);
    if (it == resources.textureIndexByHandle.end()) {
        return nullptr;
    }
    return &resources.textures[it->second];
}

const TextureResource* FindTextureResource(const ResourceData& resources, TextureHandle handle)
{
    auto it = resources.textureIndexByHandle.find(handle);
    if (it == resources.textureIndexByHandle.end()) {
        return nullptr;
    }
    return &resources.textures.at(it->second);
}
