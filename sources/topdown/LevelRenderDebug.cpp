#include "topdown/LevelRenderDebug.h"

#include <cmath>
#include <vector>

#include "raylib.h"
#include "topdown/TopdownHelpers.h"
#include "render/EffectShaderRegistry.h"
#include "nav/NavMeshData.h"
#include "scripting/ScriptSystem.h"
#include "NpcRegistry.h"
#include "resources/AsepriteAsset.h"
#include "PlayerRegistry.h"
#include "TopdownNpcAi.h"


static void DrawPolygonOutline(const GameState& state, const std::vector<Vector2>& points, Color color, float thickness = 2.0f)
{
    if (points.size() < 2) {
        return;
    }

    for (size_t i = 0; i < points.size(); ++i) {
        const size_t next = (i + 1) % points.size();
        const Vector2 a = TopdownWorldToScreen(state, points[i]);
        const Vector2 b = TopdownWorldToScreen(state, points[next]);
        DrawLineEx(a, b, thickness, color);
    }
}

static void DrawRuntimeOcclusionPolygon(
        const GameState& state,
        const TopdownRuntimeEffectRegion& runtime,
        Color color)
{
    if (!runtime.hasWallOcclusionPolygon || runtime.wallOcclusionPolygon.size() < 3) {
        return;
    }

    const std::vector<Vector2>& poly = runtime.wallOcclusionPolygon;

    for (size_t i = 0; i < poly.size(); ++i) {
        const size_t next = (i + 1) % poly.size();

        const Vector2 a = TopdownWorldToScreen(state, poly[i]);
        const Vector2 b = TopdownWorldToScreen(state, poly[next]);

        DrawLineEx(a, b, 1.5f, color);
        DrawCircleV(a, 2.5f, color);
    }
}

static void DrawBlockerDebug(const GameState& state)
{
    DrawPolygonOutline(
            state,
            state.topdown.authored.levelBoundary,
            Color{80, 200, 255, 255},
            2.0f);

    for (const TopdownRuntimeObstacle& obstacle : state.topdown.runtime.collision.obstacles) {
        const Color color =
                (obstacle.kind == TopdownObstacleKind::MovementAndVision)
                ? Color{255, 90, 90, 255}
                : Color{255, 170, 60, 255};

        DrawPolygonOutline(state, obstacle.polygon, color, 2.0f);

        if (!obstacle.name.empty()) {
            const Vector2 labelPos = TopdownWorldToScreen(state, Vector2{obstacle.bounds.x, obstacle.bounds.y});
            DrawText(
                    obstacle.name.c_str(),
                    static_cast<int>(labelPos.x),
                    static_cast<int>(labelPos.y) - 18,
                    18,
                    color);
        }
    }
}

static void DrawNavTriangles(const GameState& state)
{
    const NavMeshData& nav = state.topdown.runtime.nav.navMesh;
    if (!nav.built) {
        return;
    }

    for (const NavTriangle& tri : nav.triangles) {
        const Vector2 a = TopdownWorldToScreen(state, nav.vertices[tri.vertexIndex0]);
        const Vector2 b = TopdownWorldToScreen(state, nav.vertices[tri.vertexIndex1]);
        const Vector2 c = TopdownWorldToScreen(state, nav.vertices[tri.vertexIndex2]);

        DrawLineEx(a, b, 1.0f, Color{0, 255, 120, 255});
        DrawLineEx(b, c, 1.0f, Color{0, 255, 120, 255});
        DrawLineEx(c, a, 1.0f, Color{0, 255, 120, 255});
    }
}

static void DrawNavAdjacency(const GameState& state)
{
    const NavMeshData& nav = state.topdown.runtime.nav.navMesh;
    if (!nav.built) {
        return;
    }

    for (int i = 0; i < static_cast<int>(nav.triangles.size()); ++i) {
        const NavTriangle& tri = nav.triangles[i];
        const Vector2 a = TopdownWorldToScreen(state, tri.centroid);

        const int neighbors[3] = { tri.neighbor0, tri.neighbor1, tri.neighbor2 };
        for (int n : neighbors) {
            if (n < 0 || n <= i || n >= static_cast<int>(nav.triangles.size())) {
                continue;
            }

            const NavTriangle& other = nav.triangles[n];
            const Vector2 b = TopdownWorldToScreen(state, other.centroid);

            DrawLineEx(a, b, 2.0f, Color{255, 100, 255, 255});
        }
    }
}

static void DrawNavDebug(const GameState& state)
{
    const NavMeshData& nav = state.topdown.runtime.nav.navMesh;
    if (!nav.built) {
        DrawText("navmesh: not built", 20, 20, 20, RED);
        return;
    }

    DrawNavTriangles(state);
    DrawNavAdjacency(state);

    DrawText(
            TextFormat("nav verts: %d", static_cast<int>(nav.vertices.size())),
            20,
            20,
            20,
            YELLOW);

    DrawText(
            TextFormat("nav tris: %d", static_cast<int>(nav.triangles.size())),
            20,
            45,
            20,
            YELLOW);
}

static const char* TopdownEffectPlacementToString(TopdownEffectPlacement placement)
{
    switch (placement) {
        case TopdownEffectPlacement::AfterBottom:
            return "after_bottom";
        case TopdownEffectPlacement::AfterCharacters:
            return "after_characters";
        case TopdownEffectPlacement::Final:
            return "final";
        default:
            return "unknown";
    }
}

static const char* EffectBlendModeToString(EffectBlendMode mode)
{
    switch (mode) {
        case EffectBlendMode::Add:
            return "add";
        case EffectBlendMode::Multiply:
            return "multiply";
        case EffectBlendMode::Normal:
        default:
            return "normal";
    }
}

