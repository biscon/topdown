#include "topdown/CharacterRender.h"

#include <cmath>
#include <algorithm>
#include <vector>

#include "resources/AsepriteAsset.h"
#include "resources/TextureAsset.h"
#include "rlgl.h"
#include "raymath.h"
#include "TopdownHelpers.h"
#include "NpcRegistry.h"



static Vector2 BuildSpriteFrameOrigin(
        const SpriteAssetResource& asset,
        const SpriteFrame& frame,
        float drawScale)
{
    Vector2 origin{};

    if (asset.hasExplicitOrigin) {
        origin.x = (asset.origin.x - frame.spriteSourcePos.x) * drawScale;
        origin.y = (asset.origin.y - frame.spriteSourcePos.y) * drawScale;
    } else {
        origin.x = (frame.sourceSize.x * 0.5f - frame.spriteSourcePos.x) * drawScale;
        origin.y = (frame.sourceSize.y * 0.5f - frame.spriteSourcePos.y) * drawScale;
    }

    return origin;
}

static void DrawCenteredSpriteFrame(
        const GameState& state,
        SpriteAssetHandle handle,
        float animationTimeMs,
        Vector2 worldPos,
        float rotationRadians)
{
    const SpriteAssetResource* asset = FindSpriteAssetResource(state.resources, handle);
    if (asset == nullptr || !asset->loaded || asset->textureHandle < 0 || asset->clips.empty()) {
        return;
    }

    const TextureResource* tex = FindTextureResource(state.resources, asset->textureHandle);
    if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
        return;
    }

    const SpriteClip& clip = asset->clips[0];
    const int frameIndex = GetLoopingFrameIndex(*asset, clip, animationTimeMs);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(asset->frames.size())) {
        return;
    }

    const SpriteFrame& frame = asset->frames[frameIndex];
    const float drawScale = asset->baseDrawScale;

    const Vector2 screenPos = TopdownWorldToScreen(state, worldPos);

    Rectangle src = frame.sourceRect;

    Rectangle dst{};
    dst.x = screenPos.x;
    dst.y = screenPos.y;
    dst.width = frame.sourceRect.width * drawScale;
    dst.height = frame.sourceRect.height * drawScale;

    const Vector2 origin = BuildSpriteFrameOrigin(*asset, frame, drawScale);

    DrawTexturePro(
            tex->texture,
            src,
            dst,
            origin,
            rotationRadians * RAD2DEG,
            WHITE);
}

static void DrawCenteredSpriteFrameOneShot(
        const GameState& state,
        SpriteAssetHandle handle,
        float animationTimeMs,
        Vector2 worldPos,
        float rotationRadians)
{
    const SpriteAssetResource* asset = FindSpriteAssetResource(state.resources, handle);
    if (asset == nullptr || !asset->loaded || asset->textureHandle < 0 || asset->clips.empty()) {
        return;
    }

    const TextureResource* tex = FindTextureResource(state.resources, asset->textureHandle);
    if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
        return;
    }

    const SpriteClip& clip = asset->clips[0];
    const int frameIndex = GetOneShotFrameIndex(*asset, clip, animationTimeMs);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(asset->frames.size())) {
        return;
    }

    const SpriteFrame& frame = asset->frames[frameIndex];
    const float drawScale = asset->baseDrawScale;

    const Vector2 screenPos = TopdownWorldToScreen(state, worldPos);

    Rectangle src = frame.sourceRect;

    Rectangle dst{};
    dst.x = screenPos.x;
    dst.y = screenPos.y;
    dst.width = frame.sourceRect.width * drawScale;
    dst.height = frame.sourceRect.height * drawScale;

    const Vector2 origin = BuildSpriteFrameOrigin(*asset, frame, drawScale);

    DrawTexturePro(
            tex->texture,
            src,
            dst,
            origin,
            rotationRadians * RAD2DEG,
            WHITE);
}

