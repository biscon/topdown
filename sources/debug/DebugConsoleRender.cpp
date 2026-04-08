#include "debug/DebugConsoleInternal.h"

#include <algorithm>
#include <string>
#include <vector>

#include "raylib.h"

struct ConsoleWrappedInputLine {
    int startCharIndex = 0;
    int endCharIndex = 0;
    std::string prefix;
    std::string text;
};

struct ConsoleInputLayout {
    std::vector<ConsoleWrappedInputLine> lines;
    int visibleStartLine = 0;
    int visibleLineCount = 1;
};

static float MeasureConsoleTextWidth(const Font& font,
                                     const std::string& text,
                                     float fontSize,
                                     float spacing)
{
    if (text.empty()) {
        return 0.0f;
    }

    return MeasureTextEx(font, text.c_str(), fontSize, spacing).x;
}

std::vector<std::string> WrapConsoleDisplayText(const std::string& text,
                                                const std::string& firstLinePrefix,
                                                const std::string& continuationPrefix)
{
    const Font font = DebugConsoleGetActiveFont();
    const float fontSize = 22.0f;
    const float spacing = 1.0f;

    const float panelWidth = static_cast<float>(INTERNAL_WIDTH - CONSOLE_PADDING * 2);
    const float availableWidth = panelWidth - 20.0f;
    const float clampedWidth = std::max(1.0f, availableWidth);

    std::vector<std::string> out;

    if (text.empty()) {
        out.push_back(firstLinePrefix);
        return out;
    }

    std::string currentPrefix = firstLinePrefix;
    std::string currentText;

    const int length = static_cast<int>(text.size());

    for (int i = 0; i < length; ++i) {
        const char ch = text[static_cast<size_t>(i)];
        const std::string candidateText = currentText + ch;
        const std::string candidateDrawText = currentPrefix + candidateText;
        const float candidateWidth = MeasureConsoleTextWidth(font, candidateDrawText, fontSize, spacing);

        if (!currentText.empty() && candidateWidth > clampedWidth) {
            out.push_back(currentPrefix + currentText);
            currentPrefix = continuationPrefix;
            currentText = std::string(1, ch);
        } else {
            currentText.push_back(ch);
        }
    }

    out.push_back(currentPrefix + currentText);
    return out;
}

static int FindConsoleCaretLineIndex(const ConsoleInputLayout& layout, int caretIndex)
{
    if (layout.lines.empty()) {
        return 0;
    }

    for (int i = 0; i < static_cast<int>(layout.lines.size()); ++i) {
        const ConsoleWrappedInputLine& line = layout.lines[i];

        if (caretIndex >= line.startCharIndex && caretIndex < line.endCharIndex) {
            return i;
        }

        if (caretIndex == line.endCharIndex) {
            return i;
        }
    }

    return static_cast<int>(layout.lines.size()) - 1;
}

static float MeasureConsoleCaretX(const Font& font,
                                  float fontSize,
                                  float spacing,
                                  const ConsoleWrappedInputLine& line,
                                  int caretIndex)
{
    const int clampedCaretIndex = std::clamp(caretIndex, line.startCharIndex, line.endCharIndex);
    const int charsOnThisLine = clampedCaretIndex - line.startCharIndex;

    std::string leftText = line.prefix;
    if (charsOnThisLine > 0) {
        leftText += line.text.substr(0, static_cast<size_t>(charsOnThisLine));
    }

    return MeasureConsoleTextWidth(font, leftText, fontSize, spacing);
}

static ConsoleInputLayout BuildConsoleInputLayout(const Font& font,
                                                  float fontSize,
                                                  float spacing,
                                                  float availableWidth,
                                                  const DebugConsoleData& console)
{
    ConsoleInputLayout layout;
    layout.lines.clear();

    const float clampedWidth = std::max(1.0f, availableWidth);

    ConsoleWrappedInputLine currentLine;
    currentLine.startCharIndex = 0;
    currentLine.endCharIndex = 0;
    currentLine.prefix = "> ";
    currentLine.text.clear();

    const int inputLength = static_cast<int>(console.input.size());

    for (int i = 0; i < inputLength; ++i) {
        const char ch = console.input[static_cast<size_t>(i)];
        const std::string candidateText = currentLine.text + ch;
        const std::string candidateDrawText = currentLine.prefix + candidateText;
        const float candidateWidth = MeasureConsoleTextWidth(font, candidateDrawText, fontSize, spacing);

        if (!currentLine.text.empty() && candidateWidth > clampedWidth) {
            currentLine.endCharIndex = i;
            layout.lines.push_back(currentLine);

            ConsoleWrappedInputLine nextLine;
            nextLine.startCharIndex = i;
            nextLine.endCharIndex = i;
            nextLine.prefix.clear();
            nextLine.text = std::string(1, ch);
            currentLine = nextLine;
        } else {
            currentLine.text.push_back(ch);
        }
    }

    currentLine.endCharIndex = inputLength;
    layout.lines.push_back(currentLine);

    if (layout.lines.empty()) {
        layout.lines.push_back(ConsoleWrappedInputLine{});
        layout.lines[0].prefix = "> ";
    }

    layout.visibleLineCount = std::min(
            static_cast<int>(layout.lines.size()),
            CONSOLE_MAX_VISIBLE_INPUT_LINES);

    const int caretIndex = std::clamp(
            console.caretIndex,
            0,
            static_cast<int>(console.input.size()));

    const int caretLineIndex = FindConsoleCaretLineIndex(layout, caretIndex);
    const int maxVisibleStart = std::max(
            0,
            static_cast<int>(layout.lines.size()) - layout.visibleLineCount);

    layout.visibleStartLine = std::clamp(
            caretLineIndex - (layout.visibleLineCount - 1),
            0,
            maxVisibleStart);

    return layout;
}

