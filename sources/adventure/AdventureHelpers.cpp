#include "AdventureHelpers.h"

#include <cctype>

std::string SanitizeIdForMethod(const std::string& input)
{
    std::string out;
    out.reserve(input.size());

    for (unsigned char c : input) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else {
            out.push_back('_');
        }
    }

    return out;
}

std::string BuildSceneMethodName(const char* prefix, const std::string& objectId)
{
    return std::string("Scene_") + prefix + SanitizeIdForMethod(objectId);
}

ActorInstance* FindActorInstanceById(GameState& state, const std::string& actorId)
{
    for (ActorInstance& actor : state.adventure.actors) {
        if (actor.actorId == actorId) {
            return &actor;
        }
    }
    return nullptr;
}

const ActorInstance* FindActorInstanceById(const GameState& state, const std::string& actorId)
{
    for (const ActorInstance& actor : state.adventure.actors) {
        if (actor.actorId == actorId) {
            return &actor;
        }
    }
    return nullptr;
}

ActorInstance* GetControlledActor(GameState& state)
{
    const int index = state.adventure.controlledActorIndex;
    if (index < 0 || index >= static_cast<int>(state.adventure.actors.size())) {
        return nullptr;
    }
    return &state.adventure.actors[index];
}

const ActorInstance* GetControlledActor(const GameState& state)
{
    const int index = state.adventure.controlledActorIndex;
    if (index < 0 || index >= static_cast<int>(state.adventure.actors.size())) {
        return nullptr;
    }
    return &state.adventure.actors[index];
}

int FindActorDefinitionIndexById(const GameState& state, const std::string& actorId)
{
    for (int i = 0; i < static_cast<int>(state.adventure.actorDefinitions.size()); ++i) {
        if (state.adventure.actorDefinitions[i].actorId == actorId) {
            return i;
        }
    }
    return -1;
}

const ActorDefinitionData* FindActorDefinitionById(const GameState& state, const std::string& actorId)
{
    const int index = FindActorDefinitionIndexById(state, actorId);
    if (index < 0) {
        return nullptr;
    }
    return &state.adventure.actorDefinitions[index];
}

int FindActorInstanceIndexById(const GameState& state, const std::string& actorId)
{
    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        if (state.adventure.actors[i].actorId == actorId) {
            return i;
        }
    }
    return -1;
}

ActorDefinitionData* FindActorDefinitionByIndex(GameState& state, int actorDefIndex)
{
    if (actorDefIndex < 0 ||
        actorDefIndex >= static_cast<int>(state.adventure.actorDefinitions.size())) {
        return nullptr;
    }
    return &state.adventure.actorDefinitions[actorDefIndex];
}

const ActorDefinitionData* FindActorDefinitionByIndex(const GameState& state, int actorDefIndex)
{
    if (actorDefIndex < 0 ||
        actorDefIndex >= static_cast<int>(state.adventure.actorDefinitions.size())) {
        return nullptr;
    }
    return &state.adventure.actorDefinitions[actorDefIndex];
}

int GetControlledActorIndex(const GameState& state)
{
    const int index = state.adventure.controlledActorIndex;
    if (index < 0 || index >= static_cast<int>(state.adventure.actors.size())) {
        return -1;
    }
    return index;
}

bool HasControlledActor(const GameState& state)
{
    return GetControlledActorIndex(state) >= 0;
}

const ActorInstance* FindActorInstanceByHandle(const GameState& state, ActorHandle handle)
{
    return &state.adventure.actors[handle];
}

ActorInstance* FindActorInstanceByHandle(GameState& state, ActorHandle handle)
{
    return &state.adventure.actors[handle];
}

SceneImageLayer* FindSceneImageLayerByName(GameState& state, const std::string& layerName)
{
    for (SceneImageLayer& layer : state.adventure.currentScene.backgroundLayers) {
        if (layer.name == layerName) {
            return &layer;
        }
    }

    for (SceneImageLayer& layer : state.adventure.currentScene.foregroundLayers) {
        if (layer.name == layerName) {
            return &layer;
        }
    }

    return nullptr;
}

const SceneImageLayer* FindSceneImageLayerByName(const GameState& state, const std::string& layerName)
{
    for (const SceneImageLayer& layer : state.adventure.currentScene.backgroundLayers) {
        if (layer.name == layerName) {
            return &layer;
        }
    }

    for (const SceneImageLayer& layer : state.adventure.currentScene.foregroundLayers) {
        if (layer.name == layerName) {
            return &layer;
        }
    }

    return nullptr;
}