static void DrawCenteredSpriteClipFrame(
        const GameState& state,
        SpriteAssetHandle handle,
        int clipIndex,
        float animationTimeMs,
        Vector2 worldPos,
        float rotationRadians,
        Color tint = WHITE)
{
    const SpriteAssetResource* asset = FindSpriteAssetResource(state.resources, handle);
    if (asset == nullptr || !asset->loaded || asset->textureHandle < 0) {
        return;
    }

    if (clipIndex < 0 || clipIndex >= static_cast<int>(asset->clips.size())) {
        return;
    }

    const TextureResource* tex = FindTextureResource(state.resources, asset->textureHandle);
    if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
        return;
    }

    const SpriteClip& clip = asset->clips[clipIndex];
    const int frameIndex = GetLoopingFrameIndex(*asset, clip, animationTimeMs);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(asset->frames.size())) {
        return;
    }

    const SpriteFrame& frame = asset->frames[frameIndex];
    const float drawScale = asset->baseDrawScale;
    const Vector2 screenPos = TopdownWorldToScreen(state, worldPos);

    Rectangle src = frame.sourceRect;

    Rectangle dst{};
    dst.x = screenPos.x;
    dst.y = screenPos.y;
    dst.width = frame.sourceRect.width * drawScale;
    dst.height = frame.sourceRect.height * drawScale;

    const Vector2 origin = BuildSpriteFrameOrigin(*asset, frame, drawScale);

    DrawTexturePro(
            tex->texture,
            src,
            dst,
            origin,
            rotationRadians * RAD2DEG,
            tint);
}

static void DrawCenteredSpriteClipFrameOneShot(
        const GameState& state,
        SpriteAssetHandle handle,
        int clipIndex,
        float animationTimeMs,
        Vector2 worldPos,
        float rotationRadians,
        Color tint = WHITE)
{
    const SpriteAssetResource* asset = FindSpriteAssetResource(state.resources, handle);
    if (asset == nullptr || !asset->loaded || asset->textureHandle < 0) {
        return;
    }

    if (clipIndex < 0 || clipIndex >= static_cast<int>(asset->clips.size())) {
        return;
    }

    const TextureResource* tex = FindTextureResource(state.resources, asset->textureHandle);
    if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
        return;
    }

    const SpriteClip& clip = asset->clips[clipIndex];
    const int frameIndex = GetOneShotFrameIndex(*asset, clip, animationTimeMs);
    if (frameIndex < 0 || frameIndex >= static_cast<int>(asset->frames.size())) {
        return;
    }

    const SpriteFrame& frame = asset->frames[frameIndex];
    const float drawScale = asset->baseDrawScale;
    const Vector2 screenPos = TopdownWorldToScreen(state, worldPos);

    Rectangle src = frame.sourceRect;

    Rectangle dst{};
    dst.x = screenPos.x;
    dst.y = screenPos.y;
    dst.width = frame.sourceRect.width * drawScale;
    dst.height = frame.sourceRect.height * drawScale;

    const Vector2 origin = BuildSpriteFrameOrigin(*asset, frame, drawScale);

    DrawTexturePro(
            tex->texture,
            src,
            dst,
            origin,
            rotationRadians * RAD2DEG,
            tint);
}

// probably be cheaper to just render a multiply circle gradient texture
static void DrawBlobShadowRotated(
        const GameState& state,
        Vector2 worldPos,
        Vector2 facing,
        float rotationRadians,
        float scale,
        float projectionDist,
        float sideOffset,
        unsigned char alpha = 140) // Positive is Right, Negative is Left
{
    // 1. Calculate Perpendicular "Right" vector from facing
    // In Raylib/2D: Right = {-facing.y, facing.x}
    Vector2 right = { -facing.y, facing.x };

    // 2. Calculate Final World Position
    // Position = Base + (Forward * dist) + (Right * sideOffset)
    Vector2 forwardOffset = Vector2Scale(facing, projectionDist);
    Vector2 sideShift = Vector2Scale(right, sideOffset);

    Vector2 projectedWorldPos = Vector2Add(worldPos, Vector2Add(forwardOffset, sideShift));
    Vector2 screenPos = TopdownWorldToScreen(state, projectedWorldPos);

    const float assetScale = 1.0f;
    const float baseRadius = 45.0f * assetScale * scale;
    float rotationDegrees = rotationRadians * RAD2DEG;

    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);

    rlPushMatrix();
    rlTranslatef(std::round(screenPos.x), std::round(screenPos.y), 0);
    rlRotatef(rotationDegrees, 0, 0, 1);
    rlScalef(1.0f, 0.75f, 1.0f);

    Color centerColor = { 0, 0, 0, alpha };
    Color outerColor  = { 0, 0, 0, 0 };

    DrawCircleGradient(0, 0, baseRadius, centerColor, outerColor);
    rlPopMatrix();

    rlDrawRenderBatchActive();
    EndBlendMode();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
}

