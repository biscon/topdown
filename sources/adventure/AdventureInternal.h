#pragma once

#include <string>
#include "raylib.h"
#include "data/GameState.h"
#include "adventure/AdventureData.h"

float AdventureLength(Vector2 v);
Vector2 AdventureNormalizeOrZero(Vector2 v);

void AdventureSwitchToDirectionalIdle(ActorInstance& actor);
void AdventureForceFacing(ActorInstance& actor, SceneFacing facing);

ActorInstance* AdventureFindSceneActorById(GameState& state, const std::string& actorId, int* outActorIndex = nullptr);
const ActorDefinitionData* AdventureGetActorDefinitionForInstance(const GameState& state, const ActorInstance& actor);

bool AdventureQueueActorPathToPoint(
        GameState& state,
        ActorInstance& actor,
        Vector2 walkTarget,
        bool fastMove);

float AdventureComputeSpeechDurationMs(const std::string& text, int overrideDurationMs);

void AdventureStartSpeech(
        GameState& state,
        SpeechAnchorType anchorType,
        int actorIndex,
        int propIndex,
        Vector2 worldPos,
        const std::string& text,
        Color color,
        int durationMs,
        bool skippable = true);

void AdventureStartAmbientSpeech(
        GameState& state,
        SpeechAnchorType anchorType,
        int actorIndex,
        int propIndex,
        Vector2 worldPos,
        const std::string& text,
        Color color,
        int durationMs);

bool AdventureTryGetSpriteAnimationDurationMs(
        const GameState& state,
        SpriteAssetHandle spriteAssetHandle,
        const std::string& animationName,
        float& outDurationMs);

int AdventureFindScenePropIndexById(const GameState& state, const std::string& propId);

void AdventureQueueLoadSceneInternal(GameState& state, const char* sceneId, const char* spawnId);
