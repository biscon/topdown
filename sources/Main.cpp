#include <raylib.h>
#include <cmath>
#include "data/GameState.h"
#include "menu/Menu.h"
#include "settings/Settings.h"
#include "input/Input.h"
#include "debug/DebugConsole.h"
#include "debug/DebugConsoleTraceLog.h"
#include "resources/Resources.h"
#include "scripting/ScriptSystem.h"
#include "audio/Audio.h"
#include "ui/Cursor.h"
#include "render/EffectShaderRegistry.h"
#include "topdown/TopdownMode.h"
#include "topdown/LevelRegistry.h"
#include "topdown/PlayerRegistry.h"
#include "topdown/BloodStampGeneration.h"
#include "topdown/BloodRenderTarget.h"
#include "topdown/TopdownPlayerVignette.h"
#include "topdown/TopdownRvo.h"

static Rectangle GetFullscreenSrcRect(const Texture2D& tex)
{
    return Rectangle{
            0.5f,
            0.5f,
            (float)tex.width  - 1.0f,
            -(float)tex.height + 1.0f
    };
}

static Rectangle BuildPresentationRect(float backbufferWidth, float backbufferHeight,
                                       float drawableWidth, float drawableHeight)
{
    const float backbufferAspect = backbufferWidth / backbufferHeight;
    const float drawableAspect = drawableWidth / drawableHeight;

    Rectangle dst{};

    if (drawableAspect > backbufferAspect) {
        dst.height = drawableHeight;
        dst.width = std::round(dst.height * backbufferAspect);
        dst.x = std::floor((drawableWidth - dst.width) * 0.5f);
        dst.y = 0.0f;
    } else {
        dst.width = drawableWidth;
        dst.height = std::round(dst.width / backbufferAspect);
        dst.x = 0.0f;
        dst.y = std::floor((drawableHeight - dst.height) * 0.5f);
    }

    return dst;
}

static Rectangle BuildShakenWorldDestRect(const GameState& state, const Rectangle& baseDst)
{
    Rectangle dst = baseDst;

    const TopdownScreenShakeState& shake = state.topdown.runtime.screenShake;
    if (!shake.active) {
        return dst;
    }

    const float scaleX = baseDst.width / static_cast<float>(INTERNAL_WIDTH);
    const float scaleY = baseDst.height / static_cast<float>(INTERNAL_HEIGHT);

    const float remaining01 = 1.0f - (shake.elapsedMs / std::max(shake.durationMs, 0.001f));
    const float fadedStrengthX = shake.strengthX * remaining01;
    const float fadedStrengthY = shake.strengthY * remaining01;

    const float offsetX = shake.currentOffset.x * scaleX;
    const float offsetY = shake.currentOffset.y * scaleY;

    const float padX = std::ceil(std::abs(fadedStrengthX) * scaleX);
    const float padY = std::ceil(std::abs(fadedStrengthY) * scaleY);

    dst.x -= padX;
    dst.y -= padY;
    dst.width += padX * 2.0f;
    dst.height += padY * 2.0f;

    dst.x += offsetX;
    dst.y += offsetY;

    return dst;
}

static void ProcessGameModeInput(GameState& state)
{
    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        if (ev.key.key == KEY_ESCAPE) {
            if (state.mode == GameMode::TopDown) {
                state.mode = GameMode::Menu;
                TraceLog(LOG_DEBUG, "Opening menu");
                ConsumeEvent(ev);
            } else if (state.mode == GameMode::Game) {
                state.mode = GameMode::Menu;
                TraceLog(LOG_DEBUG, "Opening menu");
                ConsumeEvent(ev);
            }
        }
    }
}

static bool LoadFonts(GameState& state) {
    const std::string fontPath = std::string(ASSETS_PATH) + "fonts/IBMPlexSans-Bold.ttf";
    state.dialogueFont = LoadFontEx(fontPath.c_str(), 46, nullptr, 0);
    if(state.dialogueFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load dialogue font from %s", fontPath.c_str());
        return false;
    }
    state.hoverLabelFont = LoadFontEx(fontPath.c_str(), 58, nullptr, 0);
    if(state.hoverLabelFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load hover label font from %s", fontPath.c_str());
        return false;
    }
    state.speechFont = LoadFontEx(fontPath.c_str(), 50, nullptr, 0);
    if(state.speechFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load speech font from %s", fontPath.c_str());
        return false;
    }
    state.ambientSpeechFont = LoadFontEx(fontPath.c_str(), 44, nullptr, 0);
    if(state.ambientSpeechFont.texture.id == 0) {
        TraceLog(LOG_ERROR, "Could not load ambient speech font from %s", fontPath.c_str());
        return false;
    }
    return true;
}