void TopdownRenderPlayerCharacter(GameState& state)
{
    const TopdownCharacterRuntime& runtime = state.topdown.runtime.playerCharacter;
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    if (!runtime.active) {
        return;
    }

    DrawBlobShadowRotated(state, player.position, player.facing, runtime.upperRotationRadians, 1.25, 0, 0);

    DrawCenteredSpriteFrame(
            state,
            runtime.currentFeetHandle,
            runtime.feetAnimationTimeMs,
            player.position,
            runtime.feetRotationRadians);

    const TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    if (attack.active) {
        DrawCenteredSpriteFrameOneShot(
                state,
                runtime.currentUpperHandle,
                attack.stateTimeMs,
                player.position,
                runtime.upperRotationRadians);
    } else {
        DrawCenteredSpriteFrame(
                state,
                runtime.currentUpperHandle,
                runtime.upperAnimationTimeMs,
                player.position,
                runtime.upperRotationRadians);
    }
}

void TopdownRenderNpcs(GameState& state)
{
    std::vector<const TopdownNpcRuntime*> sortedNpcs;
    sortedNpcs.reserve(state.topdown.runtime.npcs.size());

    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (npc.dead || npc.corpse) {
            sortedNpcs.push_back(&npc);
        }
    }

    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.dead && !npc.corpse) {
            sortedNpcs.push_back(&npc);
        }
    }

    for (const TopdownNpcRuntime* npcPtr : sortedNpcs) {
        if (npcPtr == nullptr) {
            continue;
        }

        const TopdownNpcRuntime& npc = *npcPtr;
        if (!npc.active || !npc.visible) {
            continue;
        }

        const TopdownNpcAssetRuntime* asset =
                FindTopdownNpcAssetRuntime(state, npc.assetId);

        if (asset == nullptr || !asset->loaded) {
            continue;
        }

        const TopdownNpcClipRef* resolvedClip = TopdownGetResolvedNpcAnimationClip(npc);
        if (resolvedClip == nullptr || !TopdownNpcClipRefIsValid(*resolvedClip)) {
            continue;
        }

        unsigned char alpha = 255;

        if (npc.corpse && npc.corpseExpirationMs >= 0.0f) {
            const float fadeStartMs = npc.corpseExpirationMs;
            if (npc.corpseElapsedMs > fadeStartMs) {
                const float fadeT = std::clamp(
                        (npc.corpseElapsedMs - fadeStartMs) / kCorpseFadeDurationMs,
                        0.0f,
                        1.0f);
                alpha = static_cast<unsigned char>(std::round(255.0f * (1.0f - fadeT)));
            }
        }

        if (alpha == 0) {
            continue;
        }

        const unsigned char shadowAlpha =
                static_cast<unsigned char>(std::round((140.0f / 255.0f) * alpha));

        DrawBlobShadowRotated(
                state,
                npc.position,
                npc.facing,
                npc.rotationRadians,
                1.0f,
                8.0f,
                0.0f,
                shadowAlpha);

        Color tint = {
                static_cast<unsigned char>(alpha),
                static_cast<unsigned char>(alpha),
                static_cast<unsigned char>(alpha),
                static_cast<unsigned char>(alpha)
        };

        if (npc.oneShotActive &&
            resolvedClip->spriteHandle == npc.oneShotClip.spriteHandle &&
            resolvedClip->clipIndex == npc.oneShotClip.clipIndex) {
            DrawCenteredSpriteClipFrameOneShot(
                    state,
                    resolvedClip->spriteHandle,
                    resolvedClip->clipIndex,
                    npc.oneShotTimeMs,
                    npc.position,
                    npc.rotationRadians,
                    tint);
        } else if (npc.animationMode == TopdownNpcAnimationMode::ScriptLoop &&
                   resolvedClip->spriteHandle == npc.scriptLoopClip.spriteHandle &&
                   resolvedClip->clipIndex == npc.scriptLoopClip.clipIndex) {
            DrawCenteredSpriteClipFrame(
                    state,
                    resolvedClip->spriteHandle,
                    resolvedClip->clipIndex,
                    npc.scriptLoopTimeMs,
                    npc.position,
                    npc.rotationRadians,
                    tint);
        } else {
            DrawCenteredSpriteClipFrame(
                    state,
                    resolvedClip->spriteHandle,
                    resolvedClip->clipIndex,
                    npc.automaticLoopTimeMs,
                    npc.position,
                    npc.rotationRadians,
                    tint);
        }
    }
}