static void DrawEffectRegionDebug(const GameState& state)
{
    const int count = static_cast<int>(state.topdown.authored.effectRegions.size());

    for (int i = 0; i < count; ++i) {
        const TopdownAuthoredEffectRegion& authored = state.topdown.authored.effectRegions[i];
        const TopdownRuntimeEffectRegion& runtime = state.topdown.runtime.render.effectRegions[i];

        if (!runtime.visible) {
            continue;
        }

        Color color = SKYBLUE;
        switch (authored.placement) {
            case TopdownEffectPlacement::AfterBottom:
                color = Color{80, 200, 255, 255};
                break;
            case TopdownEffectPlacement::AfterCharacters:
                color = Color{255, 220, 80, 255};
                break;
            case TopdownEffectPlacement::Final:
                color = Color{220, 120, 255, 255};
                break;
        }

        Vector2 labelAnchor{};

        if (authored.usePolygon) {
            DrawPolygonOutline(state, authored.polygon, color);

            float sumX = 0.0f;
            float sumY = 0.0f;
            for (const Vector2& p : authored.polygon) {
                const Vector2 screen = TopdownWorldToScreen(state, p);
                DrawCircleV(screen, 3.0f, color);
                sumX += screen.x;
                sumY += screen.y;
            }

            const float countF = static_cast<float>(authored.polygon.size());
            labelAnchor.x = sumX / countF;
            labelAnchor.y = sumY / countF;
        } else {
            Rectangle rect{
                    authored.worldRect.x - state.topdown.runtime.camera.position.x,
                    authored.worldRect.y - state.topdown.runtime.camera.position.y,
                    authored.worldRect.width,
                    authored.worldRect.height
            };

            DrawRectangleLinesEx(rect, 2.0f, color);

            DrawCircleV(Vector2{rect.x, rect.y}, 3.0f, color);
            DrawCircleV(Vector2{rect.x + rect.width, rect.y}, 3.0f, color);
            DrawCircleV(Vector2{rect.x + rect.width, rect.y + rect.height}, 3.0f, color);
            DrawCircleV(Vector2{rect.x, rect.y + rect.height}, 3.0f, color);

            labelAnchor.x = rect.x + rect.width * 0.5f;
            labelAnchor.y = rect.y + rect.height * 0.5f;
        }

        DrawCircleV(labelAnchor, 4.0f, color);

        if (runtime.occludedByWalls) {
            const Vector2 originWorld = TopdownComputeEffectRegionOcclusionOrigin(authored);
            const Vector2 originScreen = TopdownWorldToScreen(state, originWorld);

            DrawCircleLines(
                    static_cast<int>(std::round(originScreen.x)),
                    static_cast<int>(std::round(originScreen.y)),
                    8.0f,
                    WHITE);

            DrawCircleV(originScreen, 3.0f, WHITE);

            DrawRuntimeOcclusionPolygon(
                    state,
                    runtime,
                    Color{255, 255, 255, 120});
        }

        const std::string line1 =
                authored.id +
                " [" +
                std::string(authored.usePolygon ? "poly" : "rect") +
                "]";

        std::string line2 =
                std::string(EffectShaderTypeToString(runtime.shaderType)) +
                "  " +
                TopdownEffectPlacementToString(authored.placement);

        if (authored.sortIndex != 0) {
            line2 += "  sort=" + std::to_string(authored.sortIndex);
        }

        std::string line3 =
                std::string("blend=") +
                EffectBlendModeToString(authored.blendMode);

        if (runtime.occludedByWalls) {
            line3 += runtime.hasWallOcclusionPolygon ? "  walls=ok" : "  walls=fail";
            line3 += authored.hasOcclusionOriginOverride ? "  origin=authored" : "  origin=auto";
        }

        DrawText(
                line1.c_str(),
                static_cast<int>(labelAnchor.x + 8.0f),
                static_cast<int>(labelAnchor.y - 22.0f),
                16,
                color);

        DrawText(
                line2.c_str(),
                static_cast<int>(labelAnchor.x + 8.0f),
                static_cast<int>(labelAnchor.y - 4.0f),
                16,
                color);

        DrawText(
                line3.c_str(),
                static_cast<int>(labelAnchor.x + 8.0f),
                static_cast<int>(labelAnchor.y + 14.0f),
                16,
                color);
    }
}

static const char* TopdownTriggerAffectsToString(TopdownTriggerAffects affects)
{
    switch (affects) {
        case TopdownTriggerAffects::Player:
            return "player";
        case TopdownTriggerAffects::Npc:
            return "npc";
        case TopdownTriggerAffects::All:
            return "all";
        default:
            return "unknown";
    }
}

