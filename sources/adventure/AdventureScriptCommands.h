#pragma once

#include "data/GameState.h"

// Script commands

bool AdventureScriptSay(GameState& state, const std::string& text, int durationMs = -1);
bool AdventureScriptSayProp(GameState& state,
                            const std::string& propId,
                            const std::string& text,
                            const Color* overrideColor = nullptr,
                            int durationMs = -1);
bool AdventureScriptSayAt(GameState& state,
                          Vector2 worldPos,
                          const std::string& text,
                          Color color = WHITE,
                          int durationMs = -1);

bool AdventureScriptStartSay(GameState& state, const std::string& text, int durationMs = -1);
bool AdventureScriptStartSayProp(GameState& state,
                                 const std::string& propId,
                                 const std::string& text,
                                 const Color* overrideColor = nullptr,
                                 int durationMs = -1);
bool AdventureScriptStartSayAt(GameState& state,
                               Vector2 worldPos,
                               const std::string& text,
                               Color color = WHITE,
                               int durationMs = -1);
bool AdventureScriptStartSayActor(GameState& state,
                                  const std::string& actorId,
                                  const std::string& text,
                                  const Color* overrideColor = nullptr,
                                  int durationMs = -1);

bool AdventureScriptWalkTo(GameState& state, Vector2 worldPos);
bool AdventureScriptWalkToHotspot(GameState& state, const std::string& hotspotId);
bool AdventureScriptWalkToExit(GameState& state, const std::string& exitId);
bool AdventureScriptFace(GameState& state, const std::string& facingName);
bool AdventureScriptChangeScene(GameState& state, const std::string& sceneId, const std::string& spawnId);
bool AdventureScriptPlayAnimation(GameState& state, const std::string& animationName);
bool AdventureScriptPlayPropAnimation(GameState& state, const std::string& propId, const std::string& animationName);
bool AdventureScriptSetPropAnimation(GameState& state, const std::string& propId, const std::string& animationName);
bool AdventureScriptSetPropPosition(GameState& state, const std::string& propId, Vector2 worldPos);
bool AdventureScriptMovePropTo(GameState& state,
                               const std::string& propId,
                               Vector2 targetPos,
                               float durationMs,
                               const std::string& interpolationName);
bool AdventureScriptSetPropVisible(GameState& state, const std::string& propId, bool visible);
bool AdventureScriptSetPropFlipX(GameState& state, const std::string& propId, bool flipX);

bool AdventureScriptSetPropPositionRelative(GameState& state, const std::string& propId, Vector2 delta);
bool AdventureScriptMovePropBy(GameState& state,
                               const std::string& propId,
                               Vector2 delta,
                               float durationMs,
                               const std::string& interpolationName);
bool AdventureScriptControlActor(GameState& state, const std::string& actorId);

bool AdventureScriptSayActor(GameState& state,
                             const std::string& actorId,
                             const std::string& text,
                             const Color* overrideColor = nullptr,
                             int durationMs = -1);

bool AdventureScriptWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos, int* outActorIndex = nullptr);
bool AdventureScriptWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId, int* outActorIndex = nullptr);
bool AdventureScriptWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId, int* outActorIndex = nullptr);

bool AdventureScriptFaceActor(GameState& state, const std::string& actorId, const std::string& facingName);
bool AdventureScriptPlayActorAnimation(GameState& state, const std::string& actorId, const std::string& animationName);
bool AdventureScriptSetActorVisible(GameState& state, const std::string& actorId, bool visible);
bool AdventureScriptStartWalkTo(GameState& state, Vector2 worldPos);
bool AdventureScriptStartWalkToHotspot(GameState& state, const std::string& hotspotId);
bool AdventureScriptStartWalkToExit(GameState& state, const std::string& exitId);

