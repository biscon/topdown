#pragma once

#include "ResourceData.h"

/*
 * aseprite spritesheet export: remember to split layers and tags, use packed, turn off slices and use this naming:
 * {title} ({layer}) #{tag} {frame}
 */

SpriteAssetHandle LoadSpriteAsset(
        ResourceData& resources,
        const char* sidecarPath,
        ResourceScope scope = ResourceScope::Scene);

SpriteAssetHandle LoadSpriteAssetFromAsepriteJson(
        ResourceData& resources,
        const char* asepriteJsonPath,
        float baseDrawScale = 1.0f,
        ResourceScope scope = ResourceScope::Scene);

SpriteAssetHandle LoadSpriteAssetFromAsepriteJsonWithOrigin(
        ResourceData& resources,
        const char* asepriteJsonPath,
        float baseDrawScale,
        Vector2 origin,
        ResourceScope scope = ResourceScope::Scene);

SpriteAssetResource* FindSpriteAssetResource(ResourceData& resources, SpriteAssetHandle handle);
const SpriteAssetResource* FindSpriteAssetResource(const ResourceData& resources, SpriteAssetHandle handle);
int FindClipIndex(const SpriteAssetResource& asset, const std::string& layerName, const std::string& tagName);
int GetLoopingFrameIndex(const SpriteAssetResource& asset, const SpriteClip& clip, float timeMs);
float GetOneShotClipDurationMs(const SpriteAssetResource& asset, const SpriteClip& clip);
int GetOneShotFrameIndex(const SpriteAssetResource& asset, const SpriteClip& clip, float timeMs);