static void DrawTriggerDebug(const GameState& state)
{
    for (const TopdownRuntimeTrigger& runtime : state.topdown.runtime.triggers) {
        if (runtime.authoredIndex < 0 ||
            runtime.authoredIndex >= static_cast<int>(state.topdown.authored.triggers.size())) {
            continue;
        }

        const TopdownAuthoredTrigger& authored = state.topdown.authored.triggers[runtime.authoredIndex];

        const Color color = runtime.enabled
                            ? Color{255, 170, 70, 255}
                            : Color{120, 120, 120, 200};

        Vector2 labelAnchor{};

        if (authored.usePolygon) {
            DrawPolygonOutline(state, authored.polygon, color, 2.0f);

            float sumX = 0.0f;
            float sumY = 0.0f;
            for (const Vector2& p : authored.polygon) {
                const Vector2 screen = TopdownWorldToScreen(state, p);
                DrawCircleV(screen, 2.5f, color);
                sumX += screen.x;
                sumY += screen.y;
            }

            const float count = static_cast<float>(authored.polygon.size());
            labelAnchor.x = sumX / count;
            labelAnchor.y = sumY / count;
        } else {
            Rectangle rect{
                    authored.worldRect.x - state.topdown.runtime.camera.position.x,
                    authored.worldRect.y - state.topdown.runtime.camera.position.y,
                    authored.worldRect.width,
                    authored.worldRect.height
            };

            DrawRectangleLinesEx(rect, 2.0f, color);
            labelAnchor.x = rect.x + rect.width * 0.5f;
            labelAnchor.y = rect.y + rect.height * 0.5f;
        }

        std::string line1 = authored.id + " [trigger]";
        std::string line2 = std::string("enabled=") + (runtime.enabled ? "yes" : "no") +
                            " affects=" + TopdownTriggerAffectsToString(authored.affects);
        std::string line3 = std::string("repeat=") + (runtime.repeat ? "yes" : "no") +
                            " delayMs=" + std::to_string(authored.delayMs) +
                            " pending=" + std::to_string(static_cast<int>(runtime.pendingCalls.size()));
        std::string line4 = std::string("script=") + (authored.script.empty() ? "<empty>" : authored.script);

        DrawText(line1.c_str(), static_cast<int>(labelAnchor.x + 8.0f), static_cast<int>(labelAnchor.y - 24.0f), 16, color);
        DrawText(line2.c_str(), static_cast<int>(labelAnchor.x + 8.0f), static_cast<int>(labelAnchor.y - 6.0f), 16, color);
        DrawText(line3.c_str(), static_cast<int>(labelAnchor.x + 8.0f), static_cast<int>(labelAnchor.y + 12.0f), 16, color);
        DrawText(line4.c_str(), static_cast<int>(labelAnchor.x + 8.0f), static_cast<int>(labelAnchor.y + 30.0f), 16, color);
    }
}

static const char* TopdownImageLayerKindToString(TopdownImageLayerKind kind)
{
    switch (kind) {
        case TopdownImageLayerKind::Bottom:
            return "bottom";
        case TopdownImageLayerKind::Top:
            return "top";
        default:
            return "unknown";
    }
}

static Rectangle BuildDebugImageLayerRect(
        const GameState& state,
        const TopdownAuthoredImageLayer& authored)
{
    const float safeScale = authored.scale > 0.0f ? authored.scale : 1.0f;

    const float scaledWidth = authored.imageSize.x * safeScale;
    const float scaledHeight = authored.imageSize.y * safeScale;

    Rectangle rect{};
    rect.x = std::round(
            authored.position.x -
            state.topdown.runtime.camera.position.x -
            (scaledWidth - authored.imageSize.x) * 0.5f);
    rect.y = std::round(
            authored.position.y -
            state.topdown.runtime.camera.position.y -
            (scaledHeight - authored.imageSize.y) * 0.5f);
    rect.width = std::round(scaledWidth);
    rect.height = std::round(scaledHeight);
    return rect;
}

static void DrawImageLayerDebug(const GameState& state)
{
    const int count = static_cast<int>(state.topdown.authored.imageLayers.size());

    for (int i = 0; i < count; ++i) {
        const TopdownAuthoredImageLayer& authored = state.topdown.authored.imageLayers[i];

        const Rectangle rect = BuildDebugImageLayerRect(state, authored);

        Color color = (authored.kind == TopdownImageLayerKind::Bottom)
                      ? Color{120, 255, 120, 255}
                      : Color{255, 120, 255, 255};

        DrawRectangleLinesEx(rect, 1.0f, color);

        const std::string line1 =
                authored.name + " [" + TopdownImageLayerKindToString(authored.kind) + "]";

        std::string line2 =
                std::string(EffectShaderTypeToString(authored.shaderType)) +
                "  scale=" +
                std::to_string(authored.scale) +
                "  blend=";

        switch (authored.blendMode) {
            case EffectBlendMode::Add:
                line2 += "add";
                break;
            case EffectBlendMode::Multiply:
                line2 += "multiply";
                break;
            case EffectBlendMode::Normal:
            default:
                line2 += "normal";
                break;
        }

        DrawText(
                line1.c_str(),
                static_cast<int>(rect.x + 4.0f),
                static_cast<int>(rect.y + 4.0f),
                14,
                color);

        DrawText(
                line2.c_str(),
                static_cast<int>(rect.x + 4.0f),
                static_cast<int>(rect.y + 20.0f),
                14,
                color);
    }
}

static void DrawScriptedPlayerPathDebug(const GameState& state)
{
    const TopdownScriptMoveState& move = state.topdown.runtime.scriptedMove;
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    if (!move.active || move.pathPoints.empty()) {
        return;
    }

    Vector2 prev = TopdownWorldToScreen(state, player.position);

    for (int i = move.currentPoint; i < static_cast<int>(move.pathPoints.size()); ++i) {
        const Vector2 p = TopdownWorldToScreen(state, move.pathPoints[i]);

        const bool isCurrentTarget = (i == move.currentPoint);
        const Color pointColor = isCurrentTarget ? GOLD : YELLOW;
        const float lineThickness = isCurrentTarget ? 3.0f : 2.0f;

        DrawLineEx(prev, p, lineThickness, YELLOW);
        DrawCircleV(p, isCurrentTarget ? 6.0f : 4.0f, pointColor);

        prev = p;
    }
}

