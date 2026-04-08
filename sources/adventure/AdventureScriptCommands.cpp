#include "adventure/AdventureHelpers.h"
#include "raylib.h"
#include "raymath.h"
#include "audio/Audio.h"
#include "render/RenderHelpers.h"
#include "AdventureInternal.h"

bool AdventureScriptSay(GameState& state, const std::string& text, int durationMs)
{
    const ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, controlledActor->actorDefIndex);
    const Color talkColor = (actorDef != nullptr) ? actorDef->talkColor : WHITE;

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Player,
            -1,
            -1,
            {},
            text,
            talkColor,
            durationMs);
    return true;
}

bool AdventureScriptSayProp(GameState& state,
                            const std::string& propId,
                            const std::string& text,
                            const Color* overrideColor,
                            int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    if (!state.adventure.props[scenePropIndex].visible) {
        return false;
    }

    Color color = WHITE;
    if (overrideColor != nullptr) {
        color = *overrideColor;
    }

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Prop,
            -1,
            scenePropIndex,
            {},
            text,
            color,
            durationMs);
    return true;
}

bool AdventureScriptSayAt(GameState& state,
                          Vector2 worldPos,
                          const std::string& text,
                          Color color,
                          int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Position,
            -1,
            -1,
            worldPos,
            text,
            color,
            durationMs);
    return true;
}

bool AdventureScriptStartSay(GameState& state, const std::string& text, int durationMs)
{
    const ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, controlledActor->actorDefIndex);
    const Color talkColor = (actorDef != nullptr) ? actorDef->talkColor : WHITE;

    AdventureStartAmbientSpeech(
            state,
            SpeechAnchorType::Player,
            -1,
            -1,
            {},
            text,
            talkColor,
            durationMs);

    return true;
}

bool AdventureScriptStartSayProp(GameState& state,
                                 const std::string& propId,
                                 const std::string& text,
                                 const Color* overrideColor,
                                 int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    if (!state.adventure.props[scenePropIndex].visible) {
        return false;
    }

    Color color = WHITE;
    if (overrideColor != nullptr) {
        color = *overrideColor;
    }

    AdventureStartAmbientSpeech(
            state,
            SpeechAnchorType::Prop,
            -1,
            scenePropIndex,
            {},
            text,
            color,
            durationMs);

    return true;
}

bool AdventureScriptStartSayAt(GameState& state,
                               Vector2 worldPos,
                               const std::string& text,
                               Color color,
                               int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    AdventureStartAmbientSpeech(
            state,
            SpeechAnchorType::Position,
            -1,
            -1,
            worldPos,
            text,
            color,
            durationMs);

    return true;
}

bool AdventureScriptStartSayActor(GameState& state,
                                  const std::string& actorId,
                                  const std::string& text,
                                  const Color* overrideColor,
                                  int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef = AdventureGetActorDefinitionForInstance(state, *actor);

    Color color = WHITE;
    if (overrideColor != nullptr) {
        color = *overrideColor;
    } else if (actorDef != nullptr) {
        color = actorDef->talkColor;
    }

    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0) {
        return false;
    }

    AdventureStartAmbientSpeech(
            state,
            SpeechAnchorType::Actor,
            actorIndex,
            -1,
            {},
            text,
            color,
            durationMs);

    return true;
}

bool AdventureScriptWalkTo(GameState& state, Vector2 worldPos)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    state.adventure.actionQueue.push({
                                             AdventureActionType::WalkToPoint,
                                             WalkToPointAction{worldPos, worldPos, false}
                                     });
    return true;
}

bool AdventureScriptWalkToHotspot(GameState& state, const std::string& hotspotId)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const auto& hotspots = state.adventure.currentScene.hotspots;
    for (int i = 0; i < static_cast<int>(hotspots.size()); ++i) {
        if (hotspots[i].id == hotspotId) {
            state.adventure.actionQueue.push({
                                                     AdventureActionType::WalkToPoint,
                                                     WalkToPointAction{hotspots[i].walkTo, hotspots[i].walkTo, false}
                                             });
            return true;
        }
    }

    return false;
}

bool AdventureScriptWalkToExit(GameState& state, const std::string& exitId)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    const auto& exits = state.adventure.currentScene.exits;
    for (int i = 0; i < static_cast<int>(exits.size()); ++i) {
        if (exits[i].id == exitId) {
            state.adventure.actionQueue.push({
                                                     AdventureActionType::WalkToPoint,
                                                     WalkToPointAction{exits[i].walkTo, exits[i].walkTo, false}
                                             });
            return true;
        }
    }

    return false;
}

