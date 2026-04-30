#include "topdown/LevelProps.h"

#include <cmath>

#include "raylib.h"
#include "raymath.h"
#include "resources/AsepriteAsset.h"
#include "resources/TextureAsset.h"
#include "topdown/TopdownHelpers.h"

static const std::vector<int>* GetPropBucket(const TopdownRenderWorld& render, TopdownEffectPlacement placement)
{
    switch (placement) {
        case TopdownEffectPlacement::AfterBottom:
            return &render.afterBottomPropIndices;

        case TopdownEffectPlacement::AfterCharacters:
            return &render.afterCharactersPropIndices;

        case TopdownEffectPlacement::Final:
            return &render.finalPropIndices;
    }

    return nullptr;
}

static const SpriteClip* FindClipByName(const SpriteAssetResource& sprite, const std::string& clipName)
{
    for (const SpriteClip& clip : sprite.clips) {
        if (clip.name == clipName) {
            return &clip;
        }
    }

    return nullptr;
}

void TopdownUpdateProps(GameState& state, float dt)
{
    const float dtMs = dt * 1000.0f;

    for (TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (!prop.active || prop.type != TopdownPropType::Sprite) {
            continue;
        }

        prop.animationTimeMs += dtMs;

        if (prop.oneShotActive && prop.animationTimeMs >= prop.oneShotDurationMs) {
            prop.oneShotActive = false;
            prop.currentAnimation = prop.baseAnimation;
            prop.animationTimeMs = 0.0f;
            prop.oneShotDurationMs = 0.0f;
        }
    }
}

void TopdownRenderProps(GameState& state, TopdownEffectPlacement placement)
{
    const std::vector<int>* bucket = GetPropBucket(state.topdown.runtime.render, placement);
    if (bucket == nullptr) {
        return;
    }

    for (int propIndex : *bucket) {
        if (propIndex < 0 || propIndex >= static_cast<int>(state.topdown.runtime.props.size())) {
            continue;
        }

        TopdownRuntimeProp& prop = state.topdown.runtime.props[propIndex];
        if (!prop.active || !prop.visible) {
            continue;
        }

        const Vector2 screen = TopdownWorldToScreen(state, prop.position);
        const float scale = static_cast<float>(state.topdown.currentLevelBaseAssetScale);

        if (prop.type == TopdownPropType::Image) {
            const TextureResource* tex = FindTextureResource(state.resources, prop.textureHandle);
            if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
                continue;
            }

            Rectangle src = {
                    0.0f,
                    0.0f,
                    static_cast<float>(tex->texture.width),
                    static_cast<float>(tex->texture.height)
            };

            if (prop.flipX) {
                src.width = -src.width;
            }

            Rectangle dst = {
                    roundf(screen.x),
                    roundf(screen.y),
                    fabsf(src.width) * scale,
                    fabsf(src.height) * scale
            };

            Vector2 origin{};
            if (prop.hasOriginOverride) {
                origin = prop.originOverride;
            } else {
                origin = {
                        fabsf(src.width) * 0.5f * scale,
                        fabsf(src.height) * 0.5f * scale
                };
            }

            Color tint = WHITE;
            tint.a = static_cast<unsigned char>(Clamp(prop.opacity, 0.0f, 1.0f) * 255.0f);

            DrawTexturePro(tex->texture, src, dst, origin, 0.0f, tint);
            continue;
        }

        if (prop.type != TopdownPropType::Sprite) {
            continue;
        }

        const SpriteAssetResource* sprite = FindSpriteAssetResource(state.resources, prop.spriteHandle);
        if (sprite == nullptr || !sprite->loaded) {
            continue;
        }

        const TextureResource* tex = FindTextureResource(state.resources, sprite->textureHandle);
        if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
            continue;
        }

        const std::string& animationName = prop.oneShotActive ? prop.oneShotAnimation : prop.currentAnimation;
        if (animationName.empty()) {
            continue;
        }

        const SpriteClip* clip = FindClipByName(*sprite, animationName);
        if (clip == nullptr) {
            continue;
        }

        int frameIndex = -1;
        if (prop.oneShotActive) {
            frameIndex = GetOneShotFrameIndex(*sprite, *clip, prop.animationTimeMs);
        } else if (prop.loop) {
            frameIndex = GetLoopingFrameIndex(*sprite, *clip, prop.animationTimeMs);
        } else {
            frameIndex = GetOneShotFrameIndex(*sprite, *clip, prop.animationTimeMs);
        }

        if (frameIndex < 0 || frameIndex >= static_cast<int>(sprite->frames.size())) {
            continue;
        }

        const SpriteFrame& frame = sprite->frames[frameIndex];
        Rectangle src = frame.sourceRect;
        if (prop.flipX) {
            src.width = -src.width;
        }

        Rectangle dst = {
                roundf(screen.x),
                roundf(screen.y),
                fabsf(src.width) * scale,
                fabsf(src.height) * scale
        };

        Vector2 origin{};
        if (prop.hasOriginOverride) {
            origin = prop.originOverride;
        } else {
            origin = {
                    fabsf(src.width) * 0.5f * scale,
                    fabsf(src.height) * 0.5f * scale
            };
        }

        Color tint = WHITE;
        tint.a = static_cast<unsigned char>(Clamp(prop.opacity, 0.0f, 1.0f) * 255.0f);

        DrawTexturePro(tex->texture, src, dst, origin, 0.0f, tint);
    }
}
