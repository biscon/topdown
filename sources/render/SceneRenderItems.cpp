#include "render/SceneRenderInternal.h"

#include <algorithm>
#include <vector>

#include "render/EffectShaderRegistry.h"

void SceneRenderBuildDepthSortedWorldDrawItems(
        const GameState& state,
        std::vector<SceneRenderWorldDrawItem>& drawItems)
{
    drawItems.clear();

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        SceneRenderWorldDrawItem item;
        item.sortY = actor.feetPos.y;
        item.sortOrder = 0;
        item.type = SceneRenderWorldDrawItemType::Actor;
        item.actorIndex = i;
        drawItems.push_back(item);
    }

    const int propCount = std::min(
            static_cast<int>(state.adventure.currentScene.props.size()),
            static_cast<int>(state.adventure.props.size()));

    for (int i = 0; i < propCount; ++i) {
        const ScenePropData& sceneProp = state.adventure.currentScene.props[i];
        const PropInstance& prop = state.adventure.props[i];
        if (!prop.visible) {
            continue;
        }

        if (sceneProp.depthMode != ScenePropDepthMode::DepthSorted) {
            continue;
        }

        SceneRenderWorldDrawItem item;
        item.sortY = prop.feetPos.y;
        item.sortOrder = 0;
        item.type = SceneRenderWorldDrawItemType::Prop;
        item.propIndex = i;
        drawItems.push_back(item);
    }

    const int effectCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = state.adventure.currentScene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];
        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::DepthSorted) {
            continue;
        }

        SceneRenderWorldDrawItem item;
        item.sortY = sceneEffect.worldPos.y + sceneEffect.worldSize.y;
        item.sortOrder = 0;
        item.type = SceneRenderWorldDrawItemType::EffectSprite;
        item.effectIndex = i;
        drawItems.push_back(item);
    }

    const int effectRegionCount = std::min(
            static_cast<int>(state.adventure.currentScene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    for (int i = 0; i < effectRegionCount; ++i) {
        const SceneEffectRegionData& sceneEffect = state.adventure.currentScene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];
        if (!effect.visible) {
            continue;
        }

        if (sceneEffect.depthMode != ScenePropDepthMode::DepthSorted) {
            continue;
        }

        if (sceneEffect.renderAsOverlay) {
            continue;
        }

        SceneRenderWorldDrawItem item;
        item.sortY = sceneEffect.usePolygon
                     ? SceneRenderGetPolygonMaxY(sceneEffect.polygon)
                     : (sceneEffect.worldRect.y + sceneEffect.worldRect.height);
        item.sortOrder = sceneEffect.sortOrder;
        item.type = SceneRenderWorldDrawItemType::EffectRegion;
        item.effectRegionIndex = i;
        drawItems.push_back(item);
    }

    std::sort(drawItems.begin(), drawItems.end(),
              [](const SceneRenderWorldDrawItem& a, const SceneRenderWorldDrawItem& b) {
                  if (a.sortY != b.sortY) {
                      return a.sortY < b.sortY;
                  }

                  if (a.sortOrder != b.sortOrder) {
                      return a.sortOrder < b.sortOrder;
                  }

                  if (a.type != b.type) {
                      return static_cast<int>(a.type) < static_cast<int>(b.type);
                  }

                  switch (a.type) {
                      case SceneRenderWorldDrawItemType::Actor:
                          return a.actorIndex < b.actorIndex;
                      case SceneRenderWorldDrawItemType::Prop:
                          return a.propIndex < b.propIndex;
                      case SceneRenderWorldDrawItemType::EffectSprite:
                          return a.effectIndex < b.effectIndex;
                      case SceneRenderWorldDrawItemType::EffectRegion:
                          return a.effectRegionIndex < b.effectRegionIndex;
                      default:
                          return false;
                  }
              });
}