bool AdventureScriptFace(GameState& state, const std::string& facingName)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }

    SceneFacing facing = SceneFacing::Front;

    if (facingName == "left") {
        facing = SceneFacing::Left;
    } else if (facingName == "right") {
        facing = SceneFacing::Right;
    } else if (facingName == "back") {
        facing = SceneFacing::Back;
    } else if (facingName == "front") {
        facing = SceneFacing::Front;
    } else {
        return false;
    }

    controlledActor->path = {};
    state.adventure.pendingInteraction = {};
    AdventureForceFacing(*controlledActor, facing);
    return true;
}

bool AdventureScriptChangeScene(GameState& state, const std::string& sceneId, const std::string& spawnId)
{
    if (sceneId.empty()) {
        return false;
    }

    AdventureQueueLoadSceneInternal(
            state,
            sceneId.c_str(),
            spawnId.empty() ? nullptr : spawnId.c_str());

    return true;
}

bool AdventureScriptPlayAnimation(GameState& state, const std::string& animationName)
{
    ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return false;
    }
    ActorInstance& player = *controlledActor;

    float durationMs = 0.0f;
    const ActorDefinitionData* actorDef =
            FindActorDefinitionByIndex(state, player.actorDefIndex);
    if (actorDef == nullptr) {
        return false;
    }

    if (!AdventureTryGetSpriteAnimationDurationMs(state, actorDef->spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING, "Animation not found or has no duration: %s", animationName.c_str());
        return false;
    }
    player.flipX = false;

    player.path = {};
    state.adventure.pendingInteraction = {};

    player.currentAnimation = animationName;
    player.animationTimeMs = 0.0f;
    player.scriptAnimationActive = true;
    player.scriptAnimationDurationMs = durationMs;
    player.inIdleState = false;
    player.stoppedTimeMs = 0.0f;

    return true;
}

bool AdventureScriptPlayPropAnimation(GameState& state, const std::string& propId, const std::string& animationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    const ScenePropData& sceneProp = state.adventure.currentScene.props[scenePropIndex];
    PropInstance& prop = state.adventure.props[scenePropIndex];

    if (sceneProp.visualType != ScenePropVisualType::Sprite) {
        return false;
    }

    float durationMs = 0.0f;
    if (!AdventureTryGetSpriteAnimationDurationMs(state, sceneProp.spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING,
                 "Prop animation not found or has no duration: prop=%s anim=%s",
                 propId.c_str(),
                 animationName.c_str());
        return false;
    }

    prop.currentAnimation = animationName;
    prop.animationTimeMs = 0.0f;
    prop.oneShotActive = true;
    prop.oneShotDurationMs = durationMs;
    prop.visible = true;

    return true;
}

bool AdventureScriptSetPropAnimation(GameState& state, const std::string& propId, const std::string& animationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    const ScenePropData& sceneProp = state.adventure.currentScene.props[scenePropIndex];
    PropInstance& prop = state.adventure.props[scenePropIndex];

    if (sceneProp.visualType != ScenePropVisualType::Sprite) {
        return false;
    }

    float durationMs = 0.0f;
    if (!AdventureTryGetSpriteAnimationDurationMs(state, sceneProp.spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING,
                 "Prop animation not found or has no duration: prop=%s anim=%s",
                 propId.c_str(),
                 animationName.c_str());
        return false;
    }

    prop.currentAnimation = animationName;
    prop.animationTimeMs = 0.0f;
    prop.oneShotActive = false;
    prop.oneShotDurationMs = 0.0f;
    prop.visible = true;

    return true;
}

bool AdventureScriptSetPropPosition(GameState& state, const std::string& propId, Vector2 worldPos)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];
    prop.feetPos = worldPos;
    prop.moveActive = false;
    prop.moveStartPos = worldPos;
    prop.moveTargetPos = worldPos;
    prop.moveElapsedMs = 0.0f;
    prop.moveDurationMs = 0.0f;
    return true;
}

