#include "topdown/LevelProps.h"

#include <cmath>

#include "raylib.h"
#include "raymath.h"
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
        if (!prop.active || !prop.visible || prop.type != TopdownPropType::Image) {
            continue;
        }

        const TextureResource* tex = FindTextureResource(state.resources, prop.textureHandle);
        if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
            continue;
        }

        const Vector2 screen = TopdownWorldToScreen(state, prop.position);

        Rectangle src = {
                0.0f,
                0.0f,
                static_cast<float>(tex->texture.width),
                static_cast<float>(tex->texture.height)
        };

        if (prop.flipX) {
            src.width = -src.width;
        }

        const float scale = static_cast<float>(state.topdown.currentLevelBaseAssetScale);

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
