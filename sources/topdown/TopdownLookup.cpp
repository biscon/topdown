#include "topdown/TopdownLookup.h"

TopdownRuntimeImageLayer* TopdownFindRuntimeImageLayerByName(
        GameState& state,
        const std::string& name)
{
    for (TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.bottomLayers) {
        if (layer.authoredIndex < 0 ||
            layer.authoredIndex >= static_cast<int>(state.topdown.authored.imageLayers.size())) {
            continue;
        }
        if (state.topdown.authored.imageLayers[layer.authoredIndex].name == name) {
            return &layer;
        }
    }

    for (TopdownRuntimeImageLayer& layer : state.topdown.runtime.render.topLayers) {
        if (layer.authoredIndex < 0 ||
            layer.authoredIndex >= static_cast<int>(state.topdown.authored.imageLayers.size())) {
            continue;
        }
        if (state.topdown.authored.imageLayers[layer.authoredIndex].name == name) {
            return &layer;
        }
    }

    return nullptr;
}

TopdownRuntimeEffectRegion* TopdownFindRuntimeEffectRegionById(
        GameState& state,
        const std::string& id)
{
    for (TopdownRuntimeEffectRegion& effect : state.topdown.runtime.render.effectRegions) {
        if (effect.authoredIndex < 0 ||
            effect.authoredIndex >= static_cast<int>(state.topdown.authored.effectRegions.size())) {
            continue;
        }
        if (state.topdown.authored.effectRegions[effect.authoredIndex].id == id) {
            return &effect;
        }
    }

    return nullptr;
}

TopdownRuntimeTrigger* TopdownFindRuntimeTriggerById(
        GameState& state,
        const std::string& id)
{
    for (TopdownRuntimeTrigger& trigger : state.topdown.runtime.triggers) {
        if (trigger.authoredIndex < 0 ||
            trigger.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
            continue;
        }

        if (state.topdown.authored.triggers[trigger.authoredIndex].id == id) {
            return &trigger;
        }
    }

    return nullptr;
}

const TopdownRuntimeTrigger* TopdownFindRuntimeTriggerById(
        const GameState& state,
        const std::string& id)
{
    for (const TopdownRuntimeTrigger& trigger : state.topdown.runtime.triggers) {
        if (trigger.authoredIndex < 0 ||
            trigger.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
            continue;
        }

        if (state.topdown.authored.triggers[trigger.authoredIndex].id == id) {
            return &trigger;
        }
    }

    return nullptr;
}

TopdownNpcRuntime* TopdownFindActiveNpcById(GameState& state, const std::string& npcId)
{
    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.active && npc.id == npcId) {
            return &npc;
        }
    }
    return nullptr;
}

const TopdownNpcRuntime* TopdownFindActiveNpcById(const GameState& state, const std::string& npcId)
{
    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.active && npc.id == npcId) {
            return &npc;
        }
    }
    return nullptr;
}

TopdownRuntimeProp* TopdownFindActivePropById(GameState& state, const std::string& propId)
{
    for (TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (prop.active && prop.id == propId) {
            return &prop;
        }
    }
    return nullptr;
}

const TopdownRuntimeProp* TopdownFindActivePropById(const GameState& state, const std::string& propId)
{
    for (const TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (prop.active && prop.id == propId) {
            return &prop;
        }
    }
    return nullptr;
}

const TopdownAuthoredSpawn* TopdownFindAuthoredSpawnById(
        const GameState& state,
        const std::string& spawnId)
{
    for (const TopdownAuthoredSpawn& spawn : state.topdown.authored.spawns) {
        if (spawn.id == spawnId) {
            return &spawn;
        }
    }
    return nullptr;
}

const SpriteClip* TopdownFindSpriteClipByName(
        const SpriteAssetResource& sprite,
        const std::string& clipName)
{
    for (const SpriteClip& clip : sprite.clips) {
        if (clip.name == clipName) {
            return &clip;
        }
    }
    return nullptr;
}
