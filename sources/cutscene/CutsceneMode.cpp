#include "cutscene/CutsceneMode.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "cutscene/CutsceneRegistry.h"
#include "input/Input.h"
#include "resources/TextureAsset.h"
#include "topdown/LevelRegistry.h"

namespace
{
    static float Clamp01(float value)
    {
        return std::max(0.0f, std::min(1.0f, value));
    }

    static unsigned char AlphaByte(float opacity)
    {
        return static_cast<unsigned char>(std::round(Clamp01(opacity) * 255.0f));
    }

    static void ReleaseCutsceneImageLayer(GameState& state, CutsceneRuntimeImageLayer& layer)
    {
        if (layer.ownsTexture && layer.textureHandle > 0) {
            ReleaseTextureAsset(state.resources, layer.textureHandle);
        }
        layer = CutsceneRuntimeImageLayer{};
    }

    static void ResetCutsceneRuntime(GameState& state)
    {
        ReleaseCutsceneImageLayer(state, state.cutscene.runtime.imageA);
        ReleaseCutsceneImageLayer(state, state.cutscene.runtime.imageB);
        state.cutscene.runtime = CutsceneRuntime{};
    }

    static TextureHandle FindExistingTextureHandle(
            const ResourceData& resources,
            const std::string& path,
            const TextureLoadSettings& settings)
    {
        const std::string normalizedPath = std::filesystem::path(path).lexically_normal().string();
        for (const TextureResource& resource : resources.textures) {
            if (resource.path == normalizedPath &&
                resource.premultiplyAlpha == settings.premultiplyAlpha &&
                resource.filterMode == settings.filter &&
                resource.wrapMode == settings.wrap) {
                return resource.handle;
            }
        }
        return -1;
    }

    static bool LoadFirstCutsceneImage(GameState& state, const CutsceneDefinition& definition)
    {
        if (definition.images.empty()) {
            TraceLog(LOG_WARNING,
                     "Cutscene '%s' has no images; stopping cutscene",
                     definition.cutsceneId.c_str());
            return false;
        }

        const CutsceneImageDefinition& image = definition.images.front();

        TextureLoadSettings settings{};
        settings.premultiplyAlpha = true;
        settings.filter = TextureFilterMode::Point;
        settings.wrap = TextureWrapMode::Clamp;

        const TextureHandle existingHandle = FindExistingTextureHandle(
                state.resources,
                image.path,
                settings);

        TextureHandle textureHandle = existingHandle;
        if (textureHandle <= 0) {
            textureHandle = LoadTextureAsset(
                    state.resources,
                    image.path.c_str(),
                    settings,
                    ResourceScope::Scene);
        }

        if (textureHandle <= 0) {
            TraceLog(LOG_ERROR,
                     "Cutscene '%s' failed loading image '%s' from %s",
                     definition.cutsceneId.c_str(),
                     image.id.c_str(),
                     image.path.c_str());
            return false;
        }

        CutsceneRuntime& runtime = state.cutscene.runtime;
        runtime.imageA.active = true;
        runtime.imageA.imageId = image.id;
        runtime.imageA.textureHandle = textureHandle;
        runtime.imageA.ownsTexture = existingHandle <= 0;
        runtime.imageA.opacity = 1.0f;
        runtime.imageA.targetOpacity = 1.0f;
        runtime.usingImageA = true;
        runtime.blackOpacity = 0.0f;

        return true;
    }

    static void DrawCutsceneImageLayer(GameState& state, const CutsceneRuntimeImageLayer& layer, int baseAssetScale)
    {
        if (!layer.active || layer.textureHandle <= 0 || layer.opacity <= 0.0f) {
            return;
        }

        const TextureResource* textureResource = FindTextureResource(state.resources, layer.textureHandle);
        if (textureResource == nullptr || !textureResource->loaded || textureResource->texture.id == 0) {
            return;
        }

        const Texture2D& texture = textureResource->texture;
        const float scaledWidth = std::round(static_cast<float>(texture.width * baseAssetScale));
        const float scaledHeight = std::round(static_cast<float>(texture.height * baseAssetScale));
        const float dstX = std::round((static_cast<float>(INTERNAL_WIDTH) - scaledWidth) * 0.5f);
        const float dstY = std::round((static_cast<float>(INTERNAL_HEIGHT) - scaledHeight) * 0.5f);

        Rectangle src{
                0.0f,
                0.0f,
                static_cast<float>(texture.width),
                static_cast<float>(texture.height)
        };
        Rectangle dst{dstX, dstY, scaledWidth, scaledHeight};
        DrawTexturePro(texture, src, dst, {0.0f, 0.0f}, 0.0f, Color{255, 255, 255, AlphaByte(layer.opacity)});
    }

