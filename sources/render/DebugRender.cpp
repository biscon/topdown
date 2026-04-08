#include <sstream>
#include <cmath>
#include "DebugRender.h"
#include "resources/AsepriteAsset.h"
#include "scene/SceneHelpers.h"
#include "RenderHelpers.h"
#include "adventure/AdventureHelpers.h"
#include "scripting/ScriptSystem.h"
#include "EffectShaderRegistry.h"

static void DrawWalkPolygons(const GameState& state)
{
    const SceneData& scene = state.adventure.currentScene;
    for (const NavPolygon& poly : scene.navMesh.sourcePolygons) {
        const int count = static_cast<int>(poly.vertices.size());
        if (count < 2) {
            continue;
        }

        for (int i = 0; i < count; ++i) {
            const Vector2 a = {
                    poly.vertices[i].x - state.adventure.camera.position.x,
                    poly.vertices[i].y - state.adventure.camera.position.y
            };
            const Vector2 b = {
                    poly.vertices[(i + 1) % count].x - state.adventure.camera.position.x,
                    poly.vertices[(i + 1) % count].y - state.adventure.camera.position.y
            };
            DrawLineEx(a, b, 2.0f, Color{80, 180, 255, 255});
        }
    }
}

static void DrawBlockingPolygons(const GameState& state)
{
    const SceneData& scene = state.adventure.currentScene;
    for (const NavPolygon& poly : scene.navMesh.blockerPolygons) {
        const int count = static_cast<int>(poly.vertices.size());
        if (count < 2) {
            continue;
        }

        for (int i = 0; i < count; ++i) {
            const Vector2 a = {
                    poly.vertices[i].x - state.adventure.camera.position.x,
                    poly.vertices[i].y - state.adventure.camera.position.y
            };
            const Vector2 b = {
                    poly.vertices[(i + 1) % count].x - state.adventure.camera.position.x,
                    poly.vertices[(i + 1) % count].y - state.adventure.camera.position.y
            };
            DrawLineEx(a, b, 2.0f, Color{180, 80, 255, 255});
        }
    }
}

static void DrawNavTriangles(const GameState& state)
{
    const NavMeshData& nav = state.adventure.currentScene.navMesh;
    if (!nav.built) {
        return;
    }

    for (const NavTriangle& tri : nav.triangles) {
        const Vector2 a = {
                nav.vertices[tri.vertexIndex0].x - state.adventure.camera.position.x,
                nav.vertices[tri.vertexIndex0].y - state.adventure.camera.position.y
        };
        const Vector2 b = {
                nav.vertices[tri.vertexIndex1].x - state.adventure.camera.position.x,
                nav.vertices[tri.vertexIndex1].y - state.adventure.camera.position.y
        };
        const Vector2 c = {
                nav.vertices[tri.vertexIndex2].x - state.adventure.camera.position.x,
                nav.vertices[tri.vertexIndex2].y - state.adventure.camera.position.y
        };

        DrawLineEx(a, b, 1.0f, Color{0, 255, 120, 255});
        DrawLineEx(b, c, 1.0f, Color{0, 255, 120, 255});
        DrawLineEx(c, a, 1.0f, Color{0, 255, 120, 255});
    }
}

static void DrawNavAdjacency(const GameState& state)
{
    const NavMeshData& nav = state.adventure.currentScene.navMesh;
    if (!nav.built) {
        return;
    }

    for (int i = 0; i < static_cast<int>(nav.triangles.size()); ++i) {
        const NavTriangle& tri = nav.triangles[i];
        const Vector2 a = {
                tri.centroid.x - state.adventure.camera.position.x,
                tri.centroid.y - state.adventure.camera.position.y
        };

        const int neighbors[3] = { tri.neighbor0, tri.neighbor1, tri.neighbor2 };
        for (int n : neighbors) {
            if (n < 0 || n <= i || n >= static_cast<int>(nav.triangles.size())) {
                continue;
            }

            const NavTriangle& other = nav.triangles[n];
            const Vector2 b = {
                    other.centroid.x - state.adventure.camera.position.x,
                    other.centroid.y - state.adventure.camera.position.y
            };

            DrawLineEx(a, b, 2.0f, Color{255, 100, 255, 255});
        }
    }
}