bool AdventureScriptStartWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos);
bool AdventureScriptStartWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId);
bool AdventureScriptStartWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId);
bool AdventureScriptSetControlsEnabled(GameState& state, bool enabled);
bool AdventureScriptSetEffectVisible(GameState& state, const std::string& effectId, bool visible);
bool AdventureScriptIsEffectVisible(const GameState& state, const std::string& effectId, bool& outVisible);
bool AdventureScriptSetEffectOpacity(GameState& state, const std::string& effectId, float opacity);
bool AdventureScriptGetEffectOpacity(const GameState& state, const std::string& effectId, float& outOpacity);
bool AdventureScriptSetEffectTint(GameState& state,
                                  const std::string& effectId,
                                  Color tint);

bool AdventureScriptPlaySound(GameState& state, const std::string& audioId);
bool AdventureScriptStopSound(GameState& state, const std::string& audioId);
bool AdventureScriptPlayMusic(GameState& state, const std::string& audioId, float fadeMs = 0.0f);
bool AdventureScriptStopMusic(GameState& state, float fadeMs = 0.0f);

bool AdventureScriptSetSoundEmitterEnabled(GameState& state, const std::string& emitterId, bool enabled);
bool AdventureScriptGetSoundEmitterEnabled(const GameState& state, const std::string& emitterId, bool& outEnabled);
bool AdventureScriptSetSoundEmitterVolume(GameState& state, const std::string& emitterId, float volume);
bool AdventureScriptPlayEmitter(GameState& state, const std::string& emitterId);
bool AdventureScriptStopEmitter(GameState& state, const std::string& emitterId);
bool AdventureScriptSetLayerVisible(GameState& state, const std::string& layerName, bool visible);
bool AdventureScriptIsLayerVisible(const GameState& state, const std::string& layerName, bool& outVisible);
bool AdventureScriptSetLayerOpacity(GameState& state, const std::string& layerName, float opacity);
bool AdventureScriptGetLayerOpacity(const GameState& state, const std::string& layerName, float& outOpacity);
bool AdventureScriptCameraFollowControlledActor(GameState& state);
bool AdventureScriptCameraFollowActor(GameState& state, const std::string& actorId);
bool AdventureScriptSetCameraPosition(GameState& state, Vector2 worldPos);
bool AdventureScriptMoveCameraTo(GameState& state,
                                 Vector2 worldPos,
                                 float durationMs,
                                 const std::string& interpolationName);

bool AdventureScriptCenterCameraOn(GameState& state, Vector2 worldPos);
bool AdventureScriptMoveCameraCenterTo(GameState& state,
                                       Vector2 worldPos,
                                       float durationMs,
                                       const std::string& interpolationName);

bool AdventureScriptPanCameraToActor(GameState& state,
                                     const std::string& actorId,
                                     float durationMs,
                                     const std::string& interpolationName);

bool AdventureScriptPanCameraToProp(GameState& state,
                                    const std::string& propId,
                                    float durationMs,
                                    const std::string& interpolationName);

bool AdventureScriptPanCameraToHotspot(GameState& state,
                                       const std::string& hotspotId,
                                       float durationMs,
                                       const std::string& interpolationName);

bool AdventureScriptShakeScreen(GameState& state,
                                float durationMs,
                                float strengthPx,
                                float frequencyHz = 30.0f,
                                bool smooth = false);

bool AdventureScriptSetEffectRegionVisible(GameState& state, const std::string& effectRegionId, bool visible);
bool AdventureScriptIsEffectRegionVisible(const GameState& state, const std::string& effectRegionId, bool& outVisible);
bool AdventureScriptSetEffectRegionOpacity(GameState& state, const std::string& effectRegionId, float opacity);
bool AdventureScriptGetEffectRegionOpacity(const GameState& state, const std::string& effectRegionId, float& outOpacity);

bool AdventureScriptSetActorPosition(GameState& state, const std::string& actorId, Vector2 worldPos);
bool AdventureScriptGetActorPosition(const GameState& state, const std::string& actorId, Vector2& outWorldPos);
bool AdventureScriptGetPropPosition(const GameState& state, const std::string& propId, Vector2& outWorldPos);

bool AdventureScriptGetHotspotInteractionPosition(const GameState& state,
                                                  const std::string& hotspotId,
                                                  Vector2& outWorldPos);
