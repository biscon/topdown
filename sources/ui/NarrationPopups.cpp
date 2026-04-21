#include "ui/NarrationPopups.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "data/GameState.h"
#include "raylib.h"
#include "raymath.h"
#include "ui/NarrationPopupsData.h"

namespace {
constexpr float POPUP_ENTER_DURATION_SECONDS = 0.22f;
constexpr float POPUP_EXIT_DURATION_SECONDS = 0.22f;
constexpr int POPUP_VISIBLE_CAP = 4;

constexpr float STACK_MARGIN_LEFT = 32.0f;
constexpr float STACK_MARGIN_BOTTOM = 32.0f;
constexpr float POPUP_STACK_SPACING = 12.0f;

constexpr float POPUP_WIDTH = 620.0f;
constexpr float POPUP_PADDING_X = 20.0f;
constexpr float POPUP_PADDING_Y = 14.0f;
constexpr float TITLE_BODY_SPACING = 8.0f;

constexpr float TITLE_FONT_SIZE = 40.0f;
constexpr float BODY_FONT_SIZE = 30.0f;
constexpr float FONT_SPACING = 1.0f;
constexpr float BODY_LINE_HEIGHT_MULTIPLIER = 1.2f;

constexpr float ENTER_SLIDE_PIXELS = 24.0f;
constexpr float EXIT_SLIDE_PIXELS = 20.0f;

constexpr float BASE_HOLD_SECONDS = 2.2f;
constexpr float EXTRA_PER_CHAR_SECONDS = 0.027f;
constexpr float HOLD_MIN_SECONDS = 2.8f;
constexpr float HOLD_MAX_SECONDS = 8.0f;

static float ComputeDefaultHoldDurationSeconds(const std::string& body)
{
    const float textChars = static_cast<float>(body.size());
    return Clamp(BASE_HOLD_SECONDS + textChars * EXTRA_PER_CHAR_SECONDS,
                 HOLD_MIN_SECONDS,
                 HOLD_MAX_SECONDS);
}

static std::vector<std::string> WrapLineToWidth(
        const Font& font,
        float fontSize,
        float maxWidth,
        const std::string& line)
{
    std::vector<std::string> wrapped;

    if (line.empty()) {
        wrapped.push_back("");
        return wrapped;
    }

    std::string current;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }

        if (i >= line.size()) {
            break;
        }

        const size_t start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
        }

        std::string word = line.substr(start, i - start);
        if (current.empty()) {
            current = word;
        } else {
            current += " ";
            current += word;
        }

        const float width = MeasureTextEx(font, current.c_str(), fontSize, FONT_SPACING).x;
        if (width > maxWidth) {
            if (current == word) {
                wrapped.push_back(word);
                current.clear();
            } else {
                const size_t split = current.size() - word.size() - 1;
                wrapped.push_back(current.substr(0, split));
                current = word;
            }
        }
    }

    if (!current.empty()) {
        wrapped.push_back(current);
    }

    if (wrapped.empty()) {
        wrapped.push_back("");
    }

    return wrapped;
}

static std::vector<std::string> WrapBodyLines(const Font& font, float maxWidth, const std::string& body)
{
    std::vector<std::string> lines;

    size_t start = 0;
    while (start <= body.size()) {
        const size_t newlinePos = body.find('\n', start);
        const bool hasNewline = newlinePos != std::string::npos;
        const size_t end = hasNewline ? newlinePos : body.size();

        const std::string rawLine = body.substr(start, end - start);
        std::vector<std::string> wrappedPart = WrapLineToWidth(font, BODY_FONT_SIZE, maxWidth, rawLine);
        lines.insert(lines.end(), wrappedPart.begin(), wrappedPart.end());

        if (!hasNewline) {
            break;
        }

        start = newlinePos + 1;
    }

    return lines;
}