    static void DrawSkipProgress(const GameState& state)
    {
        const CutsceneRuntime& runtime = state.cutscene.runtime;
        if (runtime.skipHoldMs <= 0.0f || runtime.skipRequiredMs <= 0.0f) {
            return;
        }

        const float progress = Clamp01(runtime.skipHoldMs / runtime.skipRequiredMs);
        const float barWidth = 320.0f;
        const float barHeight = 10.0f;
        const float x = std::round(static_cast<float>(INTERNAL_WIDTH) - barWidth - 72.0f);
        const float y = std::round(static_cast<float>(INTERNAL_HEIGHT) - 88.0f);

        DrawRectangleRec(Rectangle{x - 2.0f, y - 2.0f, barWidth + 4.0f, barHeight + 4.0f}, Color{0, 0, 0, 210});
        DrawRectangleRec(Rectangle{x, y, barWidth, barHeight}, Color{28, 24, 22, 230});
        DrawRectangleRec(Rectangle{x, y, std::round(barWidth * progress), barHeight}, Color{236, 190, 98, 255});

        const char* label = "HOLD SPACE";
        Font font = state.speechFont;
        const float fontSize = 28.0f;
        const float spacing = 1.0f;
        const Vector2 textSize = MeasureTextEx(font, label, fontSize, spacing);
        const Vector2 textPos{std::round(x + barWidth - textSize.x), std::round(y - textSize.y - 8.0f)};
        DrawTextEx(font, label, Vector2{textPos.x + 2.0f, textPos.y + 2.0f}, fontSize, spacing, BLACK);
        DrawTextEx(font, label, textPos, fontSize, spacing, Color{244, 228, 186, 255});
    }

    static void CompleteCutsceneSkip(GameState& state)
    {
        const std::string levelId = state.cutscene.runtime.skipLevelId;
        const std::string spawnId = state.cutscene.runtime.skipSpawnId;

        ResetCutsceneRuntime(state);

        if (levelId.empty()) {
            TraceLog(LOG_WARNING, "Cutscene ended without a skip target level; returning to menu");
            state.mode = GameMode::Menu;
            return;
        }

        state.topdown.hasPendingLevelChange = false;
        state.topdown.pendingLevelId.clear();
        state.topdown.pendingSpawnId.clear();

        bool loaded = false;
        if (!spawnId.empty()) {
            loaded = TopdownLoadLevelById(state, levelId.c_str(), spawnId.c_str());
        } else {
            loaded = TopdownLoadLevelById(state, levelId.c_str());
        }

        if (loaded) {
            state.mode = GameMode::TopDown;
        } else {
            TraceLog(LOG_ERROR, "Cutscene failed loading target topdown level: %s", levelId.c_str());
            state.mode = GameMode::Menu;
        }
    }
}

bool CutsceneStart(GameState& state, const std::string& cutsceneId)
{
    const CutsceneDefinition* definition = FindCutsceneDefinitionById(state, cutsceneId);
    if (definition == nullptr) {
        TraceLog(LOG_WARNING, "Cannot start unknown cutscene: %s", cutsceneId.c_str());
        return false;
    }

    ResetCutsceneRuntime(state);

    CutsceneRuntime& runtime = state.cutscene.runtime;
    runtime.active = true;
    runtime.cutsceneId = definition->cutsceneId;
    runtime.baseAssetScale = definition->baseAssetScale;
    runtime.scriptPath = definition->scriptPath;
    runtime.skipLevelId = definition->skipLevelId;
    runtime.skipSpawnId = definition->skipSpawnId;

    if (!LoadFirstCutsceneImage(state, *definition)) {
        ResetCutsceneRuntime(state);
        return false;
    }

    state.mode = GameMode::Cutscene;

    return true;
}

void CutsceneStop(GameState& state)
{
    ResetCutsceneRuntime(state);
    if (state.mode == GameMode::Cutscene) {
        state.mode = GameMode::Menu;
    }
}

void CutsceneUpdate(GameState& state, float dt)
{
    CutsceneRuntime& runtime = state.cutscene.runtime;
    if (!runtime.active) {
        return;
    }

    if (runtime.skipHeld) {
        runtime.skipHoldMs = std::min(runtime.skipRequiredMs, runtime.skipHoldMs + dt * 1000.0f);
    } else {
        runtime.skipHoldMs = 0.0f;
    }

    if (runtime.skipHoldMs >= runtime.skipRequiredMs) {
        CompleteCutsceneSkip(state);
        return;
    }
}

void CutsceneHandleInput(GameState& state)
{
    CutsceneRuntime& runtime = state.cutscene.runtime;
    if (!runtime.active) {
        return;
    }

    runtime.skipHeld = IsKeyDown(KEY_SPACE);

    for (InputEvent& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if (ev.key.key == KEY_SPACE || ev.key.key == KEY_ESCAPE) {
            ConsumeEvent(ev);
        }
    }

    for (InputEvent& ev : FilterEvents(state.input, true, InputEventType::KeyRepeated)) {
        if (ev.key.key == KEY_SPACE || ev.key.key == KEY_ESCAPE) {
            ConsumeEvent(ev);
        }
    }

    for (InputEvent& ev : FilterEvents(state.input, true, InputEventType::KeyReleased)) {
        if (ev.key.key == KEY_SPACE || ev.key.key == KEY_ESCAPE) {
            ConsumeEvent(ev);
        }
    }
}

void CutsceneRenderUi(GameState& state)
{
    const CutsceneRuntime& runtime = state.cutscene.runtime;
    if (!runtime.active) {
        return;
    }

    const CutsceneRuntimeImageLayer& activeLayer = runtime.usingImageA ? runtime.imageA : runtime.imageB;
    DrawCutsceneImageLayer(state, activeLayer, runtime.baseAssetScale);

    if (runtime.blackOpacity > 0.0f) {
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, Color{0, 0, 0, AlphaByte(runtime.blackOpacity)});
    }

    DrawSkipProgress(state);
}

bool CutsceneIsActive(const GameState& state)
{
    return state.cutscene.runtime.active;
}