static void DrawPlayerDebug(const GameState& state)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const Vector2 playerScreen = TopdownWorldToScreen(state, player.position);

    DrawCircleLines(
            static_cast<int>(playerScreen.x),
            static_cast<int>(playerScreen.y),
            player.radius,
            ORANGE);

    Vector2 facingTip{
            playerScreen.x + player.facing.x * 40.0f,
            playerScreen.y + player.facing.y * 40.0f
    };
    DrawLineEx(playerScreen, facingTip, 2.0f, ORANGE);

    DrawText(
            TextFormat("Player: %.1f, %.1f  vel=(%.1f, %.1f)",
                       player.position.x,
                       player.position.y,
                       player.velocity.x,
                       player.velocity.y),
            20,
            70,
            20,
            ORANGE);

    DrawScriptedPlayerPathDebug(state);
}

static void DrawSingleNpcPathDebug(const GameState& state, const TopdownNpcRuntime& npc)
{
    const NavMeshData& navMesh = state.topdown.runtime.nav.navMesh;

    // ---------------------------------------------------------------------
    // Raw triangle corridor debug
    // ---------------------------------------------------------------------
    if (navMesh.built && !npc.move.debugTrianglePath.empty()) {
        const Color triColor = Color{80, 200, 255, 110};
        const Color triOutlineColor = Color{80, 200, 255, 220};
        const Color currentTriColor = Color{255, 80, 255, 140};
        const Color currentTriOutlineColor = Color{255, 80, 255, 255};

        int currentTri = -1;
        if (npc.move.currentPoint >= 0 &&
            npc.move.currentPoint < static_cast<int>(npc.move.debugTrianglePath.size())) {
            currentTri = npc.move.debugTrianglePath[npc.move.currentPoint];
        }

        for (int i = 0; i < static_cast<int>(npc.move.debugTrianglePath.size()); ++i) {
            const int triIndex = npc.move.debugTrianglePath[i];
            if (triIndex < 0 || triIndex >= static_cast<int>(navMesh.triangles.size())) {
                continue;
            }

            const NavTriangle& tri = navMesh.triangles[triIndex];

            const Vector2 aWorld = navMesh.vertices[tri.vertexIndex0];
            const Vector2 bWorld = navMesh.vertices[tri.vertexIndex1];
            const Vector2 cWorld = navMesh.vertices[tri.vertexIndex2];

            const Vector2 a = TopdownWorldToScreen(state, aWorld);
            const Vector2 b = TopdownWorldToScreen(state, bWorld);
            const Vector2 c = TopdownWorldToScreen(state, cWorld);
            const Vector2 centroid = TopdownWorldToScreen(state, tri.centroid);

            const bool isCurrentTriangle = (triIndex == currentTri);

            DrawTriangle(
                    a,
                    b,
                    c,
                    isCurrentTriangle ? currentTriColor : triColor);

            DrawLineEx(
                    a,
                    b,
                    isCurrentTriangle ? 3.0f : 1.5f,
                    isCurrentTriangle ? currentTriOutlineColor : triOutlineColor);
            DrawLineEx(
                    b,
                    c,
                    isCurrentTriangle ? 3.0f : 1.5f,
                    isCurrentTriangle ? currentTriOutlineColor : triOutlineColor);
            DrawLineEx(
                    c,
                    a,
                    isCurrentTriangle ? 3.0f : 1.5f,
                    isCurrentTriangle ? currentTriOutlineColor : triOutlineColor);

            const std::string triLabel = std::to_string(i);
            DrawText(
                    triLabel.c_str(),
                    static_cast<int>(std::round(centroid.x + 4.0f)),
                    static_cast<int>(std::round(centroid.y - 6.0f)),
                    14,
                    isCurrentTriangle ? currentTriOutlineColor : triOutlineColor);
        }
    }

    // ---------------------------------------------------------------------
    // Final smoothed point path debug
    // ---------------------------------------------------------------------
    if (!npc.move.active || npc.move.pathPoints.empty()) {
        return;
    }

    Vector2 prev = TopdownWorldToScreen(state, npc.position);

    for (int i = npc.move.currentPoint; i < static_cast<int>(npc.move.pathPoints.size()); ++i) {
        const Vector2 p = TopdownWorldToScreen(state, npc.move.pathPoints[i]);

        const bool isCurrentTarget = (i == npc.move.currentPoint);
        const Color lineColor = isCurrentTarget
                                ? Color{255, 170, 40, 255}
                                : Color{255, 210, 120, 255};
        const Color pointColor = isCurrentTarget
                                 ? Color{255, 120, 0, 255}
                                 : Color{255, 230, 150, 255};
        const float lineThickness = isCurrentTarget ? 4.0f : 2.0f;
        const float pointRadius = isCurrentTarget ? 6.0f : 4.0f;

        DrawLineEx(prev, p, lineThickness, lineColor);
        DrawCircleV(p, pointRadius, pointColor);

        prev = p;
    }

    if (npc.move.hasFinalTarget) {
        const Vector2 finalP = TopdownWorldToScreen(state, npc.move.finalTarget);
        DrawCircleLines(
                static_cast<int>(std::round(finalP.x)),
                static_cast<int>(std::round(finalP.y)),
                8.0f,
                Color{255, 255, 255, 220});
    }
}

static const char* TopdownNpcAiModeToString(TopdownNpcAiMode mode)
{
    switch (mode) {
        case TopdownNpcAiMode::None:           return "none";
        case TopdownNpcAiMode::SeekAndDestroy: return "seek_destroy";
        default:                               return "unknown";
    }
}

