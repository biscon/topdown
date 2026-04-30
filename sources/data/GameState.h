//
// Created by bison on 09-03-26.
//
#pragma once

#include "settings/SettingsData.h"
#include "input/InputData.h"
#include "resources/ResourceData.h"
#include "debug/DebugData.h"
#include "scripting/ScriptData.h"
#include "audio/AudioData.h"
#include "ui/CursorData.h"
#include "topdown/TopdownData.h"

static constexpr int INTERNAL_WIDTH = 1920;
static constexpr int INTERNAL_HEIGHT = 1080;

enum class GameMode {
    Menu,
    Game,
    TopDown,
    Quit
};

struct GameState {
    SettingsData settings;
    InputData input;
    GameMode mode = GameMode::Menu;

    ResourceData resources;
    DebugData debug;
    ScriptData script;
    AudioData audio;
    CursorData cursor;
    Font dialogueFont;
    Font speechFont;
    Font ambientSpeechFont;
    Font hoverLabelFont;
    Font narrationTitleFont;
    Font narrationBodyFont;
    Font interactivePromptFont;

    TopdownData topdown;
};
