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
    constexpr float POPUP_PANEL_BUILD_DURATION_SECONDS = 0.42f;
    constexpr float POPUP_BORDER_REVEAL_DURATION_SECONDS = 0.30f;
    constexpr float POPUP_TEXT_REVEAL_DURATION_SECONDS = 0.70f;
    constexpr float POPUP_EXIT_DURATION_SECONDS = 0.52f;
    constexpr int POPUP_VISIBLE_CAP = 4;

    constexpr float STACK_MARGIN_LEFT = 32.0f;
    constexpr float STACK_MARGIN_BOTTOM = 32.0f;
    constexpr float POPUP_STACK_SPACING = 16.0f;

    constexpr float POPUP_WIDTH = 640.0f;
    constexpr float POPUP_PADDING_X = 24.0f;
    constexpr float POPUP_PADDING_Y = 18.0f;
    constexpr float TITLE_BODY_SPACING = 14.0f;
    constexpr float TITLE_STRIP_HEIGHT = 60.0f;

    constexpr float TITLE_FONT_SIZE = 42.0f;
    constexpr float BODY_FONT_SIZE = 32.0f;
    constexpr float FONT_SPACING = 1.0f;
    constexpr float BODY_LINE_HEIGHT_MULTIPLIER = 1.1f;

    constexpr float ENTER_SLIDE_PIXELS = 42.0f;
    constexpr float EXIT_SLIDE_PIXELS = 30.0f;

    constexpr float PANEL_CORNER_ROUNDNESS = 0.13f;
    constexpr int PANEL_CORNER_SEGMENTS = 8;
    constexpr float PANEL_BORDER_THICKNESS = 1.8f;
    constexpr float SHADOW_OFFSET_X = 7.0f;
    constexpr float SHADOW_OFFSET_Y = 8.0f;
    constexpr float ACCENT_BAR_WIDTH = 1.0f;

    constexpr float BASE_HOLD_SECONDS = 2.2f;
    constexpr float EXTRA_PER_CHAR_SECONDS = 0.027f;
    constexpr float HOLD_MIN_SECONDS = 2.8f;
    constexpr float HOLD_MAX_SECONDS = 8.0f;

    const Color PANEL_FILL_COLOR = {34, 26, 20, 255};
    const Color PANEL_TITLE_STRIP_COLOR = {52, 38, 28, 255};
    const Color PANEL_BORDER_COLOR = {150, 110, 70, 255};
    const Color PANEL_INNER_LINE_COLOR = {90, 64, 44, 255};
    const Color PANEL_ACCENT_COLOR = {196, 140, 70, 255};
    const Color PANEL_SHADOW_COLOR = {12, 9, 7, 255};
    const Color TITLE_TEXT_COLOR = {250, 232, 200, 255};
    const Color BODY_TEXT_COLOR = {222, 200, 168, 255};

    static float SmoothStep01(float t)
    {
        t = Clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static Color ScaleColorAlpha(const Color& color, float alpha01)
    {
        Color out = color;
        out.a = static_cast<unsigned char>(
                std::round(static_cast<float>(color.a) * Clamp(alpha01, 0.0f, 1.0f)));
        return out;
    }

    static float ComputeDefaultHoldDurationSeconds(const std::string& body)
    {
        const float textChars = static_cast<float>(body.size());
        return Clamp(
                BASE_HOLD_SECONDS + textChars * EXTRA_PER_CHAR_SECONDS,
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

    static std::vector<std::string> WrapBodyLines(
            const Font& font,
            float maxWidth,
            const std::string& body)
    {
        std::vector<std::string> lines;

        size_t start = 0;
        while (start <= body.size()) {
            const size_t newlinePos = body.find('\n', start);
            const bool hasNewline = newlinePos != std::string::npos;
            const size_t end = hasNewline ? newlinePos : body.size();

            const std::string rawLine = body.substr(start, end - start);
            std::vector<std::string> wrappedPart =
                    WrapLineToWidth(font, BODY_FONT_SIZE, maxWidth, rawLine);
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

        const std::vector<std::string> bodyLines =
                WrapBodyLines(state.narrationBodyFont, contentWidth, popup.body);

        const float bodyLineHeight = BODY_FONT_SIZE * BODY_LINE_HEIGHT_MULTIPLIER;
        const float bodyHeight = static_cast<float>(bodyLines.size()) * bodyLineHeight;

        const float stripTextHeight =
                std::max(titleHeight, TITLE_STRIP_HEIGHT - POPUP_PADDING_Y * 2.0f);

        return POPUP_PADDING_Y * 2.0f + stripTextHeight + TITLE_BODY_SPACING + bodyHeight;
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

    static void DrawWrappedBodyText(
            const GameState& state,
            const TopdownNarrationPopupEntry& popup,
            Vector2 origin,
            Color color)
    {
        const float contentWidth = POPUP_WIDTH - POPUP_PADDING_X * 2.0f;
        const std::vector<std::string> bodyLines =
                WrapBodyLines(state.narrationBodyFont, contentWidth, popup.body);

        const float lineHeight = BODY_FONT_SIZE * BODY_LINE_HEIGHT_MULTIPLIER;
        Vector2 linePos = origin;

        for (const std::string& line : bodyLines) {
            DrawTextEx(
                    state.narrationBodyFont,
                    line.c_str(),
                    linePos,
                    BODY_FONT_SIZE,
                    FONT_SPACING,
                    color);
            linePos.y += lineHeight;
        }
    }

} // namespace

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
    popup.phase = TopdownNarrationPopupPhase::PanelBuild;
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
            case TopdownNarrationPopupPhase::PanelBuild:
                if (popup.phaseElapsed >= POPUP_PANEL_BUILD_DURATION_SECONDS) {
                    popup.phase = TopdownNarrationPopupPhase::BorderReveal;
                    popup.phaseElapsed = 0.0f;
                }
                break;

            case TopdownNarrationPopupPhase::BorderReveal:
                if (popup.phaseElapsed >= POPUP_BORDER_REVEAL_DURATION_SECONDS) {
                    popup.phase = TopdownNarrationPopupPhase::TextReveal;
                    popup.phaseElapsed = 0.0f;
                }
                break;

            case TopdownNarrationPopupPhase::TextReveal:
                if (popup.phaseElapsed >= POPUP_TEXT_REVEAL_DURATION_SECONDS) {
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
            std::remove_if(
                    runtime.entries.begin(),
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

        float panelT = 1.0f;
        float borderT = 1.0f;
        float titleTextT = 1.0f;
        float bodyTextT = 1.0f;
        float exitT = 0.0f;

        switch (popup.phase) {
            case TopdownNarrationPopupPhase::PanelBuild:
                panelT = SmoothStep01(
                        popup.phaseElapsed / POPUP_PANEL_BUILD_DURATION_SECONDS);
                borderT = 0.0f;
                titleTextT = 0.0f;
                bodyTextT = 0.0f;
                break;

            case TopdownNarrationPopupPhase::BorderReveal:
                panelT = 1.0f;
                borderT = SmoothStep01(
                        popup.phaseElapsed / POPUP_BORDER_REVEAL_DURATION_SECONDS);
                titleTextT = 0.0f;
                bodyTextT = 0.0f;
                break;

            case TopdownNarrationPopupPhase::TextReveal: {
                panelT = 1.0f;
                borderT = 1.0f;

                const float t = Clamp(
                        popup.phaseElapsed / POPUP_TEXT_REVEAL_DURATION_SECONDS,
                        0.0f,
                        1.0f);
                titleTextT = SmoothStep01(std::min(t / 0.55f, 1.0f));
                bodyTextT = SmoothStep01(Clamp((t - 0.25f) / 0.75f, 0.0f, 1.0f));
                break;
            }

            case TopdownNarrationPopupPhase::Hold:
                panelT = 1.0f;
                borderT = 1.0f;
                titleTextT = 1.0f;
                bodyTextT = 1.0f;
                break;

            case TopdownNarrationPopupPhase::Exit:
                panelT = 1.0f;
                borderT = 1.0f;
                titleTextT = 1.0f;
                bodyTextT = 1.0f;
                exitT = SmoothStep01(
                        popup.phaseElapsed / POPUP_EXIT_DURATION_SECONDS);
                break;
        }

        const float baseAlpha = 1.0f - exitT;
        const float panelAlpha = panelT * baseAlpha;
        const float borderAlpha = borderT * baseAlpha;
        const float titleAlpha = titleTextT * baseAlpha;
        const float bodyAlpha = bodyTextT * baseAlpha;

        float enterScale = 1.0f;
        if (popup.phase == TopdownNarrationPopupPhase::PanelBuild) {
            const float t = Clamp(
                    popup.phaseElapsed / POPUP_PANEL_BUILD_DURATION_SECONDS,
                    0.0f,
                    1.0f);
            enterScale = Lerp(
                    0.88f,
                    1.03f,
                    SmoothStep01(std::min(t / 0.8f, 1.0f)));
        } else if (popup.phase == TopdownNarrationPopupPhase::BorderReveal) {
            const float t = Clamp(
                    popup.phaseElapsed / POPUP_BORDER_REVEAL_DURATION_SECONDS,
                    0.0f,
                    1.0f);
            enterScale = Lerp(1.03f, 1.0f, SmoothStep01(t));
        }

        const float exitScale = Lerp(1.0f, 0.97f, exitT);
        const float finalScale = enterScale * exitScale;

        const float slideX =
                Lerp(-ENTER_SLIDE_PIXELS, 0.0f, panelT) - exitT * EXIT_SLIDE_PIXELS;
        const float liftY =
                Lerp(8.0f, 0.0f, panelT) - exitT * 10.0f;

        if (panelAlpha <= 0.0f) {
            yCursor -= boxHeight + POPUP_STACK_SPACING;
            continue;
        }

        Rectangle box{
                STACK_MARGIN_LEFT + slideX,
                yCursor - boxHeight + liftY,
                POPUP_WIDTH,
                boxHeight
        };

        const float scaledWidth = box.width * finalScale;
        const float scaledHeight = box.height * finalScale;

        box.x += (box.width - scaledWidth) * 0.5f;
        box.y += (box.height - scaledHeight);
        box.width = scaledWidth;
        box.height = scaledHeight;

        const float scaledPaddingX = POPUP_PADDING_X * finalScale;
        const float scaledPaddingY = POPUP_PADDING_Y * finalScale;
        const float scaledTitleStripHeight = TITLE_STRIP_HEIGHT * finalScale;
        const float scaledTitleBodySpacing = TITLE_BODY_SPACING * finalScale;
        const float scaledAccentBarWidth = ACCENT_BAR_WIDTH * finalScale;
        const float scaledBorderThickness =
                std::max(1.0f, PANEL_BORDER_THICKNESS * finalScale);

        const Rectangle shadowBox{
                box.x + SHADOW_OFFSET_X * finalScale,
                box.y + SHADOW_OFFSET_Y * finalScale,
                box.width,
                box.height
        };

        DrawRectangleRounded(
                shadowBox,
                PANEL_CORNER_ROUNDNESS,
                PANEL_CORNER_SEGMENTS,
                ScaleColorAlpha(PANEL_SHADOW_COLOR, panelAlpha * 0.8f));

        DrawRectangleRounded(
                box,
                PANEL_CORNER_ROUNDNESS,
                PANEL_CORNER_SEGMENTS,
                ScaleColorAlpha(PANEL_FILL_COLOR, panelAlpha));

        const float borderRevealWidth = (box.width - 2.0f) * borderT;
        const Rectangle titleClip{
                box.x + 1.0f,
                box.y + 1.0f,
                borderRevealWidth,
                scaledTitleStripHeight
        };

        const Rectangle titleStripRounded{
                box.x + 1.0f,
                box.y + 1.0f,
                borderRevealWidth,
                scaledTitleStripHeight + 24.0f * finalScale
        };

        if (titleClip.width > 0.0f && titleClip.height > 0.0f) {
            BeginScissorMode(
                    static_cast<int>(titleClip.x),
                    static_cast<int>(titleClip.y),
                    static_cast<int>(titleClip.width),
                    static_cast<int>(titleClip.height));

            DrawRectangleRounded(
                    titleStripRounded,
                    PANEL_CORNER_ROUNDNESS,
                    PANEL_CORNER_SEGMENTS,
                    ScaleColorAlpha(PANEL_TITLE_STRIP_COLOR, borderAlpha));

            EndScissorMode();
        }

        const Rectangle accentBar{
                box.x + 10.0f * finalScale,
                box.y + scaledTitleStripHeight + 10.0f * finalScale,
                scaledAccentBarWidth,
                box.height - scaledTitleStripHeight - 20.0f * finalScale
        };
        DrawRectangleRec(
                accentBar,
                ScaleColorAlpha(PANEL_ACCENT_COLOR, borderAlpha));

        const Rectangle innerLine{
                box.x + scaledPaddingX,
                box.y + scaledTitleStripHeight,
                (box.width - scaledPaddingX * 2.0f) * borderT,
                std::max(1.0f, 2.0f * finalScale)
        };
        DrawRectangleRec(
                innerLine,
                ScaleColorAlpha(PANEL_INNER_LINE_COLOR, borderAlpha));

        DrawRectangleRoundedLinesEx(
                box,
                PANEL_CORNER_ROUNDNESS,
                PANEL_CORNER_SEGMENTS,
                scaledBorderThickness,
                ScaleColorAlpha(PANEL_BORDER_COLOR, borderAlpha));

        const Vector2 titlePos = {
                box.x + scaledPaddingX + 2.0f * finalScale,
                box.y + scaledPaddingY - 4.0f * finalScale + (1.0f - titleTextT) * 8.0f * finalScale
        };

        DrawTextEx(
                state.narrationTitleFont,
                popup.title.c_str(),
                titlePos,
                TITLE_FONT_SIZE * finalScale,
                FONT_SPACING * finalScale,
                ScaleColorAlpha(TITLE_TEXT_COLOR, titleAlpha));

        const Vector2 bodyPos = {
                box.x + scaledPaddingX,
                box.y + scaledTitleStripHeight + scaledTitleBodySpacing + (1.0f - bodyTextT) * 12.0f * finalScale
        };

        DrawWrappedBodyText(
                state,
                popup,
                bodyPos,
                ScaleColorAlpha(BODY_TEXT_COLOR, bodyAlpha));

        yCursor -= boxHeight + POPUP_STACK_SPACING;
    }
}