int main()
{
    GameState state;
    InitSettings(state.settings, "settings.json");

    unsigned int flags = 0;
    if (state.settings.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
#if defined(__APPLE__)
    flags |= FLAG_WINDOW_HIGHDPI;
#endif
    SetConfigFlags(flags);

    InstallDebugConsoleTraceLogHook();
    InitWindow(1920, 1080, "Adventure");

    {
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();
        const int renderW = GetRenderWidth();
        const int renderH = GetRenderHeight();
        const Vector2 dpi = GetWindowScaleDPI();
        const int monitor = GetCurrentMonitor();

        TraceLog(LOG_INFO, "==== DISPLAY INFO ====");
        TraceLog(LOG_INFO, "Monitor: %d", monitor);
        TraceLog(LOG_INFO, "Screen (logical): %d x %d", screenW, screenH);
        TraceLog(LOG_INFO, "Render (framebuffer): %d x %d", renderW, renderH);
        TraceLog(LOG_INFO, "DPI scale: %.2f x %.2f", dpi.x, dpi.y);
        TraceLog(LOG_INFO, "Monitor size: %d x %d",
                 GetMonitorWidth(monitor),
                 GetMonitorHeight(monitor));
        TraceLog(LOG_INFO, "======================");
    }

    SetExitKey(0);
    RefreshResolutions(state.settings);
    ApplySettings(state.settings);

    if (!InitEffectShaderRegistry()) {
        TraceLog(LOG_WARNING, "One or more effect shaders failed to load");
    }

    if (!InitTopdownBloodRenderTargetSystem()) {
        TraceLog(LOG_WARNING, "Topdown blood render target system failed to initialize");
    }

    if (!InitTopdownPlayerVignetteSystem()) {
        TraceLog(LOG_WARNING, "Topdown player vignette system failed to initialize");
    }

    TopdownRvoInit(state);

    RenderTexture2D worldTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D worldSampleTempTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldSampleTempTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D uiTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(uiTarget.texture, TEXTURE_FILTER_BILINEAR);

    InitInput(state.input);
    InitAudio(state);
    InitCursor(state);
    ScriptSystemInit(state);

    if(!LoadFonts(state)) {
        TraceLog(LOG_ERROR, "Font loading failed.");
        state.mode = GameMode::Quit;
    }

    if (!TopdownScanLevelRegistry(state)) {
        TraceLog(LOG_WARNING, "Topdown level registry scan failed");
    }

    if (!LoadTopdownPlayerCharacterAssets(state)) {
        TraceLog(LOG_WARNING, "Topdown player character assets failed to load");
    }

    if (!EnsureTopdownBloodStampLibraryGenerated(state.topdown.bloodStampLibrary)) {
        TraceLog(LOG_WARNING, "Failed generating topdown blood stamp library");
    }

    MenuInit(&state);
    DebugConsoleInit(state);
    FlushPendingDebugConsoleTraceLog(state);

    while (!WindowShouldClose())
    {
        if(state.mode == GameMode::Quit) break;

        const float dt = GetFrameTime();
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();

        const Rectangle dst = BuildPresentationRect(
                static_cast<float>(INTERNAL_WIDTH),
                static_cast<float>(INTERNAL_HEIGHT),
                static_cast<float>(screenW),
                static_cast<float>(screenH)
        );

        SetMouseOffset(
                -static_cast<int>(dst.x),
                -static_cast<int>(dst.y));

        SetMouseScale(
                static_cast<float>(INTERNAL_WIDTH) / dst.width,
                static_cast<float>(INTERNAL_HEIGHT) / dst.height
        );

        UpdateInput(state.input);
        UpdateCursor(state);

        FlushPendingDebugConsoleTraceLog(state);

        ProcessGameModeInput(state);
        UpdateDebugConsole(state, dt);
        MenuUpdate(dt);
        ScriptSystemUpdate(state, dt);

        if (state.topdown.hasPendingLevelChange) {
            const std::string levelId = state.topdown.pendingLevelId;
            const std::string spawnId = state.topdown.pendingSpawnId;

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
                TraceLog(LOG_ERROR, "Failed loading pending topdown level: %s", levelId.c_str());
            }
        }

        if(state.mode == GameMode::Menu) MenuHandleInput(state);

        if (state.mode == GameMode::TopDown) {
            TopdownHandleInput(state);
            TopdownUpdate(state, dt);
        }

        UpdateAudio(state, dt);

        if (state.mode == GameMode::TopDown) {
            TopdownRenderWorld(state, worldTarget, worldSampleTempTarget);

            BeginTextureMode(worldTarget);
            TopdownRenderDebug(state);
            EndTextureMode();

            BeginTextureMode(uiTarget);
            ClearBackground(BLANK);
            TopdownRenderUi(state);
            EndTextureMode();
        } else {
            BeginTextureMode(worldTarget);
            ClearBackground(BLACK);
            EndTextureMode();

            BeginTextureMode(uiTarget);
            ClearBackground(BLANK);
            EndTextureMode();
        }

        BeginTextureMode(uiTarget);
        if (state.mode == GameMode::Menu) {
            MenuRenderUi(state);
        }
        MenuRenderOverlay();
        RenderDebugConsole(state);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        Rectangle worldSrc = GetFullscreenSrcRect(worldTarget.texture);
        Rectangle shakenWorldDst = BuildShakenWorldDestRect(state, dst);
        DrawTexturePro(worldTarget.texture, worldSrc, shakenWorldDst, {0,0}, 0.0f, WHITE);

        Rectangle uiSrc = GetFullscreenSrcRect(uiTarget.texture);
        DrawTexturePro(uiTarget.texture, uiSrc, dst, {0,0}, 0.0f, WHITE);

        float scale = dst.width / INTERNAL_WIDTH;
        RenderCursor(state, scale);

        if(state.settings.showFPS) DrawFPS(10, 10);
        EndDrawing();
    }

    FlushPendingDebugConsoleTraceLog(state);

    ScriptSystemShutdown(state.script);
    DebugConsoleShutdown();
    ShutdownAudio(state);
    UnloadAllResources(state.resources);
    UnloadRenderTexture(worldTarget);
    UnloadRenderTexture(worldSampleTempTarget);
    UnloadRenderTexture(uiTarget);
    ShutdownTopdownBloodRenderTargetSystem();
    ShutdownTopdownPlayerVignetteSystem();
    ShutdownEffectShaderRegistry();
    ShutdownCursor(state);
    UnloadTopdownBloodRenderTarget(state);
    UnloadTopdownBloodStampLibrary(state.topdown.bloodStampLibrary);
    TopdownRvoShutdown(state);
    CloseWindow();

    return 0;
}