static float MeasurePopupHeight(const GameState& state, const TopdownNarrationPopupEntry& popup)
{
    const float contentWidth = POPUP_WIDTH - POPUP_PADDING_X * 2.0f;
    const float titleHeight = MeasureTextEx(
            state.narrationTitleFont,
            popup.title.c_str(),
            TITLE_FONT_SIZE,
            FONT_SPACING).y;

    const std::vector<std::string> bodyLines = WrapBodyLines(state.narrationBodyFont, contentWidth, popup.body);
    const float bodyLineHeight = BODY_FONT_SIZE * BODY_LINE_HEIGHT_MULTIPLIER;
    const float bodyHeight = static_cast<float>(bodyLines.size()) * bodyLineHeight;

    return POPUP_PADDING_Y * 2.0f + titleHeight + TITLE_BODY_SPACING + bodyHeight;
}

static void ForcePopupToExit(TopdownNarrationPopupEntry& popup)
{
    if (!popup.active || popup.phase == TopdownNarrationPopupPhase::Exit) {
        return;
    }

    popup.phase = TopdownNarrationPopupPhase::Exit;
    popup.phaseElapsed = 0.0f;
}

static void EnforceVisiblePopupCap(TopdownNarrationPopupsRuntime& runtime)
{
    int nonExitingCount = 0;
    for (const TopdownNarrationPopupEntry& popup : runtime.entries) {
        if (popup.active && popup.phase != TopdownNarrationPopupPhase::Exit) {
            ++nonExitingCount;
        }
    }

    while (nonExitingCount > POPUP_VISIBLE_CAP) {
        bool forced = false;
        for (TopdownNarrationPopupEntry& popup : runtime.entries) {
            if (!popup.active || popup.phase == TopdownNarrationPopupPhase::Exit) {
                continue;
            }

            ForcePopupToExit(popup);
            --nonExitingCount;
            forced = true;
            break;
        }

        if (!forced) {
            break;
        }
    }
}

static void DrawWrappedBodyText(const GameState& state, const TopdownNarrationPopupEntry& popup, Vector2 origin, Color color)
{
    const float contentWidth = POPUP_WIDTH - POPUP_PADDING_X * 2.0f;
    const std::vector<std::string> bodyLines = WrapBodyLines(state.narrationBodyFont, contentWidth, popup.body);

    const float lineHeight = BODY_FONT_SIZE * BODY_LINE_HEIGHT_MULTIPLIER;
    Vector2 linePos = origin;

    for (const std::string& line : bodyLines) {
        DrawTextEx(state.narrationBodyFont, line.c_str(), linePos, BODY_FONT_SIZE, FONT_SPACING, color);
        linePos.y += lineHeight;
    }
}

}

bool TopdownQueueNarrationPopup(
        GameState& state,
        const std::string& title,
        const std::string& body,
        float durationSeconds)
{
    if (!state.topdown.runtime.levelActive || state.mode != GameMode::TopDown) {
        return false;
    }

    TopdownNarrationPopupEntry popup;
    popup.active = true;
    popup.title = title;
    popup.body = body;
    popup.phase = TopdownNarrationPopupPhase::Enter;
    popup.phaseElapsed = 0.0f;
    popup.holdDuration = (durationSeconds > 0.0f)
                         ? durationSeconds
                         : ComputeDefaultHoldDurationSeconds(body);
    popup.measuredHeight = MeasurePopupHeight(state, popup);

    TopdownNarrationPopupsRuntime& runtime = state.topdown.runtime.narrationPopups;
    runtime.entries.push_back(std::move(popup));

    EnforceVisiblePopupCap(runtime);
    return true;
}

