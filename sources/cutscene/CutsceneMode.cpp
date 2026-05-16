#include "cutscene/CutsceneMode.h"

#include <algorithm>
#include <cmath>
#include "cutscene/CutsceneRegistry.h"
#include "input/Input.h"
#include "resources/TextureAsset.h"
#include "resources/Resources.h"
#include "scripting/ScriptSystem.h"
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

    static float ClampFadeMs(float fadeMs)
    {
        return std::max(0.0f, fadeMs);
    }

    static void StartLayerFade(CutsceneRuntimeImageLayer& layer, float targetOpacity, float fadeMs)
    {
        layer.fadeStartOpacity = layer.opacity;
        layer.targetOpacity = Clamp01(targetOpacity);
        layer.fadeTimerMs = 0.0f;
        layer.fadeDurationMs = ClampFadeMs(fadeMs);
        if (layer.fadeDurationMs <= 0.0f) {
            layer.opacity = layer.targetOpacity;
            layer.fadeStartOpacity = layer.targetOpacity;
        }
    }

    static void StartTextFade(CutsceneRuntimeText& text, float targetOpacity, float fadeMs)
    {
        text.fadeStartOpacity = text.opacity;
        text.targetOpacity = Clamp01(targetOpacity);
        text.fadeTimerMs = 0.0f;
        text.fadeDurationMs = ClampFadeMs(fadeMs);
        if (text.fadeDurationMs <= 0.0f) {
            text.opacity = text.targetOpacity;
            text.fadeStartOpacity = text.targetOpacity;
        }
    }

    static void StartBlackFade(CutsceneRuntime& runtime, float targetOpacity, float fadeMs)
    {
        runtime.blackStartOpacity = runtime.blackOpacity;
        runtime.blackTargetOpacity = Clamp01(targetOpacity);
        runtime.blackFadeTimerMs = 0.0f;
        runtime.blackFadeDurationMs = ClampFadeMs(fadeMs);
        if (runtime.blackFadeDurationMs <= 0.0f) {
            runtime.blackOpacity = runtime.blackTargetOpacity;
            runtime.blackStartOpacity = runtime.blackTargetOpacity;
        }
    }

    static bool IsFadeActive(float fadeStartOpacity, float targetOpacity, float fadeTimerMs, float fadeDurationMs)
    {
        return fadeDurationMs > 0.0f &&
               fadeTimerMs < fadeDurationMs &&
               std::fabs(fadeStartOpacity - targetOpacity) > 0.001f;
    }

    static bool IsLayerFadeActive(const CutsceneRuntimeImageLayer& layer)
    {
        return layer.active && IsFadeActive(layer.fadeStartOpacity, layer.targetOpacity, layer.fadeTimerMs, layer.fadeDurationMs);
    }

    static bool IsTextFadeActive(const CutsceneRuntimeText& text)
    {
        return text.active && IsFadeActive(text.fadeStartOpacity, text.targetOpacity, text.fadeTimerMs, text.fadeDurationMs);
    }

    static void UpdateFadeValue(float& opacity, float fadeStartOpacity, float targetOpacity, float& fadeTimerMs, float fadeDurationMs, float dtMs)
    {
        if (fadeDurationMs <= 0.0f) {
            opacity = targetOpacity;
            return;
        }

        fadeTimerMs = std::min(fadeDurationMs, fadeTimerMs + dtMs);
        const float t = Clamp01(fadeTimerMs / fadeDurationMs);
        if (t >= 1.0f) {
            opacity = targetOpacity;
        } else {
            opacity = fadeStartOpacity + (targetOpacity - fadeStartOpacity) * t;
        }
    }

    static void UpdateImageLayer(CutsceneRuntimeImageLayer& layer, float dtMs)
    {
        if (!layer.active) {
            return;
        }

        UpdateFadeValue(layer.opacity, layer.fadeStartOpacity, layer.targetOpacity, layer.fadeTimerMs, layer.fadeDurationMs, dtMs);
        if (layer.opacity <= 0.001f && layer.targetOpacity <= 0.001f && layer.fadeTimerMs >= layer.fadeDurationMs) {
            layer = CutsceneRuntimeImageLayer{};
        }
    }

    static void UpdateText(CutsceneRuntimeText& text, float dtMs)
    {
        if (!text.active) {
            return;
        }

        UpdateFadeValue(text.opacity, text.fadeStartOpacity, text.targetOpacity, text.fadeTimerMs, text.fadeDurationMs, dtMs);
        if (text.clearWhenFadeComplete && text.opacity <= 0.001f && text.fadeTimerMs >= text.fadeDurationMs) {
            text = CutsceneRuntimeText{};
        }
    }

    static void ResetCutsceneRuntime(GameState& state)
    {
        state.cutscene.runtime = CutsceneRuntime{};
    }

    static const CutsceneRuntimeLoadedImage* FindLoadedCutsceneImage(
            const CutsceneRuntime& runtime,
            const std::string& imageId)
    {
        for (const CutsceneRuntimeLoadedImage& image : runtime.loadedImages) {
            if (image.imageId == imageId) {
                return &image;
            }
        }

        return nullptr;
    }

    static bool PreloadCutsceneImages(GameState& state, const CutsceneDefinition& definition)
    {
        CutsceneRuntime& runtime = state.cutscene.runtime;
        runtime.loadedImages.clear();
        runtime.loadedImages.reserve(definition.images.size());

        TextureLoadSettings settings{};
        settings.premultiplyAlpha = true;
        settings.filter = TextureFilterMode::Point;
        settings.wrap = TextureWrapMode::Clamp;

        for (const CutsceneImageDefinition& image : definition.images) {
            const TextureHandle textureHandle = LoadTextureAsset(
                    state.resources,
                    image.path.c_str(),
                    settings,
                    ResourceScope::Scene);

            if (textureHandle < 0) {
                TraceLog(LOG_ERROR,
                         "Cutscene '%s' failed preloading image '%s' from %s",
                         definition.cutsceneId.c_str(),
                         image.id.c_str(),
                         image.path.c_str());
                return false;
            }

            CutsceneRuntimeLoadedImage loadedImage;
            loadedImage.imageId = image.id;
            loadedImage.textureHandle = textureHandle;
            runtime.loadedImages.push_back(loadedImage);
        }

        return true;
    }

    static bool ShowFirstCutsceneImage(GameState& state, const CutsceneDefinition& definition)
    {
        if (definition.images.empty()) {
            TraceLog(LOG_WARNING,
                     "Cutscene '%s' has no images; stopping cutscene",
                     definition.cutsceneId.c_str());
            return false;
        }

        CutsceneRuntime& runtime = state.cutscene.runtime;
        const CutsceneImageDefinition& image = definition.images.front();
        const CutsceneRuntimeLoadedImage* loadedImage = FindLoadedCutsceneImage(runtime, image.id);
        if (loadedImage == nullptr || loadedImage->textureHandle < 0) {
            TraceLog(LOG_WARNING,
                     "Cutscene '%s' first image '%s' was not preloaded",
                     runtime.cutsceneId.c_str(),
                     image.id.c_str());
            return false;
        }

        runtime.imageA.active = true;
        runtime.imageA.imageId = image.id;
        runtime.imageA.textureHandle = loadedImage->textureHandle;
        runtime.imageA.opacity = 1.0f;
        runtime.imageA.targetOpacity = 1.0f;
        runtime.usingImageA = true;
        runtime.blackOpacity = 0.0f;

        return true;
    }

    static void DrawCutsceneImageLayer(GameState& state, const CutsceneRuntimeImageLayer& layer, int baseAssetScale)
    {
        if (!layer.active || layer.textureHandle < 0 || layer.opacity <= 0.0f) {
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
        const Font& font = state.narrationBodyFont;
        const float fontSize = 32.0f;
        const float spacing = 1.0f;
        const Vector2 textSize = MeasureTextEx(font, label, fontSize, spacing);
        const Vector2 textPos{std::round(x + barWidth - textSize.x), std::round(y - textSize.y - 8.0f)};
        DrawTextEx(font, label, Vector2{textPos.x + 2.0f, textPos.y + 2.0f}, fontSize, spacing, BLACK);
        DrawTextEx(font, label, textPos, fontSize, spacing, Color{244, 228, 186, 255});
    }

    static void CompleteCutsceneTarget(GameState& state)
    {
        const std::string levelId = state.cutscene.runtime.skipLevelId;
        const std::string spawnId = state.cutscene.runtime.skipSpawnId;

        ResetCutsceneRuntime(state);

        if (levelId.empty()) {
            TraceLog(LOG_WARNING, "Cutscene ended without a target level; returning to menu");
            UnloadSceneResources(state.resources);
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
            UnloadSceneResources(state.resources);
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

    const bool wasAlreadyInCutsceneMode = (state.mode == GameMode::Cutscene);
    ResetCutsceneRuntime(state);

    CutsceneRuntime& runtime = state.cutscene.runtime;
    runtime.active = true;
    runtime.cutsceneId = definition->cutsceneId;
    runtime.baseAssetScale = definition->baseAssetScale;
    runtime.scriptPath = definition->scriptPath;
    runtime.skipLevelId = definition->skipLevelId;
    runtime.skipSpawnId = definition->skipSpawnId;

    if (!PreloadCutsceneImages(state, *definition)) {
        ResetCutsceneRuntime(state);
        if (wasAlreadyInCutsceneMode) {
            state.mode = GameMode::Menu;
        }
        return false;
    }

    state.mode = GameMode::Cutscene;

    bool startedScript = false;
    if (!runtime.scriptPath.empty()) {
        if (state.script.vm != nullptr) {
            lua_pushnil(state.script.vm);
            lua_setglobal(state.script.vm, "Cutscene_onEnter");
        }

        if (ScriptSystemRunFile(state.script, runtime.scriptPath)) {
            const ScriptCallResult scriptResult = ScriptSystemCallTrigger(state, "Cutscene_onEnter");
            startedScript = (scriptResult == ScriptCallResult::StartedAsync ||
                             scriptResult == ScriptCallResult::ImmediateTrue ||
                             scriptResult == ScriptCallResult::ImmediateFalse);
            if (!startedScript) {
                TraceLog(LOG_WARNING,
                         "Cutscene '%s' script did not start Cutscene_onEnter; using fallback image",
                         runtime.cutsceneId.c_str());
            }
        } else {
            TraceLog(LOG_WARNING,
                     "Cutscene '%s' failed loading script %s; using fallback image",
                     runtime.cutsceneId.c_str(),
                     runtime.scriptPath.c_str());
        }
    }

    if (!startedScript) {
        if (!ShowFirstCutsceneImage(state, *definition)) {
            ResetCutsceneRuntime(state);
            state.mode = GameMode::Menu;
            return false;
        }
    }

    return true;
}

void CutsceneStop(GameState& state)
{
    ResetCutsceneRuntime(state);
    if (state.mode == GameMode::Cutscene) {
        ScriptSystemStopFunction(state.script, "Cutscene_onEnter");
        UnloadSceneResources(state.resources);
        state.mode = GameMode::Menu;
    }
}

void CutsceneComplete(GameState& state)
{
    CompleteCutsceneTarget(state);
}

bool CutsceneRequestComplete(GameState& state)
{
    CutsceneRuntime& runtime = state.cutscene.runtime;
    if (!runtime.active) {
        return false;
    }

    runtime.completeRequested = true;
    return true;
}

bool CutsceneShowImage(GameState& state, const std::string& imageId, float fadeMs)
{
    CutsceneRuntime& runtime = state.cutscene.runtime;
    if (!runtime.active) {
        return false;
    }

    const CutsceneRuntimeLoadedImage* image = FindLoadedCutsceneImage(runtime, imageId);
    if (image == nullptr) {
        TraceLog(LOG_WARNING,
                 "Cutscene '%s' missing preloaded image id '%s'",
                 runtime.cutsceneId.c_str(),
                 imageId.c_str());
        return false;
    }

    const TextureHandle textureHandle = image->textureHandle;
    if (textureHandle < 0) {
        TraceLog(LOG_WARNING,
                 "Cutscene '%s' preloaded image '%s' has an invalid texture handle",
                 runtime.cutsceneId.c_str(),
                 imageId.c_str());
        return false;
    }

    CutsceneRuntimeImageLayer& currentLayer = runtime.usingImageA ? runtime.imageA : runtime.imageB;
    CutsceneRuntimeImageLayer& nextLayer = runtime.usingImageA ? runtime.imageB : runtime.imageA;

    if (!currentLayer.active) {
        currentLayer.active = true;
        currentLayer.imageId = imageId;
        currentLayer.textureHandle = textureHandle;
        currentLayer.opacity = 0.0f;
        StartLayerFade(currentLayer, 1.0f, fadeMs);
        return true;
    }

    nextLayer = CutsceneRuntimeImageLayer{};
    nextLayer.active = true;
    nextLayer.imageId = imageId;
    nextLayer.textureHandle = textureHandle;
    nextLayer.opacity = 0.0f;
    StartLayerFade(nextLayer, 1.0f, fadeMs);
    StartLayerFade(currentLayer, 0.0f, fadeMs);
    runtime.usingImageA = !runtime.usingImageA;
    return true;
}

bool CutsceneShowText(GameState& state, const std::string& text, float fadeMs)
{
    CutsceneRuntimeText& runtimeText = state.cutscene.runtime.text;
    runtimeText.active = true;
    runtimeText.text = text;
    runtimeText.clearWhenFadeComplete = false;
    StartTextFade(runtimeText, 1.0f, fadeMs);
    return true;
}

bool CutsceneClearText(GameState& state, float fadeMs)
{
    CutsceneRuntimeText& runtimeText = state.cutscene.runtime.text;
    if (!runtimeText.active) {
        return true;
    }

    runtimeText.clearWhenFadeComplete = true;
    StartTextFade(runtimeText, 0.0f, fadeMs);
    return true;
}

bool CutsceneFadeFromBlack(GameState& state, float fadeMs)
{
    if (!state.cutscene.runtime.active) {
        return false;
    }
    StartBlackFade(state.cutscene.runtime, 0.0f, fadeMs);
    return true;
}

bool CutsceneFadeToBlack(GameState& state, float fadeMs)
{
    if (!state.cutscene.runtime.active) {
        return false;
    }
    StartBlackFade(state.cutscene.runtime, 1.0f, fadeMs);
    return true;
}

bool CutsceneHasActiveAction(const GameState& state)
{
    const CutsceneRuntime& runtime = state.cutscene.runtime;
    return IsLayerFadeActive(runtime.imageA) ||
           IsLayerFadeActive(runtime.imageB) ||
           IsTextFadeActive(runtime.text) ||
           IsFadeActive(runtime.blackStartOpacity, runtime.blackTargetOpacity, runtime.blackFadeTimerMs, runtime.blackFadeDurationMs);
}

void CutsceneUpdate(GameState& state, float dt)
{
    CutsceneRuntime& runtime = state.cutscene.runtime;
    if (!runtime.active) {
        return;
    }

    const float dtMs = dt * 1000.0f;
    UpdateImageLayer(runtime.imageA, dtMs);
    UpdateImageLayer(runtime.imageB, dtMs);
    UpdateText(runtime.text, dtMs);
    UpdateFadeValue(runtime.blackOpacity, runtime.blackStartOpacity, runtime.blackTargetOpacity, runtime.blackFadeTimerMs, runtime.blackFadeDurationMs, dtMs);

    if (runtime.skipHeld) {
        runtime.skipHoldMs = std::min(runtime.skipRequiredMs, runtime.skipHoldMs + dtMs);
    } else {
        runtime.skipHoldMs = 0.0f;
    }

    if (runtime.skipHoldMs >= runtime.skipRequiredMs) {
        ScriptSystemStopFunction(state.script, "Cutscene_onEnter");
        CompleteCutsceneTarget(state);
        return;
    }

    if (runtime.completeRequested) {
        ScriptSystemStopFunction(state.script, "Cutscene_onEnter");
        CompleteCutsceneTarget(state);
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

    const CutsceneRuntimeImageLayer& bottomLayer = runtime.usingImageA ? runtime.imageB : runtime.imageA;
    const CutsceneRuntimeImageLayer& topLayer = runtime.usingImageA ? runtime.imageA : runtime.imageB;
    DrawCutsceneImageLayer(state, bottomLayer, runtime.baseAssetScale);
    DrawCutsceneImageLayer(state, topLayer, runtime.baseAssetScale);

    if (runtime.blackOpacity > 0.0f) {
        DrawRectangle(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT, Color{0, 0, 0, AlphaByte(runtime.blackOpacity)});
    }

    if (runtime.text.active && !runtime.text.text.empty() && runtime.text.opacity > 0.0f) {
        const Font& font = state.narrationBodyFont;
        const float fontSize = 32.0f;
        const float spacing = 1.0f;
        const Vector2 textSize = MeasureTextEx(font, runtime.text.text.c_str(), fontSize, spacing);
        const Vector2 textPos{
                std::round((static_cast<float>(INTERNAL_WIDTH) - textSize.x) * 0.5f),
                std::round(static_cast<float>(INTERNAL_HEIGHT) * 0.88f)
        };
        const unsigned char alpha = AlphaByte(runtime.text.opacity);
        DrawTextEx(font, runtime.text.text.c_str(), Vector2{textPos.x + 2.0f, textPos.y + 2.0f}, fontSize, spacing, Color{0, 0, 0, alpha});
        DrawTextEx(font, runtime.text.text.c_str(), textPos, fontSize, spacing, Color{244, 228, 186, alpha});
    }

    DrawSkipProgress(state);
}

bool CutsceneIsActive(const GameState& state)
{
    return state.cutscene.runtime.active;
}
