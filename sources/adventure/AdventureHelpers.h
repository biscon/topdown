#pragma once

#include <string>

#include "data/GameState.h"

std::string SanitizeIdForMethod(const std::string& input);
std::string BuildSceneMethodName(const char* prefix, const std::string& objectId);

ActorInstance* FindActorInstanceById(GameState& state, const std::string& actorId);
const ActorInstance* FindActorInstanceById(const GameState& state, const std::string& actorId);

ActorInstance* GetControlledActor(GameState& state);
const ActorInstance* GetControlledActor(const GameState& state);

int FindActorDefinitionIndexById(const GameState& state, const std::string& actorId);
const ActorDefinitionData* FindActorDefinitionById(const GameState& state, const std::string& actorId);
ActorDefinitionData* FindActorDefinitionByIndex(GameState& state, int actorDefIndex);
const ActorDefinitionData* FindActorDefinitionByIndex(const GameState& state, int actorDefIndex);
int FindActorInstanceIndexById(const GameState& state, const std::string& actorId);

int GetControlledActorIndex(const GameState& state);
bool HasControlledActor(const GameState& state);

const ActorInstance* FindActorInstanceByHandle(const GameState& state, ActorHandle handle);
ActorInstance* FindActorInstanceByHandle(GameState& state, ActorHandle handle);

SceneImageLayer* FindSceneImageLayerByName(GameState& state, const std::string& layerName);
const SceneImageLayer* FindSceneImageLayerByName(const GameState& state, const std::string& layerName);