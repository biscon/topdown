#include "AdventureInternal.h"

#include <cmath>
#include <algorithm>

#include "adventure/AdventureHelpers.h"
#include "nav/NavMeshQuery.h"
#include "resources/AsepriteAsset.h"
#include "raylib.h"

float AdventureLength(Vector2 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Vector2 AdventureNormalizeOrZero(Vector2 v)
{
    const float len = AdventureLength(v);
    if (len <= 0.0001f) {
        return Vector2{0.0f, 0.0f};
    }
    return Vector2{v.x / len, v.y / len};
}

void AdventureSwitchToDirectionalIdle(ActorInstance& actor)
{
    switch (actor.facing) {
        case ActorFacing::Left:
            actor.currentAnimation = "idle_right";
            actor.flipX = true;
            break;
        case ActorFacing::Right:
            actor.currentAnimation = "idle_right";
            actor.flipX = false;
            break;
        case ActorFacing::Back:
            actor.currentAnimation = "idle_back";
            actor.flipX = false;
            break;
        case ActorFacing::Front:
        default:
            actor.currentAnimation = "idle_front";
            actor.flipX = false;
            break;
    }

    actor.animationTimeMs = 0.0f;
}

void AdventureForceFacing(ActorInstance& actor, SceneFacing facing)
{
    switch (facing) {
        case SceneFacing::Left:
            actor.facing = ActorFacing::Left;
            actor.flipX = true;
            actor.currentAnimation = "idle_right";
            break;

        case SceneFacing::Right:
            actor.facing = ActorFacing::Right;
            actor.flipX = false;
            actor.currentAnimation = "idle_right";
            break;

        case SceneFacing::Back:
            actor.facing = ActorFacing::Back;
            actor.flipX = false;
            actor.currentAnimation = "idle_back";
            break;

        case SceneFacing::Front:
        default:
            actor.facing = ActorFacing::Front;
            actor.flipX = false;
            actor.currentAnimation = "idle_front";
            break;
    }

    actor.animationTimeMs = 0.0f;
    actor.inIdleState = true;
    actor.stoppedTimeMs = actor.idleDelayMs;
}

ActorInstance* AdventureFindSceneActorById(GameState& state, const std::string& actorId, int* outActorIndex)
{
    const int actorIndex = FindActorInstanceIndexById(state, actorId);
    if (outActorIndex != nullptr) {
        *outActorIndex = actorIndex;
    }

    if (actorIndex < 0 || actorIndex >= static_cast<int>(state.adventure.actors.size())) {
        return nullptr;
    }

    ActorInstance& actor = state.adventure.actors[actorIndex];
    if (!actor.activeInScene || !actor.visible) {
        return nullptr;
    }

    return &actor;
}

const ActorDefinitionData* AdventureGetActorDefinitionForInstance(const GameState& state, const ActorInstance& actor)
{
    return FindActorDefinitionByIndex(state, actor.actorDefIndex);
}

bool AdventureQueueActorPathToPoint(
        GameState& state,
        ActorInstance& actor,
        Vector2 walkTarget,
        bool fastMove)
{
    const NavMeshData& navMesh = state.adventure.currentScene.navMesh;

    std::vector<Vector2> pathPoints;
    std::vector<int> trianglePath;
    Vector2 resolvedEnd{};

    const bool ok = BuildNavPath(
            navMesh,
            actor.feetPos,
            walkTarget,
            pathPoints,
            &trianglePath,
            &resolvedEnd);

    if (!ok) {
        actor.path = {};
        return false;
    }

    actor.path.points = pathPoints;
    actor.path.currentPoint = 0;
    actor.path.active = !actor.path.points.empty();
    actor.path.fastMove = fastMove;

    actor.stoppedTimeMs = 0.0f;
    actor.inIdleState = false;
    actor.animationTimeMs = 0.0f;

    return true;
}

float AdventureComputeSpeechDurationMs(const std::string& text, int overrideDurationMs)
{
    if (overrideDurationMs >= 0) {
        return static_cast<float>(overrideDurationMs);
    }

    /*
    const float durationMs = 1000.0f + static_cast<float>(text.size()) * 45.0f;
    return std::clamp(durationMs, 1800.0f, 7000.0f);
    */

    const float durationMs = 750.0f + static_cast<float>(text.size()) * 45.0f;
    return std::clamp(durationMs, 1400.0f, 7000.0f);
}

void AdventureStartSpeech(
        GameState& state,
        SpeechAnchorType anchorType,
        int actorIndex,
        int propIndex,
        Vector2 worldPos,
        const std::string& text,
        Color color,
        int durationMs,
        bool skippable)
{
    state.adventure.speechUi = {};
    state.adventure.speechUi.active = true;
    state.adventure.speechUi.anchorType = anchorType;
    state.adventure.speechUi.propIndex = propIndex;
    state.adventure.speechUi.actorIndex = actorIndex;
    state.adventure.speechUi.worldPos = worldPos;
    state.adventure.speechUi.text = text;
    state.adventure.speechUi.color = color;
    state.adventure.speechUi.timerMs = 0.0f;
    state.adventure.speechUi.durationMs = AdventureComputeSpeechDurationMs(text, durationMs);
    state.adventure.speechUi.skippable = skippable;
    state.adventure.speechUi.fadeInMs = 50.0f;
    state.adventure.speechUi.fadeOutMs = 50.0f;
}

void AdventureStartAmbientSpeech(
        GameState& state,
        SpeechAnchorType anchorType,
        int actorIndex,
        int propIndex,
        Vector2 worldPos,
        const std::string& text,
        Color color,
        int durationMs)
{
    constexpr size_t MAX_AMBIENT_SPEECH = 6;

    if (state.adventure.ambientSpeechUis.size() >= MAX_AMBIENT_SPEECH) {
        state.adventure.ambientSpeechUis.erase(state.adventure.ambientSpeechUis.begin());
    }

    SpeechUiState speech{};
    speech.active = true;
    speech.anchorType = anchorType;
    speech.propIndex = propIndex;
    speech.actorIndex = actorIndex;
    speech.worldPos = worldPos;
    speech.text = text;
    speech.color = color;
    speech.timerMs = 0.0f;

    const float baseDurationMs = AdventureComputeSpeechDurationMs(text, durationMs);
    speech.fadeInMs = 50.0f;
    speech.fadeOutMs = 50.0f;
    speech.durationMs = baseDurationMs + speech.fadeInMs + speech.fadeOutMs;
    speech.skippable = false;

    state.adventure.ambientSpeechUis.push_back(speech);
}

bool AdventureTryGetSpriteAnimationDurationMs(
        const GameState& state,
        SpriteAssetHandle spriteAssetHandle,
        const std::string& animationName,
        float& outDurationMs)
{
    outDurationMs = 0.0f;

    if (spriteAssetHandle < 0) {
        return false;
    }

    const SpriteAssetResource* asset =
            FindSpriteAssetResource(state.resources, spriteAssetHandle);
    if (asset == nullptr || !asset->loaded) {
        return false;
    }

    for (const std::string& layerName : asset->layerNames) {
        const int clipIndex = FindClipIndex(*asset, layerName, animationName);
        if (clipIndex < 0) {
            continue;
        }

        const SpriteClip& clip = asset->clips[clipIndex];
        const float totalMs = GetOneShotClipDurationMs(*asset, clip);
        if (totalMs > 0.0f) {
            outDurationMs = totalMs;
            return true;
        }
    }

    return false;
}

int AdventureFindScenePropIndexById(const GameState& state, const std::string& propId)
{
    const auto& props = state.adventure.currentScene.props;
    for (int i = 0; i < static_cast<int>(props.size()); ++i) {
        if (props[i].id == propId) {
            return i;
        }
    }
    return -1;
}

void AdventureQueueLoadSceneInternal(GameState& state, const char* sceneId, const char* spawnId)
{
    state.adventure.pendingSceneId = sceneId;
    state.adventure.pendingSpawnId = (spawnId != nullptr) ? spawnId : "";
    state.adventure.hasPendingSceneLoad = true;

    if (state.adventure.currentScene.loaded) {
        SceneFadeState& fade = state.adventure.sceneFade;
        if (fade.phase == SceneFadePhase::None) {
            fade.phase = SceneFadePhase::FadingOut;
            fade.durationMs = 300.0f;
            fade.elapsedMs = 0.0f;
            fade.opacity = 0.0f;
            fade.loadTriggered = false;
            state.adventure.fadeInputBlocked = true;
        }
    }
}

