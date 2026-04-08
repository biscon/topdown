#pragma once

#include "data/GameState.h"

bool GetActorCurrentFrameInfo(
        const GameState& state,
        const ActorInstance& actor,
        const SpriteAssetResource*& outAsset,
        const SpriteFrame*& outFrame,
        float& outFinalScale,
        Vector2& outScreenFeet);

Rectangle GetActorScreenRect(const GameState& state, const ActorInstance& actor);
Rectangle GetActorWorldRect(const GameState& state, const ActorInstance& actor);
Rectangle GetActorInteractionRect(const GameState& state, const ActorInstance& actor);

Rectangle GetPropScreenRect(
        const GameState& state,
        const ScenePropData& sceneProp,
        const PropInstance& prop);
