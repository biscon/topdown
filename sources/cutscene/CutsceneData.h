#pragma once

#include <string>
#include <vector>

#include "resources/ResourceData.h"

struct CutsceneImageDefinition {
    std::string id;
    std::string path;
};

struct CutsceneDefinition {
    std::string cutsceneId;
    std::string descriptorPath;
    std::string directoryPath;

    std::string scriptPath;

    std::string skipLevelId;
    std::string skipSpawnId;

    int baseAssetScale = 1;

    std::vector<CutsceneImageDefinition> images;
};

struct CutsceneRuntimeImageLayer {
    bool active = false;
    std::string imageId;
    TextureHandle textureHandle = -1;
    float opacity = 0.0f;
    float fadeStartOpacity = 0.0f;
    float targetOpacity = 0.0f;
    float fadeTimerMs = 0.0f;
    float fadeDurationMs = 0.0f;
};

struct CutsceneRuntimeLoadedImage {
    std::string imageId;
    TextureHandle textureHandle = -1;
};

struct CutsceneRuntimeText {
    bool active = false;
    std::string text;

    float opacity = 0.0f;
    float fadeStartOpacity = 0.0f;
    float targetOpacity = 0.0f;
    float fadeTimerMs = 0.0f;
    float fadeDurationMs = 0.0f;
    bool clearWhenFadeComplete = false;
};

struct CutsceneRuntime {
    bool active = false;
    std::string cutsceneId;

    int baseAssetScale = 1;

    std::string scriptPath;
    std::string skipLevelId;
    std::string skipSpawnId;

    std::vector<CutsceneRuntimeLoadedImage> loadedImages;

    CutsceneRuntimeImageLayer imageA;
    CutsceneRuntimeImageLayer imageB;
    bool usingImageA = true;

    CutsceneRuntimeText text;

    float blackOpacity = 1.0f;
    float blackStartOpacity = 1.0f;
    float blackTargetOpacity = 0.0f;
    float blackFadeTimerMs = 0.0f;
    float blackFadeDurationMs = 0.0f;

    bool skipHeld = false;
    bool completeRequested = false;
    float skipHoldMs = 0.0f;
    float skipRequiredMs = 1200.0f;
};

struct CutsceneData {
    std::vector<CutsceneDefinition> registry;
    CutsceneRuntime runtime;
};