static void DrawDebugTrianglePath(const GameState& state)
{
    const NavMeshData& nav = state.adventure.currentScene.navMesh;
    const std::vector<int>& triPath = state.adventure.debugTrianglePath;

    for (int i = 0; i + 1 < static_cast<int>(triPath.size()); ++i) {
        const int aIndex = triPath[i];
        const int bIndex = triPath[i + 1];

        if (aIndex < 0 || bIndex < 0 ||
            aIndex >= static_cast<int>(nav.triangles.size()) ||
            bIndex >= static_cast<int>(nav.triangles.size())) {
            continue;
        }

        const Vector2 a{
                nav.triangles[aIndex].centroid.x - state.adventure.camera.position.x,
                nav.triangles[aIndex].centroid.y - state.adventure.camera.position.y
        };
        const Vector2 b{
                nav.triangles[bIndex].centroid.x - state.adventure.camera.position.x,
                nav.triangles[bIndex].centroid.y - state.adventure.camera.position.y
        };

        DrawLineEx(a, b, 3.0f, ORANGE);
    }
}

static void DrawDebugActorPath(const GameState& state)
{
    const ActorInstance* controlledActor = GetControlledActor(state);
    if (!state.adventure.currentScene.loaded || controlledActor == nullptr) {
        return;
    }

    Vector2 prev{
            controlledActor->feetPos.x - state.adventure.camera.position.x,
            controlledActor->feetPos.y - state.adventure.camera.position.y
    };

    for (int i = controlledActor->path.currentPoint; i < static_cast<int>(controlledActor->path.points.size()); ++i) {
        const Vector2 p{
                controlledActor->path.points[i].x - state.adventure.camera.position.x,
                controlledActor->path.points[i].y - state.adventure.camera.position.y
        };

        DrawLineEx(prev, p, 3.0f, YELLOW);
        DrawCircleV(p, 4.0f, GOLD);
        prev = p;
    }
}

static void DrawDebugTargets(const GameState& state)
{
    if (state.adventure.hasLastClickWorldPos) {
        const Vector2 p{
                state.adventure.lastClickWorldPos.x - state.adventure.camera.position.x,
                state.adventure.lastClickWorldPos.y - state.adventure.camera.position.y
        };
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 8.0f, RED);
    }

    if (state.adventure.hasLastResolvedTargetPos) {
        const Vector2 p{
                state.adventure.lastResolvedTargetPos.x - state.adventure.camera.position.x,
                state.adventure.lastResolvedTargetPos.y - state.adventure.camera.position.y
        };
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 10.0f, GREEN);
    }
}

static void DrawDebugActors(const GameState& state)
{
    if (!state.adventure.currentScene.loaded) {
        return;
    }

    const int controlledActorIndex = GetControlledActorIndex(state);
    const CameraData& cam = state.adventure.camera;

    for (int i = 0; i < static_cast<int>(state.adventure.actors.size()); ++i) {
        const ActorInstance& actor = state.adventure.actors[i];
        if (!actor.activeInScene || !actor.visible) {
            continue;
        }

        const Rectangle visualRect = GetActorScreenRect(state, actor);
        const Rectangle interactionRect = GetActorInteractionRect(state, actor);

        DrawRectangleLinesEx(visualRect, 2.0f, RED);
        DrawRectangleLinesEx(interactionRect, 2.0f, LIME);

        const Vector2 feet{
                actor.feetPos.x - state.adventure.camera.position.x,
                actor.feetPos.y - state.adventure.camera.position.y
        };
        DrawCircleV(feet, 5.0f, RED);

        DrawText(
                actor.actorId.c_str(),
                static_cast<int>(feet.x + 8.0f),
                static_cast<int>(feet.y - 8.0f),
                16,
                RED);

        if (i == controlledActorIndex &&
            cam.followDeadZoneWidth > 0.0f &&
            cam.followDeadZoneHeight > 0.0f) {

            float biasShiftX = 0.0f;
            switch (cam.biasLatch) {
                case CameraBiasLatch::Left:
                    biasShiftX = cam.followBiasX;
                    break;
                case CameraBiasLatch::Right:
                    biasShiftX = -cam.followBiasX;
                    break;
                case CameraBiasLatch::None:
                default:
                    break;
            }

            const Rectangle deadZoneRect{
                    (cam.viewportWidth - cam.followDeadZoneWidth) * 0.5f + biasShiftX,
                    (cam.viewportHeight - cam.followDeadZoneHeight) * 0.5f,
                    cam.followDeadZoneWidth,
                    cam.followDeadZoneHeight
            };

            DrawRectangleLinesEx(deadZoneRect, 2.0f, BLUE);
        }
    }
}