bool AdventureScriptMovePropTo(GameState& state,
                               const std::string& propId,
                               Vector2 targetPos,
                               float durationMs,
                               const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    MoveInterpolation interpolation = MoveInterpolation::Linear;
    if (!ParseInterpolation(interpolationName, interpolation)) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];

    if (durationMs <= 0.0f) {
        prop.feetPos = targetPos;
        prop.moveActive = false;
        prop.moveStartPos = targetPos;
        prop.moveTargetPos = targetPos;
        prop.moveElapsedMs = 0.0f;
        prop.moveDurationMs = 0.0f;
        prop.moveInterpolation = interpolation;
        return true;
    }

    prop.moveActive = true;
    prop.moveStartPos = prop.feetPos;
    prop.moveTargetPos = targetPos;
    prop.moveElapsedMs = 0.0f;
    prop.moveDurationMs = durationMs;
    prop.moveInterpolation = interpolation;
    return true;
}

bool AdventureScriptSetPropVisible(GameState& state, const std::string& propId, bool visible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    state.adventure.props[scenePropIndex].visible = visible;
    return true;
}

bool AdventureScriptSetPropFlipX(GameState& state, const std::string& propId, bool flipX)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    state.adventure.props[scenePropIndex].flipX = flipX;
    return true;
}

bool AdventureScriptSetPropPositionRelative(GameState& state, const std::string& propId, Vector2 delta)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];
    const Vector2 newPos{
            prop.feetPos.x + delta.x,
            prop.feetPos.y + delta.y
    };

    prop.feetPos = newPos;
    prop.moveActive = false;
    prop.moveStartPos = newPos;
    prop.moveTargetPos = newPos;
    prop.moveElapsedMs = 0.0f;
    prop.moveDurationMs = 0.0f;
    return true;
}

bool AdventureScriptMovePropBy(GameState& state,
                               const std::string& propId,
                               Vector2 delta,
                               float durationMs,
                               const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    PropInstance& prop = state.adventure.props[scenePropIndex];
    const Vector2 targetPos{
            prop.feetPos.x + delta.x,
            prop.feetPos.y + delta.y
    };

    return AdventureScriptMovePropTo(
            state,
            propId,
            targetPos,
            durationMs,
            interpolationName);
}

bool AdventureScriptControlActor(GameState& state, const std::string& actorId)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return false;
    }

    ActorInstance& actor = state.adventure.actors[actorIndex];
    if (!actor.activeInScene || !actor.visible) {
        return false;
    }

    const ActorDefinitionData* actorDef = FindActorDefinitionByIndex(state, actor.actorDefIndex);
    if (actorDef == nullptr || !actorDef->controllable) {
        return false;
    }

    state.adventure.controlledActorIndex = actorIndex;
    return true;
}

bool AdventureScriptSayActor(GameState& state,
                             const std::string& actorId,
                             const std::string& text,
                             const Color* overrideColor,
                             int durationMs)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef = AdventureGetActorDefinitionForInstance(state, *actor);

    Color color = WHITE;
    if (overrideColor != nullptr) {
        color = *overrideColor;
    } else if (actorDef != nullptr) {
        color = actorDef->talkColor;
    }

    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0) {
        return false;
    }

    AdventureStartSpeech(
            state,
            SpeechAnchorType::Actor,
            actorIndex,
            -1,
            {},
            text,
            color,
            durationMs);

    return true;
}

bool AdventureScriptWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos, int* outActorIndex)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    int actorIndex = -1;
    ActorInstance* actor = AdventureFindSceneActorById(state, actorId, &actorIndex);
    if (actor == nullptr) {
        return false;
    }

    if (outActorIndex != nullptr) {
        *outActorIndex = actorIndex;
    }

    return AdventureQueueActorPathToPoint(state, *actor, worldPos, false);
}

bool AdventureScriptWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId, int* outActorIndex)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const auto& hotspots = state.adventure.currentScene.hotspots;
    for (const SceneHotspot& hotspot : hotspots) {
        if (hotspot.id == hotspotId) {
            return AdventureScriptWalkActorTo(state, actorId, hotspot.walkTo, outActorIndex);
        }
    }

    return false;
}

bool AdventureScriptWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId, int* outActorIndex)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const auto& exits = state.adventure.currentScene.exits;
    for (const SceneExit& exitObj : exits) {
        if (exitObj.id == exitId) {
            return AdventureScriptWalkActorTo(state, actorId, exitObj.walkTo, outActorIndex);
        }
    }

    return false;
}

bool AdventureScriptFaceActor(GameState& state, const std::string& actorId, const std::string& facingName)
{
    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    SceneFacing facing = SceneFacing::Front;

    if (facingName == "left") {
        facing = SceneFacing::Left;
    } else if (facingName == "right") {
        facing = SceneFacing::Right;
    } else if (facingName == "back") {
        facing = SceneFacing::Back;
    } else if (facingName == "front") {
        facing = SceneFacing::Front;
    } else {
        return false;
    }

    actor->path = {};
    AdventureForceFacing(*actor, facing);
    return true;
}

