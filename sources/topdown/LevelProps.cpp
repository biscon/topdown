#include "topdown/LevelProps.h"

#include <algorithm>

#include "raymath.h"
#include "resources/AsepriteAsset.h"
#include "resources/TextureAsset.h"
#include "topdown/TopdownHelpers.h"

TopdownRuntimeProp* FindProp(GameState& state, const std::string& id)
{
    for (TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (prop.active && prop.id == id) {
            return &prop;
        }
    }

    return nullptr;
}

const TopdownRuntimeProp* FindProp(const GameState& state, const std::string& id)
{
    for (const TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (prop.active && prop.id == id) {
            return &prop;
        }
    }

    return nullptr;
}

static int FindPropClipIndexByAnimationName(const SpriteAssetResource& sprite, const std::string& animation)
{
    if (animation.empty()) {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(sprite.clips.size()); ++i) {
        if (sprite.clips[i].name == animation) {
            return i;
        }
    }

    return -1;
}

static std::vector<int>& GetPropBucket(GameState& state, TopdownEffectPlacement placement)
{
    if (placement == TopdownEffectPlacement::AfterCharacters) return state.topdown.runtime.render.afterCharactersPropIndices;
    if (placement == TopdownEffectPlacement::Final) return state.topdown.runtime.render.finalPropIndices;
    return state.topdown.runtime.render.afterBottomPropIndices;
}

void TopdownUpdateProps(GameState& state, float dt)
{
    const float dtMs = dt * 1000.0f;
    for (TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (!prop.active) {
            continue;
        }

        prop.animationTimeMs += dtMs;

        if (prop.oneShotActive &&
            prop.oneShotDurationMs > 0.0f &&
            prop.animationTimeMs >= prop.oneShotDurationMs) {
            prop.oneShotActive = false;
            prop.currentAnimation = prop.baseAnimation;
            prop.animationTimeMs = 0.0f;
        }

        if (prop.moving) {
            prop.moveTimerMs += dtMs;

            const float durationMs = std::max(prop.moveDurationMs, 0.0001f);
            float t = Clamp(prop.moveTimerMs / durationMs, 0.0f, 1.0f);
            t = ApplyInterpolation(prop.moveInterpolation, t);

            prop.position = Vector2Lerp(prop.moveStart, prop.moveEnd, t);

            if (prop.moveTimerMs >= prop.moveDurationMs) {
                prop.position = prop.moveEnd;
                prop.moving = false;
            }
        }
    }
}

void TopdownRenderProps(GameState& state, TopdownEffectPlacement placement)
{
    std::vector<int>& bucket = GetPropBucket(state, placement);

    for (int index : bucket) {
        if (index < 0 || index >= static_cast<int>(state.topdown.runtime.props.size())) {
            continue;
        }

        const TopdownRuntimeProp& prop = state.topdown.runtime.props[index];
        if (!prop.active || !prop.visible) {
            continue;
        }

        const Vector2 screen = TopdownWorldToScreen(state, prop.position);

        Color tint = WHITE;
        tint.a = static_cast<unsigned char>(std::round(255.0f * Clamp(prop.opacity, 0.0f, 1.0f)));

        if (prop.type == TopdownPropType::Image) {
            const TextureResource* texture = FindTextureResource(state.resources, prop.textureHandle);
            if (texture == nullptr || !texture->loaded || texture->texture.id == 0) {
                continue;
            }

            Rectangle src{
                    0.0f,
                    0.0f,
                    static_cast<float>(texture->texture.width),
                    static_cast<float>(texture->texture.height)
            };

            if (prop.flipX) {
                src.width = -src.width;
            }

            const Vector2 origin = prop.hasOriginOverride
                    ? prop.originOverride
                    : Vector2{src.width * 0.5f, src.height * 0.5f};

            Rectangle dst{
                    std::round(screen.x),
                    std::round(screen.y),
                    std::fabs(src.width),
                    std::fabs(src.height)
            };

            DrawTexturePro(texture->texture, src, dst, origin, 0.0f, tint);
            continue;
        }

        const SpriteAssetResource* sprite = FindSpriteAssetResource(state.resources, prop.spriteHandle);
        if (sprite == nullptr || !sprite->loaded || sprite->frames.empty()) {
            continue;
        }

        const TextureResource* spriteTexture = FindTextureResource(state.resources, sprite->textureHandle);
        if (spriteTexture == nullptr || !spriteTexture->loaded || spriteTexture->texture.id == 0) {
            continue;
        }

        const std::string animationName = prop.oneShotActive ? prop.oneShotAnimation : prop.currentAnimation;
        const int clipIndex = FindPropClipIndexByAnimationName(*sprite, animationName);
        if (clipIndex < 0 || clipIndex >= static_cast<int>(sprite->clips.size())) {
            continue;
        }

        const SpriteClip& clip = sprite->clips[clipIndex];
        const int frameIndex = prop.oneShotActive
                ? GetOneShotFrameIndex(*sprite, clip, prop.animationTimeMs)
                : GetLoopingFrameIndex(*sprite, clip, prop.animationTimeMs);

        if (frameIndex < 0 || frameIndex >= static_cast<int>(sprite->frames.size())) {
            continue;
        }

        const SpriteFrame& frame = sprite->frames[frameIndex];

        Rectangle src{
                static_cast<float>(frame.sourceRect.x),
                static_cast<float>(frame.sourceRect.y),
                static_cast<float>(frame.sourceRect.width),
                static_cast<float>(frame.sourceRect.height)
        };

        if (prop.flipX) {
            src.width = -src.width;
        }

        const Vector2 origin = prop.hasOriginOverride
                ? prop.originOverride
                : Vector2{static_cast<float>(frame.sourceRect.width) * 0.5f,
                          static_cast<float>(frame.sourceRect.height) * 0.5f};

        Rectangle dst{
                std::round(screen.x),
                std::round(screen.y),
                std::fabs(static_cast<float>(frame.sourceRect.width)),
                std::fabs(static_cast<float>(frame.sourceRect.height))
        };

        DrawTexturePro(spriteTexture->texture, src, dst, origin, 0.0f, tint);
    }
}
