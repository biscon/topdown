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

const TopdownPlayerWeaponConfig* TopdownPlayerGetCurrentWeaponConfig(
        const GameState& state);

bool TopdownPlayerWeaponUsesAmmo(
        const TopdownPlayerWeaponConfig& config);

bool TopdownPlayerCurrentWeaponUsesAmmo(
        const GameState& state);

bool TopdownPlayerHasEquipmentSet(
        const GameState& state,
        const std::string& equipmentSetId);

bool TopdownPlayerAddEquipmentSet(
        GameState& state,
        const std::string& equipmentSetId);

bool TopdownPlayerRemoveEquipmentSet(
        GameState& state,
        const std::string& equipmentSetId);

bool TopdownPlayerEquipEquipmentSet(
        GameState& state,
        const std::string& equipmentSetId);

bool TopdownPlayerEquipSlot(
        GameState& state,
        int slot);

int TopdownPlayerGetReserveAmmo(
        const GameState& state,
        const std::string& ammoType);

int TopdownPlayerGetLoadedAmmo(
        const GameState& state,
        const std::string& equipmentSetId);

bool TopdownPlayerSetReserveAmmo(
        GameState& state,
        const std::string& ammoType,
        int count);

bool TopdownPlayerSetLoadedAmmo(
        GameState& state,
        const std::string& equipmentSetId,
        int count);

bool TopdownPlayerAddAmmo(
        GameState& state,
        const std::string& ammoType,
        int amount);

bool TopdownPlayerRemoveAmmo(
        GameState& state,
        const std::string& ammoType,
        int amount);

bool TopdownPlayerCanUseHealthItem(const GameState& state);
bool TopdownPlayerUseHealthItem(GameState& state);

bool TopdownPlayerCanReloadCurrentWeapon(const GameState& state);
bool TopdownPlayerStartReload(GameState& state);
void TopdownPlayerCancelReload(GameState& state);
void TopdownPlayerUpdateReload(GameState& state, float dt);
void TopdownPlayerValidateReloadState(GameState& state);

void TopdownValidatePlayerEquipmentRuntime(GameState& state);

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