static void DrawNpcAiDebug(const GameState& state)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible) {
            continue;
        }

        const Vector2 npcScreen = TopdownWorldToScreen(state, npc.position);

        const Color baseColor =
                npc.hostile
                ? Color{255, 110, 110, 255}
                : Color{110, 220, 255, 255};

        if (npc.visionRange > 0.0f) {
            DrawCircleLines(
                    static_cast<int>(std::round(npcScreen.x)),
                    static_cast<int>(std::round(npcScreen.y)),
                    npc.visionRange,
                    Color{255, 180, 80, 90});
        }

        if (npc.hearingRange > 0.0f) {
            DrawCircleLines(
                    static_cast<int>(std::round(npcScreen.x)),
                    static_cast<int>(std::round(npcScreen.y)),
                    npc.hearingRange,
                    Color{80, 180, 255, 90});
        }

        if (npc.attackRange > 0.0f) {
            DrawCircleLines(
                    static_cast<int>(std::round(npcScreen.x)),
                    static_cast<int>(std::round(npcScreen.y)),
                    npc.collisionRadius + player.radius + npc.attackRange,
                    Color{255, 60, 60, 110});
        }

        const float facingAngle = std::atan2(npc.facing.y, npc.facing.x);
        const float halfAngle = npc.visionHalfAngleDegrees * DEG2RAD;

        Vector2 leftRay{
                npc.position.x + std::cos(facingAngle - halfAngle) * std::min(npc.visionRange, 160.0f),
                npc.position.y + std::sin(facingAngle - halfAngle) * std::min(npc.visionRange, 160.0f)
        };
        Vector2 rightRay{
                npc.position.x + std::cos(facingAngle + halfAngle) * std::min(npc.visionRange, 160.0f),
                npc.position.y + std::sin(facingAngle + halfAngle) * std::min(npc.visionRange, 160.0f)
        };

        DrawLineEx(npcScreen, TopdownWorldToScreen(state, leftRay), 1.5f, Color{255, 180, 80, 180});
        DrawLineEx(npcScreen, TopdownWorldToScreen(state, rightRay), 1.5f, Color{255, 180, 80, 180});

        if (npc.hasPlayerTarget) {
            DrawLineEx(
                    npcScreen,
                    TopdownWorldToScreen(state, player.position),
                    1.5f,
                    Color{255, 60, 60, 180});
        }

        if (npc.hasPlayerTarget) {
            const Vector2 lastKnownScreen = TopdownWorldToScreen(state, npc.lastKnownPlayerPosition);
            DrawCircleLines(
                    static_cast<int>(std::round(lastKnownScreen.x)),
                    static_cast<int>(std::round(lastKnownScreen.y)),
                    10.0f,
                    WHITE);
        }

        const float distToPlayer =
                TopdownLength(TopdownSub(player.position, npc.position));

        const float distToLastKnown =
                TopdownLength(TopdownSub(npc.lastKnownPlayerPosition, npc.position));

        DrawText(
                TextFormat("ai=%s  hostile=%s  persistent=%s",
                           TopdownNpcAiModeToString(npc.aiMode),
                           npc.hostile ? "yes" : "no",
                           npc.persistentChase ? "yes" : "no"),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 34.0f),
                16,
                baseColor);

        DrawText(
                TextFormat("aware=%s  combat=%s  target=%s",
                           TopdownNpcAwarenessStateToString(npc.awarenessState),
                           TopdownNpcCombatStateToString(npc.combatState),
                           npc.hasPlayerTarget ? "yes" : "no"),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 52.0f),
                16,
                baseColor);

        DrawText(
                TextFormat("atkCd=%.0f  repath=%.0f  lose=%.0f",
                           npc.attackCooldownRemainingMs,
                           npc.repathTimerMs,
                           npc.loseTargetTimerMs),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 70.0f),
                16,
                baseColor);

        DrawText(
                TextFormat("distPlayer=%.0f  distLast=%.0f",
                           distToPlayer,
                           distToLastKnown),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 88.0f),
                16,
                baseColor);

        DrawText(
                TextFormat("progressT=%.0f  progressLast=%.0f",
                           npc.lostTargetProgressTimerMs,
                           npc.lostTargetLastDistance),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 106.0f),
                16,
                baseColor);
    }
}

static void DrawNpcDebug(const GameState& state)
{
    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active) {
            continue;
        }

        const Vector2 npcScreen = TopdownWorldToScreen(state, npc.position);

        const Color color = npc.visible
                            ? Color{120, 255, 180, 255}
                            : Color{120, 255, 180, 100};

        DrawCircleLines(
                static_cast<int>(std::round(npcScreen.x)),
                static_cast<int>(std::round(npcScreen.y)),
                npc.collisionRadius,
                color);

        Vector2 facingTip{
                npcScreen.x + npc.facing.x * 36.0f,
                npcScreen.y + npc.facing.y * 36.0f
        };
        DrawLineEx(npcScreen, facingTip, 2.0f, color);

        DrawCircleV(npcScreen, 3.0f, color);

        std::string line1 = npc.id + " [" + npc.assetId + "]";
        if (!npc.visible) {
            line1 += " hidden";
        }

        const std::string resolvedName = TopdownGetResolvedNpcAnimationName(npc);

        std::string line2 =
                "mode=" +
                std::string(
                        npc.animationMode == TopdownNpcAnimationMode::ScriptLoop
                        ? "script"
                        : "auto");

        if (npc.oneShotActive) {
            line2 += "  oneshot=" + npc.oneShotClip.clipName;
        }

        std::string line3 =
                "visible=" +
                (resolvedName.empty() ? std::string("<none>") : resolvedName);

        if (npc.move.active) {
            line3 += npc.move.running ? "  move=run" : "  move=walk";
        } else {
            line3 += "  move=idle";
        }

        std::string line4 =
                "pts=" + std::to_string(static_cast<int>(npc.move.pathPoints.size())) +
                "  tris=" + std::to_string(static_cast<int>(npc.move.debugTrianglePath.size()));

        DrawText(
                line1.c_str(),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y - 22.0f),
                16,
                color);

        DrawText(
                line2.c_str(),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y - 4.0f),
                16,
                color);

        DrawText(
                line3.c_str(),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 14.0f),
                16,
                color);

        DrawText(
                line4.c_str(),
                static_cast<int>(npcScreen.x + 10.0f),
                static_cast<int>(npcScreen.y + 32.0f),
                16,
                color);

        DrawSingleNpcPathDebug(state, npc);
    }
}

