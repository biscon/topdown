#include <filesystem>
#include "Resources.h"

static void RebuildTextureIndices(ResourceData& resources)
{
    resources.textureIndexByHandle.clear();
    resources.textureHandleByPath.clear();

    for (size_t i = 0; i < resources.textures.size(); ++i) {
        resources.textureIndexByHandle[resources.textures[i].handle] = i;
        resources.textureHandleByPath[resources.textures[i].path] = resources.textures[i].handle;
    }
}

static void RebuildSpriteIndices(ResourceData& resources)
{
    resources.spriteAssetIndexByHandle.clear();
    resources.spriteAssetHandleByPath.clear();

    for (size_t i = 0; i < resources.spriteAssets.size(); ++i) {
        resources.spriteAssetIndexByHandle[resources.spriteAssets[i].handle] = i;
        resources.spriteAssetHandleByPath[resources.spriteAssets[i].cacheKey] =
                resources.spriteAssets[i].handle;
    }
}

void UnloadSceneResources(ResourceData& resources)
{
    for (auto it = resources.spriteAssets.begin(); it != resources.spriteAssets.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            it = resources.spriteAssets.erase(it);
        } else {
            ++it;
        }
    }

    RebuildSpriteIndices(resources);

    for (auto it = resources.textures.begin(); it != resources.textures.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            if (it->loaded && it->texture.id != 0) {
                UnloadTexture(it->texture);
            }
            it = resources.textures.erase(it);
        } else {
            ++it;
        }
    }

    RebuildTextureIndices(resources);

    for (auto it = resources.sounds.begin(); it != resources.sounds.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            UnloadSound(it->sound);
            it = resources.sounds.erase(it);
        } else {
            ++it;
        }
    }

    resources.soundPathToHandle.clear();
    for (const SoundResource& sound : resources.sounds) {
        resources.soundPathToHandle[sound.path] = sound.handle;
    }

    for (auto it = resources.musics.begin(); it != resources.musics.end(); ) {
        if (it->scope == ResourceScope::Scene) {
            UnloadMusicStream(it->music);
            it = resources.musics.erase(it);
        } else {
            ++it;
        }
    }

    resources.musicPathToHandle.clear();
    for (const MusicResource& music : resources.musics) {
        resources.musicPathToHandle[music.path] = music.handle;
    }
}

static void UnloadAllTextureAssets(ResourceData& resources)
{
    for (auto& tex : resources.textures) {
        if (tex.loaded && tex.texture.id != 0) {
            UnloadTexture(tex.texture);
            tex.texture = {};
            tex.loaded = false;
        }
    }

    resources.textures.clear();
    resources.textureIndexByHandle.clear();
    resources.textureHandleByPath.clear();
    resources.nextTextureHandle = 1;
}

static void UnloadAllSoundAssets(ResourceData& resources)
{
    for (auto& sound : resources.sounds) {
        UnloadSound(sound.sound);
        sound.sound = {};
    }

    resources.sounds.clear();
    resources.soundPathToHandle.clear();
}

static void UnloadAllMusicAssets(ResourceData& resources)
{
    for (auto& music : resources.musics) {
        UnloadMusicStream(music.music);
        music.music = {};
    }

    resources.musics.clear();
    resources.musicPathToHandle.clear();
}

void UnloadAllResources(ResourceData& resources)
{
    resources.spriteAssets.clear();
    resources.spriteAssetIndexByHandle.clear();
    resources.spriteAssetHandleByPath.clear();
    resources.nextSpriteAssetHandle = 1;

    UnloadAllTextureAssets(resources);
    UnloadAllSoundAssets(resources);
    UnloadAllMusicAssets(resources);
}

SoundHandle LoadSoundResource(GameState& state, const std::string& path, ResourceScope scope)
{
    const std::string normPath = std::filesystem::path(path).lexically_normal().string();

    auto it = state.resources.soundPathToHandle.find(normPath);
    if (it != state.resources.soundPathToHandle.end()) {
        return it->second;
    }

    Sound sound = LoadSound(normPath.c_str());
    if (sound.frameCount <= 0) {
        TraceLog(LOG_ERROR, "Failed to load sound: %s", normPath.c_str());
        return -1;
    }

    SoundResource res;
    res.handle = static_cast<SoundHandle>(state.resources.sounds.size() + 1);
    res.path = normPath;
    res.sound = sound;
    res.scope = scope;

    state.resources.sounds.push_back(res);
    state.resources.soundPathToHandle[normPath] = res.handle;

    TraceLog(LOG_INFO, "Loaded sound: %s", normPath.c_str());
    return res.handle;
}

MusicHandle LoadMusicResource(GameState& state, const std::string& path, ResourceScope scope)
{
    const std::string normPath = std::filesystem::path(path).lexically_normal().string();

    auto it = state.resources.musicPathToHandle.find(normPath);
    if (it != state.resources.musicPathToHandle.end()) {
        return it->second;
    }

    Music music = LoadMusicStream(normPath.c_str());
    if (music.ctxData == nullptr) {
        TraceLog(LOG_ERROR, "Failed to load music: %s", normPath.c_str());
        return -1;
    }

    MusicResource res;
    res.handle = static_cast<MusicHandle>(state.resources.musics.size() + 1);
    res.path = normPath;
    res.music = music;
    res.scope = scope;

    state.resources.musics.push_back(res);
    state.resources.musicPathToHandle[normPath] = res.handle;

    TraceLog(LOG_INFO, "Loaded music: %s", normPath.c_str());
    return res.handle;
}

Sound* GetSoundResource(GameState& state, SoundHandle handle)
{
    if (handle <= 0) {
        return nullptr;
    }

    const int index = handle - 1;
    if (index < 0 || index >= static_cast<int>(state.resources.sounds.size())) {
        return nullptr;
    }

    return &state.resources.sounds[index].sound;
}

Music* GetMusicResource(GameState& state, MusicHandle handle)
{
    if (handle <= 0) {
        return nullptr;
    }

    const int index = handle - 1;
    if (index < 0 || index >= static_cast<int>(state.resources.musics.size())) {
        return nullptr;
    }

    return &state.resources.musics[index].music;
}