bool AdventureScriptPlayActorAnimation(GameState& state, const std::string& actorId, const std::string& animationName)
{
    ActorInstance* actor = AdventureFindSceneActorById(state, actorId);
    if (actor == nullptr) {
        return false;
    }

    const ActorDefinitionData* actorDef = AdventureGetActorDefinitionForInstance(state, *actor);
    if (actorDef == nullptr) {
        return false;
    }

    float durationMs = 0.0f;
    if (!AdventureTryGetSpriteAnimationDurationMs(state, actorDef->spriteAssetHandle, animationName, durationMs)) {
        TraceLog(LOG_WARNING, "Actor animation not found or has no duration: actor=%s anim=%s",
                 actorId.c_str(), animationName.c_str());
        return false;
    }

    actor->path = {};
    actor->currentAnimation = animationName;
    actor->animationTimeMs = 0.0f;
    actor->scriptAnimationActive = true;
    actor->scriptAnimationDurationMs = durationMs;
    actor->inIdleState = false;
    actor->stoppedTimeMs = 0.0f;
    actor->flipX = false;

    return true;
}

bool AdventureScriptSetActorVisible(GameState& state, const std::string& actorId, bool visible)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return false;
    }

    ActorInstance& actor = state.adventure.actors[actorIndex];
    actor.visible = visible;

    if (!visible) {
        actor.path = {};
        actor.scriptAnimationActive = false;
    }

    return true;
}

bool AdventureScriptStartWalkTo(GameState& state, Vector2 worldPos)
{
    return AdventureScriptWalkTo(state, worldPos);
}

bool AdventureScriptStartWalkToHotspot(GameState& state, const std::string& hotspotId)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* controlledActor = GetControlledActor(state);
    if (controlledActor == nullptr) {
        return false;
    }

    const auto& hotspots = state.adventure.currentScene.hotspots;
    for (const SceneHotspot& hotspot : hotspots) {
        if (hotspot.id == hotspotId) {
            return AdventureQueueActorPathToPoint(state, *controlledActor, hotspot.walkTo, false);
        }
    }

    return false;
}

bool AdventureScriptStartWalkToExit(GameState& state, const std::string& exitId)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    ActorInstance* controlledActor = GetControlledActor(state);
    if (controlledActor == nullptr) {
        return false;
    }

    const auto& exits = state.adventure.currentScene.exits;
    for (const SceneExit& exitObj : exits) {
        if (exitObj.id == exitId) {
            return AdventureQueueActorPathToPoint(state, *controlledActor, exitObj.walkTo, false);
        }
    }

    return false;
}

bool AdventureScriptStartWalkActorTo(GameState& state, const std::string& actorId, Vector2 worldPos)
{
    return AdventureScriptWalkActorTo(state, actorId, worldPos, nullptr);
}

bool AdventureScriptStartWalkActorToHotspot(GameState& state, const std::string& actorId, const std::string& hotspotId)
{
    return AdventureScriptWalkActorToHotspot(state, actorId, hotspotId, nullptr);
}

bool AdventureScriptStartWalkActorToExit(GameState& state, const std::string& actorId, const std::string& exitId)
{
    return AdventureScriptWalkActorToExit(state, actorId, exitId, nullptr);
}

bool AdventureScriptSetControlsEnabled(GameState& state, bool enabled)
{
    state.adventure.controlsEnabled = enabled;
    if(!enabled) {
        //state.adventure.pendingInteraction = {};
        state.adventure.actionQueue.clear();
    }
    return true;
}

static int FindEffectSpriteIndexById(const GameState& state, const std::string& effectId)
{
    const int count = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < count; ++i) {
        if (state.adventure.currentScene.effectSprites[i].id == effectId) {
            return i;
        }
    }

    return -1;
}

bool AdventureScriptSetEffectVisible(GameState& state, const std::string& effectId, bool visible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 || effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    state.adventure.effectSprites[effectIndex].visible = visible;
    return true;
}

bool AdventureScriptIsEffectVisible(const GameState& state, const std::string& effectId, bool& outVisible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 || effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    outVisible = state.adventure.effectSprites[effectIndex].visible;
    return true;
}

bool AdventureScriptSetEffectOpacity(GameState& state, const std::string& effectId, float opacity)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 || effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    state.adventure.effectSprites[effectIndex].opacity = Clamp(opacity, 0.0f, 1.0f);
    return true;
}