void TopdownUpdateNarrationPopups(GameState& state, float dt)
{
    TopdownNarrationPopupsRuntime& runtime = state.topdown.runtime.narrationPopups;

    for (TopdownNarrationPopupEntry& popup : runtime.entries) {
        if (!popup.active) {
            continue;
        }

        popup.phaseElapsed += dt;

        switch (popup.phase) {
            case TopdownNarrationPopupPhase::Enter:
                if (popup.phaseElapsed >= POPUP_ENTER_DURATION_SECONDS) {
                    popup.phase = TopdownNarrationPopupPhase::Hold;
                    popup.phaseElapsed = 0.0f;
                }
                break;

            case TopdownNarrationPopupPhase::Hold:
                if (popup.phaseElapsed >= popup.holdDuration) {
                    popup.phase = TopdownNarrationPopupPhase::Exit;
                    popup.phaseElapsed = 0.0f;
                }
                break;

            case TopdownNarrationPopupPhase::Exit:
                if (popup.phaseElapsed >= POPUP_EXIT_DURATION_SECONDS) {
                    popup.active = false;
                }
                break;
        }
    }

    runtime.entries.erase(
            std::remove_if(runtime.entries.begin(),
                           runtime.entries.end(),
                           [](const TopdownNarrationPopupEntry& popup)
                           {
                               return !popup.active;
                           }),
            runtime.entries.end());
}

void TopdownRenderNarrationPopups(GameState& state)
{
    TopdownNarrationPopupsRuntime& runtime = state.topdown.runtime.narrationPopups;
    if (runtime.entries.empty()) {
        return;
    }

    float yCursor = INTERNAL_HEIGHT - STACK_MARGIN_BOTTOM;

    for (int i = static_cast<int>(runtime.entries.size()) - 1; i >= 0; --i) {
        TopdownNarrationPopupEntry& popup = runtime.entries[i];
        if (!popup.active) {
            continue;
        }

        const float boxHeight = (popup.measuredHeight > 0.0f)
                                ? popup.measuredHeight
                                : MeasurePopupHeight(state, popup);
        popup.measuredHeight = boxHeight;

        float alpha01 = 1.0f;
        float slideX = 0.0f;
        if (popup.phase == TopdownNarrationPopupPhase::Enter) {
            const float t = Clamp(popup.phaseElapsed / POPUP_ENTER_DURATION_SECONDS, 0.0f, 1.0f);
            alpha01 = t;
            slideX = (1.0f - t) * -ENTER_SLIDE_PIXELS;
        } else if (popup.phase == TopdownNarrationPopupPhase::Exit) {
            const float t = Clamp(popup.phaseElapsed / POPUP_EXIT_DURATION_SECONDS, 0.0f, 1.0f);
            alpha01 = 1.0f - t;
            slideX = -t * EXIT_SLIDE_PIXELS;
        }

        if (alpha01 <= 0.0f) {
            yCursor -= boxHeight + POPUP_STACK_SPACING;
            continue;
        }

        const Rectangle box{
                STACK_MARGIN_LEFT + slideX,
                yCursor - boxHeight,
                POPUP_WIDTH,
                boxHeight
        };

        Color fill = {18, 18, 18, static_cast<unsigned char>(std::round(200.0f * alpha01))};
        Color border = {180, 180, 180, static_cast<unsigned char>(std::round(145.0f * alpha01))};
        Color titleColor = {236, 236, 236, static_cast<unsigned char>(std::round(255.0f * alpha01))};
        Color bodyColor = {214, 214, 214, static_cast<unsigned char>(std::round(255.0f * alpha01))};

        DrawRectangleRec(box, fill);
        DrawRectangleLinesEx(box, 1.0f, border);

        const Vector2 titlePos = {
                box.x + POPUP_PADDING_X,
                box.y + POPUP_PADDING_Y
        };
        DrawTextEx(state.narrationTitleFont,
                   popup.title.c_str(),
                   titlePos,
                   TITLE_FONT_SIZE,
                   FONT_SPACING,
                   titleColor);

        const float titleHeight = MeasureTextEx(
                state.narrationTitleFont,
                popup.title.c_str(),
                TITLE_FONT_SIZE,
                FONT_SPACING).y;

        const Vector2 bodyPos = {
                box.x + POPUP_PADDING_X,
                titlePos.y + titleHeight + TITLE_BODY_SPACING
        };

        DrawWrappedBodyText(state, popup, bodyPos, bodyColor);

        yCursor -= boxHeight + POPUP_STACK_SPACING;
    }
}