static const char* TopdownFireModeToString(TopdownFireMode mode)
{
    switch (mode) {
        case TopdownFireMode::SemiAuto: return "semi";
        case TopdownFireMode::FullAuto: return "full";
        case TopdownFireMode::Burst:    return "burst";
        default:                        return "unknown";
    }
}

static Vector2 BuildPlayerAimForward(const GameState& state)
{
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;

    Vector2 forward{
            std::cos(character.upperRotationRadians),
            std::sin(character.upperRotationRadians)
    };

    forward = TopdownNormalizeOrZero(forward);
    if (TopdownLengthSqr(forward) <= 0.000001f) {
        forward = state.topdown.runtime.player.facing;
    }
    if (TopdownLengthSqr(forward) <= 0.000001f) {
        forward = Vector2{1.0f, 0.0f};
    }

    return forward;
}

static bool TryBuildDebugMuzzleWorldPosition(
        const GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        Vector2& outMuzzleWorld)
{
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    const std::string attackAnimationId =
            FindTopdownPlayerEquipmentAttackAnimationId(
                    state,
                    character.equippedSetId,
                    TopdownAttackType::Ranged);

    if (!attackAnimationId.empty()) {
        const TopdownPlayerAnimationEntry* animationEntry =
                FindTopdownPlayerAnimationEntry(state, attackAnimationId);

        if (animationEntry != nullptr && animationEntry->hasMuzzle) {
            const SpriteAssetResource* sprite =
                    FindSpriteAssetResource(state.resources, animationEntry->spriteHandle);

            if (sprite != nullptr && sprite->loaded && sprite->hasExplicitOrigin) {
                const float drawScale = sprite->baseDrawScale;

                const float localX = (animationEntry->muzzle.x - sprite->origin.x) * drawScale;
                const float localY = (animationEntry->muzzle.y - sprite->origin.y) * drawScale;

                const float radians = character.upperRotationRadians;
                const Vector2 forward{ std::cos(radians), std::sin(radians) };
                const Vector2 right{ -forward.y, forward.x };

                outMuzzleWorld = TopdownAdd(
                        player.position,
                        TopdownAdd(
                                TopdownMul(forward, localX),
                                TopdownMul(right, localY)));

                return true;
            }
        }
    }

    const float radians = character.upperRotationRadians;
    const Vector2 forward{ std::cos(radians), std::sin(radians) };
    const Vector2 right{ -forward.y, forward.x };

    outMuzzleWorld = TopdownAdd(
            player.position,
            TopdownAdd(
                    TopdownMul(forward, weaponConfig.muzzleOrigin.x),
                    TopdownMul(right, weaponConfig.muzzleOrigin.y)));

    return true;
}

static bool DebugIsNpcInsideMeleeArc(
        const GameState& state,
        const TopdownNpcRuntime& npc,
        float meleeRange,
        float meleeArcDegrees)
{
    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    if (meleeRange <= 0.0f || meleeArcDegrees <= 0.0f) {
        return false;
    }

    Vector2 toCenter = TopdownSub(npc.position, player.position);
    const float centerDist = TopdownLength(toCenter);

    if (centerDist <= 0.000001f) {
        return true;
    }

    Vector2 dirToCenter = TopdownMul(toCenter, 1.0f / centerDist);

    Vector2 closestPointOnNpc = TopdownSub(
            npc.position,
            TopdownMul(dirToCenter, npc.collisionRadius));

    Vector2 toHitPoint = TopdownSub(closestPointOnNpc, player.position);
    const float hitPointDist = TopdownLength(toHitPoint);

    if (hitPointDist > meleeRange) {
        return false;
    }

    const Vector2 attackForward = BuildPlayerAimForward(state);
    Vector2 toHitDir = TopdownNormalizeOrZero(toHitPoint);

    if (TopdownLengthSqr(toHitDir) <= 0.000001f) {
        return true;
    }

    const float cosThreshold =
            std::cos((meleeArcDegrees * 0.5f) * DEG2RAD);

    return TopdownDot(attackForward, toHitDir) >= cosThreshold;
}

static TopdownNpcRuntime* DebugFindBestMeleeTarget(
        GameState& state,
        float meleeRange,
        float meleeArcDegrees)
{
    TopdownNpcRuntime* bestNpc = nullptr;
    float bestDistSqr = 0.0f;

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible) {
            continue;
        }

        if (!DebugIsNpcInsideMeleeArc(state, npc, meleeRange, meleeArcDegrees)) {
            continue;
        }

        const float distSqr =
                TopdownLengthSqr(TopdownSub(npc.position, player.position));

        if (bestNpc == nullptr || distSqr < bestDistSqr) {
            bestNpc = &npc;
            bestDistSqr = distSqr;
        }
    }

    return bestNpc;
}

