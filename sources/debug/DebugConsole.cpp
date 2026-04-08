#include "debug/DebugConsole.h"
#include "debug/DebugConsoleInternal.h"

#include <string>
#include <vector>

#include "raylib.h"

static Font gConsoleFont{};
static bool gConsoleFontLoaded = false;

Font DebugConsoleGetActiveFont()
{
    return gConsoleFontLoaded ? gConsoleFont : GetFontDefault();
}

void DebugConsoleAddLine(GameState& state, const std::string& text, Color color)
{
    DebugConsoleData& console = state.debug.console;

    const int previousLineCount = static_cast<int>(console.lines.size());
    const bool wasAtBottom = (console.scrollOffset == 0);

    const std::vector<std::string> wrappedLines = WrapConsoleDisplayText(text);
    const int addedLineCount = static_cast<int>(wrappedLines.size());

    for (const std::string& wrapped : wrappedLines) {
        DebugConsoleLine line;
        line.text = wrapped;
        line.color = color;
        console.lines.push_back(line);
    }

    while (static_cast<int>(console.lines.size()) > console.maxLines) {
        console.lines.erase(console.lines.begin());
    }

    if (wasAtBottom) {
        console.scrollOffset = 0;
    } else {
        const int linesActuallyAdded =
                static_cast<int>(console.lines.size()) - previousLineCount;

        if (linesActuallyAdded > 0) {
            console.scrollOffset += linesActuallyAdded;
        }
    }

    DebugConsoleClampState(console);
}

void UpdateDebugConsole(GameState& state, float dt)
{
    UpdateDebugConsoleInputInternal(state, dt);
}

void RenderDebugConsole(const GameState& state)
{
    RenderDebugConsoleInternal(state);
}

void DebugConsoleInit(GameState& state)
{
    state.debug.console = {};
    state.debug.console.caretVisible = true;
    state.debug.console.caretIndex = 0;

    const std::string fontPath = std::string(ASSETS_PATH) + "fonts/Inconsolata.otf";
    gConsoleFont = LoadFontEx(fontPath.c_str(), CONSOLE_FONT_SIZE, nullptr, 0);
    gConsoleFontLoaded = (gConsoleFont.texture.id != 0);

    if (gConsoleFontLoaded) {
        SetTextureFilter(gConsoleFont.texture, TEXTURE_FILTER_BILINEAR);
        DebugConsoleAddLine(state, std::string("Loaded console font: ") + fontPath, SKYBLUE);
    } else {
        DebugConsoleAddLine(state, std::string("Failed to load console font: ") + fontPath, RED);
    }
}

void DebugConsoleShutdown()
{
    if (gConsoleFontLoaded) {
        UnloadFont(gConsoleFont);
        gConsoleFont = {};
        gConsoleFontLoaded = false;
    }
}