bool AdventureScriptSetEffectTint(GameState& state,
                                  const std::string& effectId,
                                  Color tint)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 ||
        effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    state.adventure.effectSprites[effectIndex].tint = tint;
    return true;
}

static int FindSoundEmitterIndexById(const GameState& state, const std::string& emitterId)
{
    const int count = std::min(
            static_cast<int>(state.adventure.currentScene.soundEmitters.size()),
            static_cast<int>(state.audio.sceneEmitters.size()));

    for (int i = 0; i < count; ++i) {
        if (state.adventure.currentScene.soundEmitters[i].id == emitterId) {
            return i;
        }
    }

    return -1;
}

bool AdventureScriptPlaySound(GameState& state, const std::string& audioId)
{
    if (audioId.empty()) {
        return false;
    }

    return PlaySoundById(state, audioId);
}

bool AdventureScriptStopSound(GameState& state, const std::string& audioId)
{
    if (audioId.empty()) {
        return false;
    }
    return StopSoundById(state, audioId);
}

bool AdventureScriptPlayMusic(GameState& state, const std::string& audioId, float fadeMs)
{
    if (audioId.empty()) {
        return false;
    }

    return PlayMusicById(state, audioId, fadeMs);
}

bool AdventureScriptStopMusic(GameState& state, float fadeMs)
{
    StopMusic(state, fadeMs);
    return true;
}

bool AdventureScriptSetSoundEmitterEnabled(GameState& state, const std::string& emitterId, bool enabled)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int emitterIndex = FindSoundEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.sceneEmitters.size())) {
        return false;
    }

    state.audio.sceneEmitters[emitterIndex].enabled = enabled;
    return true;
}

bool AdventureScriptGetSoundEmitterEnabled(const GameState& state, const std::string& emitterId, bool& outEnabled)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int emitterIndex = FindSoundEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.sceneEmitters.size())) {
        return false;
    }

    outEnabled = state.audio.sceneEmitters[emitterIndex].enabled;
    return true;
}

bool AdventureScriptSetSoundEmitterVolume(GameState& state, const std::string& emitterId, float volume)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int emitterIndex = FindSoundEmitterIndexById(state, emitterId);
    if (emitterIndex < 0 || emitterIndex >= static_cast<int>(state.audio.sceneEmitters.size())) {
        return false;
    }

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    state.audio.sceneEmitters[emitterIndex].volume = volume;
    return true;
}

bool AdventureScriptPlayEmitter(GameState& state, const std::string& emitterId)
{
    if (emitterId.empty()) {
        return false;
    }

    return PlaySoundEmitterById(state, emitterId);
}

bool AdventureScriptStopEmitter(GameState& state, const std::string& emitterId)
{
    if (emitterId.empty()) {
        return false;
    }

    return StopSoundEmitterById(state, emitterId);
}

bool AdventureScriptSetLayerVisible(GameState& state, const std::string& layerName, bool visible)
{
    if (!state.adventure.currentScene.loaded || layerName.empty()) {
        return false;
    }

    SceneImageLayer* layer = FindSceneImageLayerByName(state, layerName);
    if (layer == nullptr) {
        return false;
    }

    layer->visible = visible;
    return true;
}

bool AdventureScriptIsLayerVisible(const GameState& state, const std::string& layerName, bool& outVisible)
{
    if (!state.adventure.currentScene.loaded || layerName.empty()) {
        return false;
    }

    const SceneImageLayer* layer = FindSceneImageLayerByName(state, layerName);
    if (layer == nullptr) {
        return false;
    }

    outVisible = layer->visible;
    return true;
}

bool AdventureScriptSetLayerOpacity(GameState& state, const std::string& layerName, float opacity)
{
    if (!state.adventure.currentScene.loaded || layerName.empty()) {
        return false;
    }

    SceneImageLayer* layer = FindSceneImageLayerByName(state, layerName);
    if (layer == nullptr) {
        return false;
    }

    layer->opacity = Clamp(opacity, 0.0f, 1.0f);
    return true;
}

bool AdventureScriptGetLayerOpacity(const GameState& state, const std::string& layerName, float& outOpacity)
{
    if (!state.adventure.currentScene.loaded || layerName.empty()) {
        return false;
    }

    const SceneImageLayer* layer = FindSceneImageLayerByName(state, layerName);
    if (layer == nullptr) {
        return false;
    }

    outOpacity = layer->opacity;
    return true;
}

