#pragma once

#include "resources/ResourceData.h"

TextureHandle LoadTextureAsset(
        ResourceData& resources,
        const char* filePath,
        ResourceScope scope = ResourceScope::Global);

TextureHandle LoadTextureAsset(
        ResourceData& resources,
        const char* filePath,
        const TextureLoadSettings& settings,
        ResourceScope scope = ResourceScope::Global);

TextureResource* FindTextureResource(ResourceData& resources, TextureHandle handle);
const TextureResource* FindTextureResource(const ResourceData& resources, TextureHandle handle);