static void DrawScenePolygon(const GameState& state, const ScenePolygon& poly, Color color)
{
    const int count = static_cast<int>(poly.vertices.size());
    for (int i = 0; i < count; ++i) {
        const Vector2 a{
                poly.vertices[i].x - state.adventure.camera.position.x,
                poly.vertices[i].y - state.adventure.camera.position.y
        };
        const Vector2 b{
                poly.vertices[(i + 1) % count].x - state.adventure.camera.position.x,
                poly.vertices[(i + 1) % count].y - state.adventure.camera.position.y
        };
        DrawLineEx(a, b, 2.0f, color);
    }
}

static void DrawSceneObjectDebug(const GameState& state)
{
    const SceneData& scene = state.adventure.currentScene;

    for (const auto& spawn : scene.spawns) {
        const Vector2 p{
                spawn.position.x - state.adventure.camera.position.x,
                spawn.position.y - state.adventure.camera.position.y
        };
        DrawCircleV(p, 5.0f, SKYBLUE);
        DrawText(spawn.id.c_str(), static_cast<int>(p.x + 8.0f), static_cast<int>(p.y - 8.0f), 16, SKYBLUE);
    }

    for (const auto& hotspot : scene.hotspots) {
        DrawScenePolygon(state, hotspot.shape, ORANGE);
        const Vector2 p{
                hotspot.walkTo.x - state.adventure.camera.position.x,
                hotspot.walkTo.y - state.adventure.camera.position.y
        };
        DrawCircleV(p, 4.0f, ORANGE);
        DrawText(hotspot.id.c_str(), static_cast<int>(p.x + 8.0f), static_cast<int>(p.y - 8.0f), 16, ORANGE);
    }

    for (const auto& exitObj : scene.exits) {
        DrawScenePolygon(state, exitObj.shape, PURPLE);
        const Vector2 p{
                exitObj.walkTo.x - state.adventure.camera.position.x,
                exitObj.walkTo.y - state.adventure.camera.position.y
        };
        DrawCircleV(p, 4.0f, PURPLE);
        DrawText(exitObj.id.c_str(), static_cast<int>(p.x + 8.0f), static_cast<int>(p.y - 8.0f), 16, PURPLE);
    }

    for (const auto& prop : state.adventure.props) {
        const ScenePropData& sceneProp = scene.props[prop.scenePropIndex];
        const Vector2 p{
                prop.feetPos.x - state.adventure.camera.position.x,
                prop.feetPos.y - state.adventure.camera.position.y
        };

        const Rectangle propRect = GetPropScreenRect(state, sceneProp, prop);
        DrawRectangleLinesEx(propRect, 2.0f, MAGENTA);

        DrawCircleV(p, 4.0f, MAGENTA);
        DrawText(sceneProp.id.c_str(), static_cast<int>(p.x + 8.0f), static_cast<int>(p.y - 8.0f), 16, MAGENTA);
    }
}

static Color GetEffectDebugColor(const SceneEffectSpriteData& sceneEffect)
{
    if (sceneEffect.renderAsOverlay) {
        return YELLOW;
    }

    switch (sceneEffect.depthMode) {
        case ScenePropDepthMode::Back:
            return SKYBLUE;
        case ScenePropDepthMode::DepthSorted:
            return GREEN;
        case ScenePropDepthMode::Front:
            return PINK;
        default:
            return SKYBLUE;
    }
}

static void DrawEffectDebug(const GameState& state)
{
    const SceneData& scene = state.adventure.currentScene;

    const int effectCount = std::min(
            static_cast<int>(scene.effectSprites.size()),
            static_cast<int>(state.adventure.effectSprites.size()));

    for (int i = 0; i < effectCount; ++i) {
        const SceneEffectSpriteData& sceneEffect = scene.effectSprites[i];
        const EffectSpriteInstance& effect = state.adventure.effectSprites[i];
        if (!effect.visible) {
            continue;
        }

        const Color color = GetEffectDebugColor(sceneEffect);

        Rectangle rect{};
        rect.x = sceneEffect.worldPos.x - state.adventure.camera.position.x;
        rect.y = sceneEffect.worldPos.y - state.adventure.camera.position.y;
        rect.width = sceneEffect.worldSize.x;
        rect.height = sceneEffect.worldSize.y;

        DrawRectangleLinesEx(rect, 2.0f, color);

        const Vector2 anchor{
                rect.x + rect.width * 0.5f,
                rect.y + rect.height
        };
        DrawCircleV(anchor, 4.0f, color);

        DrawText(sceneEffect.id.c_str(),
                 static_cast<int>(anchor.x + 8.0f),
                 static_cast<int>(anchor.y - 8.0f),
                 16,
                 color);
    }
}