bool AdventureScriptCameraFollowControlledActor(GameState& state)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    state.adventure.camera.mode = CameraModeData::FollowControlledActor;
    state.adventure.camera.followedActor = -1;
    state.adventure.camera.moving = false;
    state.adventure.camera.moveElapsedMs = 0.0f;
    state.adventure.camera.moveDurationMs = 0.0f;
    return true;
}

bool AdventureScriptCameraFollowActor(GameState& state, const std::string& actorId)
{
    if (!state.adventure.currentScene.loaded || actorId.empty()) {
        return false;
    }

    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return false;
    }

    const ActorInstance& actor = state.adventure.actors[actorIndex];
    if (!actor.activeInScene || !actor.visible) {
        return false;
    }

    state.adventure.camera.mode = CameraModeData::FollowActor;
    state.adventure.camera.followedActor = actor.handle;
    state.adventure.camera.moving = false;
    state.adventure.camera.moveElapsedMs = 0.0f;
    state.adventure.camera.moveDurationMs = 0.0f;
    return true;
}

bool AdventureScriptSetCameraPosition(GameState& state, Vector2 worldPos)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    CameraData& cam = state.adventure.camera;
    const SceneData& scene = state.adventure.currentScene;

    const float maxX = std::max(0.0f, scene.worldWidth - cam.viewportWidth);
    const float maxY = std::max(0.0f, scene.worldHeight - cam.viewportHeight);

    cam.mode = CameraModeData::Scripted;
    cam.currentBiasShiftX = 0.0f;
    cam.biasLatch = CameraBiasLatch::None;
    cam.followedActor = -1;
    cam.moving = false;
    cam.moveElapsedMs = 0.0f;
    cam.moveDurationMs = 0.0f;

    cam.position.x = Clamp(worldPos.x, 0.0f, maxX);
    cam.position.y = Clamp(worldPos.y, 0.0f, maxY);
    return true;
}

bool AdventureScriptMoveCameraTo(GameState& state,
                                 Vector2 worldPos,
                                 float durationMs,
                                 const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    MoveInterpolation interpolation = MoveInterpolation::AccelerateDecelerate;
    if (!ParseInterpolation(interpolationName, interpolation)) {
        return false;
    }

    CameraData& cam = state.adventure.camera;
    const SceneData& scene = state.adventure.currentScene;

    const float maxX = std::max(0.0f, scene.worldWidth - cam.viewportWidth);
    const float maxY = std::max(0.0f, scene.worldHeight - cam.viewportHeight);

    Vector2 clampedTarget{};
    clampedTarget.x = Clamp(worldPos.x, 0.0f, maxX);
    clampedTarget.y = Clamp(worldPos.y, 0.0f, maxY);

    cam.biasLatch = CameraBiasLatch::None;
    cam.currentBiasShiftX = 0.0f;
    cam.mode = CameraModeData::Scripted;
    cam.followedActor = -1;
    cam.interpolation = interpolation;

    if (durationMs <= 0.0f) {
        cam.moving = false;
        cam.moveStart = clampedTarget;
        cam.moveTarget = clampedTarget;
        cam.moveElapsedMs = 0.0f;
        cam.moveDurationMs = 0.0f;
        cam.position = clampedTarget;
        return true;
    }

    cam.moving = true;
    cam.moveStart = cam.position;
    cam.moveTarget = clampedTarget;
    cam.moveElapsedMs = 0.0f;
    cam.moveDurationMs = durationMs;
    return true;
}


static Vector2 MakeCameraTopLeftForCenterTarget(const CameraData& cam, Vector2 centerWorldPos)
{
    Vector2 topLeft{};
    topLeft.x = centerWorldPos.x - cam.viewportWidth * 0.5f;
    topLeft.y = centerWorldPos.y - cam.viewportHeight * 0.5f;
    return topLeft;
}

static bool GetActorCameraCenterTarget(const GameState& state,
                                       const ActorInstance& actor,
                                       Vector2& outCenterWorldPos)
{
    const CameraData& cam = state.adventure.camera;

    Rectangle actorRect = GetActorScreenRect(state, actor);
    outCenterWorldPos.x = actorRect.x + (0.5f * actorRect.width) + cam.position.x;
    outCenterWorldPos.y = actorRect.y + (0.5f * actorRect.height) + cam.position.y;
    return true;
}