void RenderDebugConsoleInternal(const GameState& state)
{
    const DebugConsoleData& console = state.debug.console;
    if (!console.open) {
        return;
    }

    const Font font = DebugConsoleGetActiveFont();

    const Rectangle panelRect{
            static_cast<float>(CONSOLE_PADDING),
            static_cast<float>(CONSOLE_TOP),
            static_cast<float>(INTERNAL_WIDTH - CONSOLE_PADDING * 2),
            static_cast<float>(CONSOLE_HEIGHT)
    };

    const float fontSize = 22.0f;
    const float spacing = 1.0f;

    const float inputAvailableWidth =
            panelRect.width - static_cast<float>(CONSOLE_INPUT_INNER_PADDING_X * 2);

    const ConsoleInputLayout inputLayout = BuildConsoleInputLayout(
            font,
            fontSize,
            spacing,
            inputAvailableWidth,
            console);

    const int visibleInputLineCount = std::max(1, inputLayout.visibleLineCount);

    const float inputRectHeight =
            static_cast<float>(CONSOLE_INPUT_INNER_PADDING_Y * 2) +
            static_cast<float>(visibleInputLineCount * CONSOLE_LINE_HEIGHT);

    const Rectangle inputRect{
            panelRect.x,
            panelRect.y + panelRect.height - inputRectHeight,
            panelRect.width,
            inputRectHeight
    };

    const Rectangle linesRect{
            panelRect.x,
            panelRect.y,
            panelRect.width,
            inputRect.y - panelRect.y - 6.0f
    };

    DrawRectangleRounded(panelRect, 0.02f, 4, Color{20, 20, 20, 220});
    DrawRectangleLinesEx(panelRect, 2.0f, Color{180, 180, 180, 255});

    DrawRectangleRec(inputRect, Color{32, 32, 32, 230});
    DrawRectangleLinesEx(inputRect, 1.0f, Color{140, 140, 140, 255});

    const float topInset = 8.0f;
    const float bottomInset = 8.0f;
    const float availableLineHeight = linesRect.height - topInset - bottomInset;
    const int visibleLineCount = std::max(
            1,
            static_cast<int>(availableLineHeight / static_cast<float>(CONSOLE_LINE_HEIGHT)));

    const int totalLines = static_cast<int>(console.lines.size());
    const int maxScroll = std::max(0, totalLines - visibleLineCount);
    const int scrollOffset = std::clamp(console.scrollOffset, 0, maxScroll);

    const int startIndex = std::max(0, totalLines - visibleLineCount - scrollOffset);
    const int endIndex = std::min(totalLines, startIndex + visibleLineCount);

    float y = linesRect.y + topInset;
    for (int i = startIndex; i < endIndex; ++i) {
        DrawTextEx(
                font,
                console.lines[i].text.c_str(),
                Vector2{linesRect.x + 10.0f, y},
                fontSize,
                spacing,
                console.lines[i].color);
        y += static_cast<float>(CONSOLE_LINE_HEIGHT);
    }

    const Vector2 inputPos{
            inputRect.x + static_cast<float>(CONSOLE_INPUT_INNER_PADDING_X),
            inputRect.y + static_cast<float>(CONSOLE_INPUT_INNER_PADDING_Y)
    };

    const int caretIndex = std::clamp(
            console.caretIndex,
            0,
            static_cast<int>(console.input.size()));

    for (int visualLine = 0; visualLine < inputLayout.visibleLineCount; ++visualLine) {
        const int layoutLineIndex = inputLayout.visibleStartLine + visualLine;
        const ConsoleWrappedInputLine& line = inputLayout.lines[layoutLineIndex];

        const std::string drawText = line.prefix + line.text;
        const Vector2 linePos{
                inputPos.x,
                inputPos.y + static_cast<float>(visualLine * CONSOLE_LINE_HEIGHT)
        };

        DrawTextEx(
                font,
                drawText.c_str(),
                linePos,
                fontSize,
                spacing,
                WHITE);
    }

    const int caretLineIndex = FindConsoleCaretLineIndex(inputLayout, caretIndex);
    if (console.caretVisible &&
        caretLineIndex >= inputLayout.visibleStartLine &&
        caretLineIndex < inputLayout.visibleStartLine + inputLayout.visibleLineCount) {

        const ConsoleWrappedInputLine& caretLine = inputLayout.lines[caretLineIndex];
        const int visibleCaretLine = caretLineIndex - inputLayout.visibleStartLine;

        const float caretX =
                inputPos.x +
                MeasureConsoleCaretX(font, fontSize, spacing, caretLine, caretIndex);

        const float caretTop =
                inputPos.y +
                static_cast<float>(visibleCaretLine * CONSOLE_LINE_HEIGHT) +
                2.0f;

        const float caretBottom = caretTop + fontSize;

        DrawLineEx(
                Vector2{caretX, caretTop},
                Vector2{caretX, caretBottom},
                2.0f,
                WHITE);
    }

    const std::string helpText =
            "` console  |  Enter submit  |  Up/Down history  |  Left/Right move  |  Home/End caret  |  Ctrl+V paste  |  Ctrl+L clear  |  Del delete  |  PgUp/PgDn scroll";

    DrawTextEx(
            font,
            helpText.c_str(),
            Vector2{panelRect.x + 10.0f, panelRect.y - 28.0f},
            18.0f,
            1.0f,
            LIGHTGRAY);
}