static void DrawEffectRegionDebug(const GameState& state)
{
    const SceneData& scene = state.adventure.currentScene;

    const int count = std::min(
            static_cast<int>(scene.effectRegions.size()),
            static_cast<int>(state.adventure.effectRegions.size()));

    auto GetRegionDebugColor = [](const SceneEffectRegionData& sceneEffect) -> Color {
        if (sceneEffect.renderAsOverlay) {
            return Color{255, 210, 80, 255};   // warm yellow/orange
        }

        switch (sceneEffect.depthMode) {
            case ScenePropDepthMode::Back:
                return Color{80, 160, 255, 255};   // blue
            case ScenePropDepthMode::DepthSorted:
                return Color{80, 255, 180, 255};   // aqua/green
            case ScenePropDepthMode::Front:
                return Color{200, 120, 255, 255};  // violet
            default:
                return Color{255, 255, 255, 255};
        }
    };

    auto GetBlendModeText = [](EffectBlendMode mode) -> const char* {
        switch (mode) {
            case EffectBlendMode::Add:
                return "add";
            case EffectBlendMode::Multiply:
                return "multiply";
            case EffectBlendMode::Normal:
            default:
                return "normal";
        }
    };

    for (int i = 0; i < count; ++i) {
        const SceneEffectRegionData& sceneEffect = scene.effectRegions[i];
        const EffectRegionInstance& effect = state.adventure.effectRegions[i];
        if (!effect.visible) {
            continue;
        }

        const Color color = GetRegionDebugColor(sceneEffect);

        const bool isPolygon =
                static_cast<int>(sceneEffect.polygon.vertices.size()) >= 3;

        Vector2 labelAnchor{};

        if (isPolygon) {
            const int vertexCount = static_cast<int>(sceneEffect.polygon.vertices.size());

            float sumX = 0.0f;
            float sumY = 0.0f;

            for (int v = 0; v < vertexCount; ++v) {
                const Vector2 a{
                        sceneEffect.polygon.vertices[v].x - state.adventure.camera.position.x,
                        sceneEffect.polygon.vertices[v].y - state.adventure.camera.position.y
                };
                const Vector2 b{
                        sceneEffect.polygon.vertices[(v + 1) % vertexCount].x - state.adventure.camera.position.x,
                        sceneEffect.polygon.vertices[(v + 1) % vertexCount].y - state.adventure.camera.position.y
                };

                DrawLineEx(a, b, 2.0f, color);
                DrawCircleV(a, 3.0f, color);

                sumX += a.x;
                sumY += a.y;
            }

            labelAnchor.x = sumX / static_cast<float>(vertexCount);
            labelAnchor.y = sumY / static_cast<float>(vertexCount);
        } else {
            Rectangle rect{};
            rect.x = sceneEffect.worldRect.x - state.adventure.camera.position.x;
            rect.y = sceneEffect.worldRect.y - state.adventure.camera.position.y;
            rect.width = sceneEffect.worldRect.width;
            rect.height = sceneEffect.worldRect.height;

            DrawRectangleLinesEx(rect, 2.0f, color);

            DrawCircleV(Vector2{rect.x, rect.y}, 3.0f, color);
            DrawCircleV(Vector2{rect.x + rect.width, rect.y}, 3.0f, color);
            DrawCircleV(Vector2{rect.x + rect.width, rect.y + rect.height}, 3.0f, color);
            DrawCircleV(Vector2{rect.x, rect.y + rect.height}, 3.0f, color);

            labelAnchor.x = rect.x + rect.width * 0.5f;
            labelAnchor.y = rect.y + rect.height * 0.5f;
        }

        DrawCircleV(labelAnchor, 4.0f, color);

        std::string line1 =
                sceneEffect.id +
                " [" +
                std::string(isPolygon ? "poly" : "rect") +
                "]";

        std::string line2 =
                std::string(EffectShaderTypeToString(effect.shaderType)) +
                "  blend=" +
                GetBlendModeText(sceneEffect.blendMode);

        if (sceneEffect.renderAsOverlay) {
            line2 += "  overlay";
        }

        DrawText(
                line1.c_str(),
                static_cast<int>(labelAnchor.x + 8.0f),
                static_cast<int>(labelAnchor.y - 20.0f),
                16,
                color);

        DrawText(
                line2.c_str(),
                static_cast<int>(labelAnchor.x + 8.0f),
                static_cast<int>(labelAnchor.y - 2.0f),
                16,
                color);
    }
}