static bool GetPropCameraCenterTarget(const GameState& state,
                                      const ScenePropData& sceneProp,
                                      const PropInstance& prop,
                                      Vector2& outCenterWorldPos)
{
    const CameraData& cam = state.adventure.camera;

    Rectangle propRect = GetPropScreenRect(state, sceneProp, prop);
    outCenterWorldPos.x = propRect.x + (0.5f * propRect.width) + cam.position.x;
    outCenterWorldPos.y = propRect.y + (0.5f * propRect.height) + cam.position.y;
    return true;
}

bool AdventureScriptCenterCameraOn(GameState& state, Vector2 worldPos)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const Vector2 cameraTopLeft =
            MakeCameraTopLeftForCenterTarget(state.adventure.camera, worldPos);

    return AdventureScriptSetCameraPosition(state, cameraTopLeft);
}

bool AdventureScriptMoveCameraCenterTo(GameState& state,
                                       Vector2 worldPos,
                                       float durationMs,
                                       const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const Vector2 cameraTopLeft =
            MakeCameraTopLeftForCenterTarget(state.adventure.camera, worldPos);

    return AdventureScriptMoveCameraTo(
            state,
            cameraTopLeft,
            durationMs,
            interpolationName);
}

bool AdventureScriptPanCameraToActor(GameState& state,
                                     const std::string& actorId,
                                     float durationMs,
                                     const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded || actorId.empty()) {
        return false;
    }

    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return false;
    }

    const ActorInstance& actor = state.adventure.actors[actorIndex];
    if (!actor.activeInScene || !actor.visible) {
        return false;
    }

    Vector2 centerWorldPos{};
    GetActorCameraCenterTarget(state, actor, centerWorldPos);

    return AdventureScriptMoveCameraCenterTo(
            state,
            centerWorldPos,
            durationMs,
            interpolationName);
}

bool AdventureScriptPanCameraToProp(GameState& state,
                                    const std::string& propId,
                                    float durationMs,
                                    const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded || propId.empty()) {
        return false;
    }

    const int scenePropIndex = AdventureFindScenePropIndexById(state, propId);
    if (scenePropIndex < 0 ||
        scenePropIndex >= static_cast<int>(state.adventure.currentScene.props.size()) ||
        scenePropIndex >= static_cast<int>(state.adventure.props.size())) {
        return false;
    }

    const ScenePropData& sceneProp = state.adventure.currentScene.props[scenePropIndex];
    const PropInstance& prop = state.adventure.props[scenePropIndex];

    if (!prop.visible) {
        return false;
    }

    Vector2 centerWorldPos{};
    GetPropCameraCenterTarget(state, sceneProp, prop, centerWorldPos);

    return AdventureScriptMoveCameraCenterTo(
            state,
            centerWorldPos,
            durationMs,
            interpolationName);
}

bool AdventureScriptPanCameraToHotspot(GameState& state,
                                       const std::string& hotspotId,
                                       float durationMs,
                                       const std::string& interpolationName)
{
    if (!state.adventure.currentScene.loaded || hotspotId.empty()) {
        return false;
    }

    for (const SceneHotspot& hotspot : state.adventure.currentScene.hotspots) {
        if (hotspot.id == hotspotId) {
            return AdventureScriptMoveCameraCenterTo(
                    state,
                    hotspot.walkTo,
                    durationMs,
                    interpolationName);
        }
    }

    return false;
}

bool AdventureScriptShakeScreen(GameState& state,
                                float durationMs,
                                float strengthPx,
                                float frequencyHz,
                                bool smooth)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    if (durationMs <= 0.0f) {
        return false;
    }

    if (strengthPx <= 0.0f) {
        return false;
    }

    if (frequencyHz <= 0.0f) {
        frequencyHz = 30.0f;
    }

    ScreenShakeState& shake = state.adventure.screenShake;

    if (!shake.active) {
        shake.active = true;
        shake.durationMs = durationMs;
        shake.elapsedMs = 0.0f;
        shake.strengthX = strengthPx;
        shake.strengthY = strengthPx;
        shake.frequencyHz = frequencyHz;
        shake.sampleTimerMs = 0.0f;
        shake.smooth = smooth;
        shake.previousOffset = Vector2{0.0f, 0.0f};
        shake.sampledOffset = Vector2{0.0f, 0.0f};
        shake.currentOffset = Vector2{0.0f, 0.0f};
        return true;
    }

    shake.active = true;
    shake.durationMs = durationMs;
    shake.elapsedMs = 0.0f;
    shake.strengthX = std::max(shake.strengthX, strengthPx);
    shake.strengthY = std::max(shake.strengthY, strengthPx);
    shake.frequencyHz = std::max(shake.frequencyHz, frequencyHz);
    shake.sampleTimerMs = 0.0f;
    shake.smooth = smooth;

    return true;
}

