#include "topdown/LevelProps.h"

#include <algorithm>
#include "raymath.h"
#include "resources/TextureAsset.h"
#include "resources/AsepriteAsset.h"
#include "topdown/TopdownHelpers.h"

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
        if (!prop.active) continue;
        prop.animationTimeMs += dtMs;

        if (prop.oneShotActive && prop.oneShotDurationMs > 0.0f && prop.animationTimeMs >= prop.oneShotDurationMs) {
            prop.oneShotActive = false;
            prop.currentAnimation = prop.baseAnimation;
            prop.animationTimeMs = 0.0f;
        }

        if (prop.moving) {
            prop.moveTimerMs += dtMs;
            const float duration = std::max(prop.moveDurationMs, 0.0001f);
            float t = Clamp(prop.moveTimerMs / duration, 0.0f, 1.0f);
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
        if (index < 0 || index >= (int)state.topdown.runtime.props.size()) continue;
        const TopdownRuntimeProp& prop = state.topdown.runtime.props[index];
        if (!prop.active || !prop.visible) continue;

        const Vector2 screen = TopdownWorldToScreen(state, prop.position);
        const Color tint{255,255,255,(unsigned char)std::round(255.0f * Clamp(prop.opacity, 0.0f, 1.0f))};

        if (prop.type == TopdownPropType::Image) {
            const TextureResource* tex = FindTextureResource(state.resources, prop.textureHandle);
            if (tex == nullptr || !tex->loaded || tex->texture.id == 0) continue;
            Rectangle src{0,0,(float)tex->texture.width,(float)tex->texture.height};
            if (prop.flipX) src.width = -src.width;
            Vector2 origin = prop.hasOriginOverride ? prop.originOverride : Vector2{(float)tex->texture.width*0.5f,(float)tex->texture.height*0.5f};
            DrawTexturePro(tex->texture, src, Rectangle{std::round(screen.x), std::round(screen.y), (float)tex->texture.width, (float)tex->texture.height}, origin, 0.0f, tint);
            continue;
        }

        const SpriteAssetResource* sprite = FindSpriteAssetResource(state.resources, prop.spriteHandle);
        if (sprite == nullptr || !sprite->loaded || sprite->frames.empty()) continue;
        const std::string& anim = prop.oneShotActive ? prop.oneShotAnimation : prop.currentAnimation;
        const int clipIndex = FindClipIndex(*sprite, "", anim);
        if (clipIndex < 0 || clipIndex >= (int)sprite->clips.size()) continue;
        const SpriteClip& clip = sprite->clips[clipIndex];
        int frameIndex = prop.oneShotActive ? GetOneShotFrameIndex(*sprite, clip, prop.animationTimeMs) : GetLoopingFrameIndex(*sprite, clip, prop.animationTimeMs);
        if (frameIndex < 0 || frameIndex >= (int)sprite->frames.size()) continue;
        const SpriteFrame& frame = sprite->frames[frameIndex];
        Rectangle src{(float)frame.sourceRect.x,(float)frame.sourceRect.y,(float)frame.sourceRect.width,(float)frame.sourceRect.height};
        if (prop.flipX) src.width = -src.width;
        Vector2 origin = prop.hasOriginOverride ? prop.originOverride : Vector2{(float)frame.sourceRect.width*0.5f,(float)frame.sourceRect.height*0.5f};
        DrawTexturePro(sprite->atlasTexture, src, Rectangle{std::round(screen.x), std::round(screen.y), (float)frame.sourceRect.width, (float)frame.sourceRect.height}, origin, 0.0f, tint);
    }
}
