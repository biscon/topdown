#pragma once

#include <string>
#include "data/GameState.h"

bool LoadTopdownPlayerCharacterAssets(GameState& state);
void InitializeTopdownPlayerCharacterRuntime(GameState& state);

SpriteAssetHandle FindTopdownPlayerAnimationHandle(
        const GameState& state,
        const std::string& animationId);

bool HasTopdownPlayerAnimation(
        const GameState& state,
        const std::string& animationId);

SpriteAssetHandle FindTopdownPlayerFeetAnimationHandle(
        const GameState& state,
        const std::string& suffix);

SpriteAssetHandle FindTopdownPlayerEquipmentAnimationHandle(
        const GameState& state,
        const std::string& equipmentSetId,
        const std::string& suffix);

bool HasTopdownPlayerEquipmentAnimationSet(
        const GameState& state,
        const std::string& equipmentSetId);

const TopdownPlayerWeaponConfig* FindTopdownPlayerWeaponConfigByEquipmentSetId(
        const GameState& state,
        const std::string& equipmentSetId);

const TopdownPlayerWeaponConfig* FindTopdownPlayerWeaponConfigBySlot(
        const GameState& state,
        int slot);

SpriteAssetHandle FindTopdownPlayerEquipmentAttackAnimationHandle(
        const GameState& state,
        const std::string& equipmentSetId,
        TopdownAttackType attackType);

const TopdownPlayerAnimationEntry* FindTopdownPlayerAnimationEntry(
        const GameState& state,
        const std::string& animationId);

std::string FindTopdownPlayerEquipmentAttackAnimationId(
        const GameState& state,
        const std::string& equipmentSetId,
        TopdownAttackType attackType);