static void DrawMeleeArcDebug(
        GameState& state,
        float meleeRange,
        float meleeArcDegrees,
        Color color,
        const char* label,
        int labelOffsetY)
{
    if (meleeRange <= 0.0f || meleeArcDegrees <= 0.0f) {
        return;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const Vector2 playerScreen = TopdownWorldToScreen(state, player.position);
    const Vector2 forward = BuildPlayerAimForward(state);

    const float baseAngle = std::atan2(forward.y, forward.x);
    const float halfArc = (meleeArcDegrees * 0.5f) * DEG2RAD;

    const int segments = 24;
    std::vector<Vector2> points;
    points.reserve(static_cast<size_t>(segments) + 2);

    points.push_back(playerScreen);

    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = baseAngle - halfArc + (halfArc * 2.0f) * t;

        Vector2 worldPoint{
                player.position.x + std::cos(angle) * meleeRange,
                player.position.y + std::sin(angle) * meleeRange
        };

        points.push_back(TopdownWorldToScreen(state, worldPoint));
    }

    for (size_t i = 1; i + 1 < points.size(); ++i) {
        DrawLineEx(points[i], points[i + 1], 2.0f, color);
    }

    DrawLineEx(points[0], points[1], 2.0f, color);
    DrawLineEx(points[0], points.back(), 2.0f, color);

    DrawText(
            TextFormat("%s  range=%.0f arc=%.0f",
                       label,
                       meleeRange,
                       meleeArcDegrees),
            20,
            labelOffsetY,
            20,
            color);

    for (TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || !npc.visible) {
            continue;
        }

        if (!DebugIsNpcInsideMeleeArc(state, npc, meleeRange, meleeArcDegrees)) {
            continue;
        }

        const Vector2 npcScreen = TopdownWorldToScreen(state, npc.position);
        DrawCircleLines(
                static_cast<int>(std::round(npcScreen.x)),
                static_cast<int>(std::round(npcScreen.y)),
                npc.collisionRadius + 4.0f,
                color);
    }

    TopdownNpcRuntime* best = DebugFindBestMeleeTarget(state, meleeRange, meleeArcDegrees);
    if (best != nullptr) {
        const Vector2 bestScreen = TopdownWorldToScreen(state, best->position);
        DrawCircleLines(
                static_cast<int>(std::round(bestScreen.x)),
                static_cast<int>(std::round(bestScreen.y)),
                best->collisionRadius + 10.0f,
                WHITE);

        DrawText(
                label,
                static_cast<int>(bestScreen.x + best->collisionRadius + 12.0f),
                static_cast<int>(bestScreen.y - 8.0f),
                16,
                color);
    }
}

static void DrawRangedDebug(
        const GameState& state,
        const TopdownPlayerWeaponConfig& weaponConfig,
        const char* label,
        int labelOffsetY)
{
    if (weaponConfig.maxRange <= 0.0f) {
        return;
    }

    const TopdownPlayerRuntime& player = state.topdown.runtime.player;
    const Vector2 forward = BuildPlayerAimForward(state);

    Vector2 muzzleWorld{};
    TryBuildDebugMuzzleWorldPosition(state, weaponConfig, muzzleWorld);

    const Vector2 rangeEnd = TopdownAdd(
            muzzleWorld,
            TopdownMul(forward, weaponConfig.maxRange));

    const Vector2 playerScreen = TopdownWorldToScreen(state, player.position);
    const Vector2 muzzleScreen = TopdownWorldToScreen(state, muzzleWorld);
    const Vector2 endScreen = TopdownWorldToScreen(state, rangeEnd);

    const Color color = Color{255, 220, 120, 255};

    DrawLineEx(playerScreen, TopdownAdd(playerScreen, TopdownMul(forward, 80.0f)), 2.0f, color);
    DrawLineEx(muzzleScreen, endScreen, 1.5f, Color{255, 220, 120, 140});
    DrawCircleV(muzzleScreen, 4.0f, color);

    DrawText(
            TextFormat("%s  range=%.0f spread=%.1f pellets=%d",
                       label,
                       weaponConfig.maxRange,
                       weaponConfig.spreadDegrees,
                       weaponConfig.pelletCount),
            20,
            labelOffsetY,
            20,
            color);
}

static void DrawScriptDebug(const GameState& state)
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

static void DrawCombatDebug(GameState& state)
{
    const TopdownCharacterRuntime& character = state.topdown.runtime.playerCharacter;
    const TopdownPlayerAttackRuntime& attack = state.topdown.runtime.playerAttack;

    const TopdownPlayerWeaponConfig* weaponConfig =
            FindTopdownPlayerWeaponConfigByEquipmentSetId(state, character.equippedSetId);

    if (weaponConfig == nullptr) {
        DrawText("combat: no weapon config for equipped set", 20, 100, 20, RED);
        return;
    }

    DrawText(
            TextFormat("weapon=%s  slot=%d  fireMode=%s  attackActive=%s  cooldown=%.0fms",
                       weaponConfig->equipmentSetId.c_str(),
                       weaponConfig->slot,
                       TopdownFireModeToString(attack.currentFireMode),
                       attack.active ? "yes" : "no",
                       attack.cooldownRemainingMs),
            20,
            100,
            20,
            SKYBLUE);

    int y = 125;

    if (weaponConfig->primaryAttackType == TopdownAttackType::Ranged) {
        DrawRangedDebug(state, *weaponConfig, "primary ranged", y);
        y += 24;
    } else if (weaponConfig->primaryAttackType == TopdownAttackType::Melee) {
        DrawMeleeArcDebug(
                state,
                weaponConfig->meleeRange,
                weaponConfig->meleeArcDegrees,
                Color{255, 110, 110, 255},
                "primary melee",
                y);
        y += 24;
    }

    if (weaponConfig->secondaryAttackType == TopdownAttackType::Ranged) {
        DrawRangedDebug(state, *weaponConfig, "secondary ranged", y);
        y += 24;
    } else if (weaponConfig->secondaryAttackType == TopdownAttackType::Melee) {
        DrawMeleeArcDebug(
                state,
                weaponConfig->meleeRange,
                weaponConfig->meleeArcDegrees,
                Color{110, 220, 255, 255},
                "secondary melee",
                y);
        y += 24;
    }
}