static int FindEffectRegionIndexById(const GameState& state, const std::string& effectRegionId)
{
    const int count = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < count; ++i) {
        if (state.adventure.currentScene.effectRegions[i].id == effectRegionId) {
            return i;
        }
    }

    return -1;
}

bool AdventureScriptSetEffectRegionVisible(GameState& state, const std::string& effectRegionId, bool visible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectRegionIndex = FindEffectRegionIndexById(state, effectRegionId);
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
        return false;
    }

    state.adventure.effectRegions[effectRegionIndex].visible = visible;
    return true;
}

bool AdventureScriptIsEffectRegionVisible(const GameState& state, const std::string& effectRegionId, bool& outVisible)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectRegionIndex = FindEffectRegionIndexById(state, effectRegionId);
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
        return false;
    }

    outVisible = state.adventure.effectRegions[effectRegionIndex].visible;
    return true;
}

bool AdventureScriptSetEffectRegionOpacity(GameState& state, const std::string& effectRegionId, float opacity)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectRegionIndex = FindEffectRegionIndexById(state, effectRegionId);
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
        return false;
    }

    state.adventure.effectRegions[effectRegionIndex].opacity = Clamp(opacity, 0.0f, 1.0f);
    return true;
}

bool AdventureScriptGetEffectRegionOpacity(const GameState& state, const std::string& effectRegionId, float& outOpacity)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectRegionIndex = FindEffectRegionIndexById(state, effectRegionId);
    if (effectRegionIndex < 0 ||
        effectRegionIndex >= static_cast<int>(state.adventure.effectRegions.size())) {
        return false;
    }

    outOpacity = state.adventure.effectRegions[effectRegionIndex].opacity;
    return true;
}

bool AdventureScriptGetEffectOpacity(const GameState& state, const std::string& effectId, float& outOpacity)
{
    if (!state.adventure.currentScene.loaded) {
        return false;
    }

    const int effectIndex = FindEffectSpriteIndexById(state, effectId);
    if (effectIndex < 0 ||
        effectIndex >= static_cast<int>(state.adventure.effectSprites.size())) {
        return false;
    }

    outOpacity = state.adventure.effectSprites[effectIndex].opacity;
    return true;
}

bool AdventureScriptSetActorPosition(GameState& state, const std::string& actorId, Vector2 worldPos)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 ||
        actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        TraceLog(LOG_WARNING, "setActorPosition: actor not found: %s", actorId.c_str());
        return false;
    }

    ActorInstance& actor = state.adventure.actors[actorIndex];
    actor.feetPos = worldPos;
    actor.path = {};
    actor.animationTimeMs = 0.0f;
    actor.stoppedTimeMs = actor.idleDelayMs;
    actor.inIdleState = true;

    const int controlledActorIndex = GetControlledActorIndex(state);
    if (actorIndex == controlledActorIndex) {
        state.adventure.pendingInteraction = {};
    }

    return true;
}

bool AdventureScriptGetActorPosition(const GameState& state, const std::string& actorId, Vector2& outWorldPos)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (actorIndex < 0 ||
        actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        TraceLog(LOG_WARNING, "getActorPosition: actor not found: %s", actorId.c_str());
        return false;
    }

    const ActorInstance& actor = state.adventure.actors[actorIndex];
    outWorldPos = actor.feetPos;
    return true;
}

bool AdventureScriptGetPropPosition(const GameState& state, const std::string& propId, Vector2& outWorldPos)
{
    const int propIndex = AdventureFindScenePropIndexById(state, propId);
    if (propIndex < 0 ||
        propIndex >= static_cast<int>(state.adventure.props.size())) {
        TraceLog(LOG_WARNING, "getPropPosition: prop not found: %s", propId.c_str());
        return false;
    }

    outWorldPos = state.adventure.props[propIndex].feetPos;
    return true;
}

bool AdventureScriptGetHotspotInteractionPosition(const GameState& state,
                                                  const std::string& hotspotId,
                                                  Vector2& outWorldPos)
{
    for (const SceneHotspot& hotspot : state.adventure.currentScene.hotspots) {
        if (hotspot.id == hotspotId) {
            outWorldPos = hotspot.walkTo;
            return true;
        }
    }

    TraceLog(LOG_WARNING, "getHotspotInteractionPosition: hotspot not found: %s", hotspotId.c_str());
    return false;
}