static void DrawScriptDebugPanel(const GameState& state)
{
    std::vector<ScriptDebugEntry> entries;
    ScriptSystemBuildDebugEntries(state.script, state, entries);

    const int x = 20;
    const int y = 210;
    const int lineHeight = 22;

    DrawText("[ScriptSystem]", x, y, 20, SKYBLUE);

    if (entries.empty()) {
        DrawText("  <no active scripts>", x, y + lineHeight, 20, LIGHTGRAY);
        return;
    }

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const ScriptDebugEntry& entry = entries[i];

        const int lineY = y + lineHeight * (i + 1);

        DrawText(entry.functionName.c_str(), x + 10, lineY, 20, WHITE);
        DrawText(entry.waitLabel.c_str(), x + 600, lineY, 20, YELLOW);
        DrawLine(x + 10, lineY+20, x + 850, lineY+20, LIGHTGRAY);
    }
}

static void DrawSoundEmitterDebug(const GameState& state)
{
    const SceneData& scene = state.adventure.currentScene;

    const int count = std::min(
            static_cast<int>(scene.soundEmitters.size()),
            static_cast<int>(state.audio.sceneEmitters.size()));

    for (int i = 0; i < count; ++i) {
        const SceneSoundEmitterData& sceneEmitter = scene.soundEmitters[i];
        const SoundEmitterInstance& emitter = state.audio.sceneEmitters[i];

        const Vector2 center{
                sceneEmitter.position.x - state.adventure.camera.position.x,
                sceneEmitter.position.y - state.adventure.camera.position.y
        };

        Color color = emitter.enabled ? ORANGE : GRAY;
        if (emitter.active) {
            color = GREEN;
        }

        DrawCircleLines(
                static_cast<int>(center.x),
                static_cast<int>(center.y),
                sceneEmitter.radius,
                Fade(color, 0.75f));

        DrawCircleV(center, 5.0f, color);

        DrawText(
                sceneEmitter.id.c_str(),
                static_cast<int>(center.x + 8.0f),
                static_cast<int>(center.y - 26.0f),
                16,
                color);

        DrawText(
                sceneEmitter.soundId.c_str(),
                static_cast<int>(center.x + 8.0f),
                static_cast<int>(center.y - 8.0f),
                16,
                color);

        if (sceneEmitter.pan) {
            DrawLineEx(
                    Vector2{center.x - 16.0f, center.y},
                    Vector2{center.x + 16.0f, center.y},
                    2.0f,
                    color);
        }
    }
}

void RenderAdventureDebug(const GameState& state) {
    if (!state.adventure.currentScene.loaded) {
        return;
    }
    if (state.debug.showWalkPolygons) {
        DrawWalkPolygons(state);
        DrawBlockingPolygons(state);
    }

    if (state.debug.showNavTriangles) {
        DrawNavTriangles(state);
    }

    if (state.debug.showNavAdjacency) {
        DrawNavAdjacency(state);
    }

    if (state.debug.showTrianglePath) {
        DrawDebugTrianglePath(state);
    }

    if (state.debug.showPath) {
        DrawDebugActorPath(state);
        DrawDebugTargets(state);
    }

    if (state.debug.showSceneObjects) {
        DrawSceneObjectDebug(state);
    }

    if (state.debug.showEffects) {
        DrawEffectDebug(state);
        DrawEffectRegionDebug(state);
    }

    if (state.debug.showFeetPoints) {
        DrawDebugActors(state);
        DrawSoundEmitterDebug(state);
    }

    if (state.debug.showScaleInfo) {
        const ActorInstance* controlledActor = GetControlledActor(state);
        if (controlledActor != nullptr) {
            const float depthScale = ComputeDepthScale(state.adventure.currentScene, controlledActor->feetPos.y);
            DrawText(TextFormat("scale: %.3f", depthScale), 20, 50, 20, YELLOW);
        }
        DrawText(TextFormat("scene: %s", state.adventure.currentScene.sceneId.c_str()), 20, 75, 20, YELLOW);
        DrawText(TextFormat("nav verts: %d", static_cast<int>(state.adventure.currentScene.navMesh.vertices.size())), 20, 100, 20, YELLOW);
        DrawText(TextFormat("nav tris: %d", static_cast<int>(state.adventure.currentScene.navMesh.triangles.size())), 20, 125, 20, YELLOW);
        DrawText(TextFormat("cam: %.0f, %.0f", state.adventure.camera.position.x, state.adventure.camera.position.y), 20, 150, 20, YELLOW);
        DrawText(TextFormat("spawns: %d hotspots: %d exits: %d",
                            static_cast<int>(state.adventure.currentScene.spawns.size()),
                            static_cast<int>(state.adventure.currentScene.hotspots.size()),
                            static_cast<int>(state.adventure.currentScene.exits.size())),
                 20, 175, 20, YELLOW);
    }

    if (state.debug.showScripts) {
        DrawScriptDebugPanel(state);
    }
}
