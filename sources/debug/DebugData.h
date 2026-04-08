#pragma once

#include <string>
#include <vector>
#include "raylib.h"

struct DebugConsoleLine {
    std::string text;
    Color color = LIGHTGRAY;
};

struct DebugConsoleData {
    bool open = false;

    std::string input;
    std::vector<DebugConsoleLine> lines;

    std::vector<std::string> history;
    int historyIndex = -1;

    std::string historyDraftInput;
    int historyDraftCaretIndex = 0;
    bool historyBrowsing = false;

    int scrollOffset = 0;

    float caretBlinkMs = 0.0f;
    bool caretVisible = true;

    int maxLines = 2000;
    int visibleLines = 20;
    int caretIndex = 0;
};

struct DebugData {
    bool showWalkPolygons = false;
    bool showNavTriangles = false;
    bool showNavAdjacency = false;
    bool showPath = false;
    bool showFeetPoints = false;
    bool showScaleInfo = false;
    bool showTrianglePath = false;
    bool showSceneObjects = false;
    bool showEffects = false;
    bool showScripts = false;
    DebugConsoleData console{};
};