static const char* TopdownDoorHingeSideToString(TopdownDoorHingeSide side)
{
    switch (side) {
        case TopdownDoorHingeSide::Left:   return "left";
        case TopdownDoorHingeSide::Right:  return "right";
        case TopdownDoorHingeSide::Top:    return "top";
        case TopdownDoorHingeSide::Bottom: return "bottom";
        default:                           return "unknown";
    }
}

static void DrawDoorDebug(const GameState& state)
{
    const int count = static_cast<int>(state.topdown.authored.doors.size());

    for (int i = 0; i < count; ++i) {
        const TopdownAuthoredDoor& authored = state.topdown.authored.doors[i];
        const TopdownRuntimeDoor& runtime = state.topdown.runtime.doors[i];

        Vector2 a{};
        Vector2 b{};
        Vector2 c{};
        Vector2 d{};
        TopdownBuildDoorCorners(runtime, a, b, c, d);

        const Color slabColor{255, 90, 90, 255};
        const Color hingeColor{255, 255, 0, 255};
        const Color segmentColor{255, 200, 80, 255};

        DrawPolygonOutline(state, std::vector<Vector2>{a, b, c, d}, slabColor, 2.0f);

        const Vector2 hingeScreen = TopdownWorldToScreen(state, runtime.hinge);
        DrawCircleV(hingeScreen, 5.0f, hingeColor);

        const TopdownSegment centerSeg = TopdownBuildDoorCenterSegment(runtime);
        DrawLineEx(
                TopdownWorldToScreen(state, centerSeg.a),
                TopdownWorldToScreen(state, centerSeg.b),
                2.0f,
                segmentColor);

        Vector2 labelPos = TopdownWorldToScreen(state, runtime.hinge);
        labelPos.x += 10.0f;
        labelPos.y -= 18.0f;

        DrawText(
                authored.id.c_str(),
                static_cast<int>(labelPos.x),
                static_cast<int>(labelPos.y),
                16,
                slabColor);

        DrawText(
                TextFormat("hinge=%s  angle=%.1f  swing=[%.1f, %.1f]",
                           TopdownDoorHingeSideToString(authored.hingeSide),
                           runtime.angleRadians * RAD2DEG,
                           authored.swingMinDegrees,
                           authored.swingMaxDegrees),
                static_cast<int>(labelPos.x),
                static_cast<int>(labelPos.y + 18.0f),
                16,
                slabColor);

        DrawText(
                TextFormat("len=%.1f  thick=%.1f  autoClose=%s  locked=%s",
                           runtime.length,
                           runtime.thickness,
                           runtime.autoClose ? "yes" : "no",
                           runtime.locked ? "yes" : "no"),
                static_cast<int>(labelPos.x),
                static_cast<int>(labelPos.y + 36.0f),
                16,
                slabColor);
    }
}

void TopdownRenderDebug(GameState& state)
{
    if (!state.topdown.authored.loaded) {
        return;
    }

    if (state.topdown.runtime.debug.showBlockers) {
        DrawBlockerDebug(state);
    }

    if (state.topdown.runtime.debug.showNav) {
        DrawNavDebug(state);
    }

    if (state.topdown.runtime.debug.showSpawnPoints) {
        for (const TopdownAuthoredSpawn& spawn : state.topdown.authored.spawns) {
            const Vector2 spawnScreen = TopdownWorldToScreen(state, spawn.position);

            DrawCircleV(spawnScreen, 8.0f, SKYBLUE);
            DrawText(spawn.id.c_str(),
                     static_cast<int>(spawnScreen.x) + 10,
                     static_cast<int>(spawnScreen.y) - 10,
                     18,
                     SKYBLUE);

            const float radians = spawn.orientationDegrees * DEG2RAD;
            Vector2 tip{
                    spawnScreen.x + std::cos(radians) * 30.0f,
                    spawnScreen.y + std::sin(radians) * 30.0f
            };
            DrawLineEx(spawnScreen, tip, 2.0f, SKYBLUE);
        }
    }

    if (state.topdown.runtime.debug.showEffects) {
        DrawEffectRegionDebug(state);
    }

    if (state.topdown.runtime.debug.showTriggers) {
        DrawTriggerDebug(state);
    }

    if (state.topdown.runtime.debug.showImageLayers) {
        DrawImageLayerDebug(state);
    }

    if (state.topdown.runtime.debug.showPlayer) {
        DrawPlayerDebug(state);
        DrawNpcDebug(state);
    }

    if (state.topdown.runtime.debug.showCombatDebug) {
        DrawCombatDebug(state);
    }

    if (state.topdown.runtime.debug.showScriptDebug) {
        DrawScriptDebug(state);
    }

    if (state.topdown.runtime.debug.showAiDebug) {
        DrawNpcAiDebug(state);
    }

    if (state.topdown.runtime.debug.showDoors) {
        DrawDoorDebug(state);
    }
}
