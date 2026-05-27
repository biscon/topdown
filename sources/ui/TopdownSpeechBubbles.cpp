#include "ui/TopdownSpeechBubbles.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <vector>
#include <utility>

#include "data/GameState.h"
#include "raymath.h"
#include "topdown/TopdownHelpers.h"

namespace {

    // speak("Hej Per, har du savnet mig?", WHITE, 16000)
constexpr float SPEECH_FADE_DURATION_MS = 250.0f;
constexpr float SPEECH_FONT_SIZE = 28.0f;
constexpr float SPEECH_FONT_SPACING = 1.0f;
constexpr float SPEECH_TEXT_PADDING = 16.0f;
constexpr float SPEECH_MAX_BUBBLE_WIDTH = 480.0f;
constexpr float SPEECH_LINE_HEIGHT_MULTIPLIER = 1.10f;
constexpr float SPEECH_ANCHOR_VERTICAL_OFFSET = 64.0f;
constexpr float SPEECH_ANCHOR_BUBBLE_SPACING = 12.0f;
constexpr float SPEECH_BORDER_INSET = 3.0f;
constexpr float SPEECH_BUBBLE_ROUNDNESS = 0.25f;

const Color SPEECH_BUBBLE_OUTER_COLOR = {150, 110, 70, 255};
const Color SPEECH_BUBBLE_INNER_COLOR = {34, 26, 20, 255};
//const Color SPEECH_BUBBLE_INNER_COLOR = {52, 38, 28, 255};

bool CanShowSpeechBubble(const GameState& state, const std::string& text, float durationMs)
{
    if (text.empty()) {
        return false;
    }

    if (durationMs <= 0.0f) {
        return false;
    }

    if (!state.topdown.runtime.levelActive || state.mode != GameMode::TopDown) {
        return false;
    }

    return true;
}

bool HasActiveNpcAnchor(const GameState& state, const std::string& npcId)
{
    for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
        if (!npc.active || npc.id != npcId) {
            continue;
        }

        return true;
    }

    return false;
}

bool HasActivePropAnchor(const GameState& state, const std::string& propId)
{
    for (const TopdownRuntimeProp& prop : state.topdown.runtime.props) {
        if (!prop.active || prop.id != propId) {
            continue;
        }

        return true;
    }

    return false;
}

void UpsertSpeechBubble(
        TopdownSpeechBubbleRuntime& runtime,
        TopdownSpeechBubbleAnchorType anchorType,
        std::string anchorId,
        const std::string& text,
        float durationMs,
        Color color)
{
    for (TopdownSpeechBubbleEntry& entry : runtime.entries) {
        if (entry.anchorType != anchorType || entry.anchorId != anchorId) {
            continue;
        }

        entry.active = true;
        entry.text = text;
        entry.color = color;
        entry.elapsedMs = 0.0f;
        entry.durationMs = durationMs;
        return;
    }

    TopdownSpeechBubbleEntry entry{};
    entry.active = true;
    entry.anchorType = anchorType;
    entry.anchorId = std::move(anchorId);
    entry.text = text;
    entry.color = color;
    entry.durationMs = durationMs;

    runtime.entries.push_back(std::move(entry));
}

Color ScaleColorAlpha(const Color& color, float alpha01)
{
    Color out = color;
    out.a = static_cast<unsigned char>(
            std::round(static_cast<float>(color.a) * Clamp(alpha01, 0.0f, 1.0f)));
    return out;
}

std::vector<std::string> WrapLineToWidth(const Font& font, float maxWidth, const std::string& line)
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

        const std::string word = line.substr(start, i - start);
        const std::string candidate = current.empty() ? word : (current + " " + word);
        const float width = MeasureTextEx(font, candidate.c_str(), SPEECH_FONT_SIZE, SPEECH_FONT_SPACING).x;
        if (width > maxWidth) {
            if (!current.empty()) {
                wrapped.push_back(current);
                current = word;
            } else {
                wrapped.push_back(word);
                current.clear();
            }
        } else {
            current = candidate;
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

std::vector<std::string> WrapTextToWidth(const Font& font, float maxWidth, const std::string& text)
{
    std::vector<std::string> lines;

    size_t start = 0;
    while (start <= text.size()) {
        const size_t newlinePos = text.find('\n', start);
        const bool hasNewline = newlinePos != std::string::npos;
        const size_t end = hasNewline ? newlinePos : text.size();

        const std::string rawLine = text.substr(start, end - start);
        std::vector<std::string> wrapped = WrapLineToWidth(font, maxWidth, rawLine);
        lines.insert(lines.end(), wrapped.begin(), wrapped.end());

        if (!hasNewline) {
            break;
        }

        start = newlinePos + 1;
    }

    return lines;
}

bool ResolveSpeechBubbleAnchorScreenPosition(
        const GameState& state,
        const TopdownSpeechBubbleEntry& entry,
        Vector2* outScreenPos)
{
    if (outScreenPos == nullptr) {
        return false;
    }

    Vector2 worldPos{};
    switch (entry.anchorType) {
    case TopdownSpeechBubbleAnchorType::Player:
        worldPos = state.topdown.runtime.player.position;
        break;
    case TopdownSpeechBubbleAnchorType::Npc:
        for (const TopdownNpcRuntime& npc : state.topdown.runtime.npcs) {
            if (npc.id != entry.anchorId) {
                continue;
            }

            if (!npc.active || !npc.visible || npc.dead || npc.corpse) {
                return false;
            }

            worldPos = npc.position;
            *outScreenPos = TopdownWorldToScreen(state, worldPos);
            return true;
        }
        return false;
    case TopdownSpeechBubbleAnchorType::Prop:
        for (const TopdownRuntimeProp& prop : state.topdown.runtime.props) {
            if (prop.id != entry.anchorId) {
                continue;
            }

            if (!prop.active || !prop.visible) {
                return false;
            }

            worldPos = prop.position;
            *outScreenPos = TopdownWorldToScreen(state, worldPos);
            return true;
        }
        return false;
    }

    *outScreenPos = TopdownWorldToScreen(state, worldPos);
    return true;
}

float ComputeSpeechBubbleAlpha(const TopdownSpeechBubbleEntry& entry)
{
    if (entry.durationMs <= 0.0f) {
        return 0.0f;
    }

    const float fadeWindowMs = std::min(SPEECH_FADE_DURATION_MS, entry.durationMs * 0.5f);
    if (fadeWindowMs <= 0.0f) {
        return 1.0f;
    }

    const float elapsed = Clamp(entry.elapsedMs, 0.0f, entry.durationMs);
    const float remaining = std::max(0.0f, entry.durationMs - elapsed);

    float alpha = 1.0f;
    if (elapsed < fadeWindowMs) {
        alpha = std::min(alpha, TopdownSmoothStep01(elapsed / fadeWindowMs));
    }
    if (remaining < fadeWindowMs) {
        alpha = std::min(alpha, TopdownSmoothStep01(remaining / fadeWindowMs));
    }

    return Clamp(alpha, 0.0f, 1.0f);
}

} // namespace

bool TopdownShowPlayerSpeechBubble(
        GameState& state,
        const std::string& text,
        float durationMs,
        Color color)
{
    if (!CanShowSpeechBubble(state, text, durationMs)) {
        return false;
    }

    UpsertSpeechBubble(
            state.topdown.runtime.speechBubbles,
            TopdownSpeechBubbleAnchorType::Player,
            "",
            text,
            durationMs,
            color);

    return true;
}

bool TopdownShowNpcSpeechBubble(
        GameState& state,
        const std::string& npcId,
        const std::string& text,
        float durationMs,
        Color color)
{
    if (!CanShowSpeechBubble(state, text, durationMs)) {
        return false;
    }

    if (npcId.empty() || !HasActiveNpcAnchor(state, npcId)) {
        return false;
    }

    UpsertSpeechBubble(
            state.topdown.runtime.speechBubbles,
            TopdownSpeechBubbleAnchorType::Npc,
            npcId,
            text,
            durationMs,
            color);

    return true;
}

bool TopdownShowPropSpeechBubble(
        GameState& state,
        const std::string& propId,
        const std::string& text,
        float durationMs,
        Color color)
{
    if (!CanShowSpeechBubble(state, text, durationMs)) {
        return false;
    }

    if (propId.empty() || !HasActivePropAnchor(state, propId)) {
        return false;
    }

    UpsertSpeechBubble(
            state.topdown.runtime.speechBubbles,
            TopdownSpeechBubbleAnchorType::Prop,
            propId,
            text,
            durationMs,
            color);

    return true;
}

void TopdownUpdateSpeechBubbles(GameState& state, float dt)
{
    TopdownSpeechBubbleRuntime& runtime = state.topdown.runtime.speechBubbles;

    for (TopdownSpeechBubbleEntry& entry : runtime.entries) {
        if (!entry.active) {
            continue;
        }

        entry.elapsedMs += dt * 1000.0f;
        if (entry.elapsedMs >= entry.durationMs) {
            entry.active = false;
        }
    }

    runtime.entries.erase(
            std::remove_if(
                    runtime.entries.begin(),
                    runtime.entries.end(),
                    [](const TopdownSpeechBubbleEntry& entry)
                    {
                        return !entry.active || entry.durationMs <= 0.0f || entry.elapsedMs >= entry.durationMs;
                    }),
            runtime.entries.end());
}

void TopdownRenderSpeechBubbles(GameState& state)
{
    for (const TopdownSpeechBubbleEntry& entry : state.topdown.runtime.speechBubbles.entries) {
        if (!entry.active || entry.text.empty() || entry.durationMs <= 0.0f) {
            continue;
        }

        Vector2 anchorScreen{};
        if (!ResolveSpeechBubbleAnchorScreenPosition(state, entry, &anchorScreen)) {
            continue;
        }

        const float textMaxWidth = SPEECH_MAX_BUBBLE_WIDTH - (SPEECH_TEXT_PADDING * 2.0f);
        const std::vector<std::string> lines = WrapTextToWidth(state.speechFont, textMaxWidth, entry.text);

        float textWidth = 0.0f;
        for (const std::string& line : lines) {
            textWidth = std::max(
                    textWidth,
                    MeasureTextEx(state.speechFont, line.c_str(), SPEECH_FONT_SIZE, SPEECH_FONT_SPACING).x);
        }

        const float lineHeight = SPEECH_FONT_SIZE * SPEECH_LINE_HEIGHT_MULTIPLIER;
        const float textHeight = lineHeight * static_cast<float>(lines.size());

        const float bubbleWidth = std::min(SPEECH_MAX_BUBBLE_WIDTH, textWidth + SPEECH_TEXT_PADDING * 2.0f);
        const float bubbleHeight = textHeight + SPEECH_TEXT_PADDING * 2.0f;

        float bubbleX = anchorScreen.x - bubbleWidth * 0.5f;
        float bubbleYAbove =
                anchorScreen.y - SPEECH_ANCHOR_VERTICAL_OFFSET - SPEECH_ANCHOR_BUBBLE_SPACING - bubbleHeight;
        float bubbleYBelow = anchorScreen.y - SPEECH_ANCHOR_VERTICAL_OFFSET + SPEECH_ANCHOR_BUBBLE_SPACING;
        float bubbleY = (bubbleYAbove >= 0.0f) ? bubbleYAbove : bubbleYBelow;

        bubbleX = Clamp(bubbleX, 0.0f, static_cast<float>(INTERNAL_WIDTH) - bubbleWidth);
        bubbleY = Clamp(bubbleY, 0.0f, static_cast<float>(INTERNAL_HEIGHT) - bubbleHeight);

        bubbleX = roundf(bubbleX);
        bubbleY = roundf(bubbleY);

        Rectangle outerRect{bubbleX, bubbleY, bubbleWidth, bubbleHeight};
        Rectangle innerRect{
                bubbleX + SPEECH_BORDER_INSET,
                bubbleY + SPEECH_BORDER_INSET,
                bubbleWidth - SPEECH_BORDER_INSET * 2.0f,
                bubbleHeight - SPEECH_BORDER_INSET * 2.0f};

        const float alpha = ComputeSpeechBubbleAlpha(entry);
        if (alpha <= 0.0f) {
            continue;
        }

        const Color outerColor = ScaleColorAlpha(SPEECH_BUBBLE_OUTER_COLOR, alpha);
        const Color innerColor = ScaleColorAlpha(SPEECH_BUBBLE_INNER_COLOR, alpha);
        const Color textColor = ScaleColorAlpha(entry.color, alpha);

        DrawRectangleRounded(outerRect, SPEECH_BUBBLE_ROUNDNESS, 8, outerColor);
        DrawRectangleRounded(innerRect, SPEECH_BUBBLE_ROUNDNESS, 8, innerColor);

        Vector2 textPos{
                roundf(innerRect.x + SPEECH_TEXT_PADDING - SPEECH_BORDER_INSET),
                roundf(innerRect.y + SPEECH_TEXT_PADDING - SPEECH_BORDER_INSET)
        };

        for (const std::string& line : lines) {
            DrawTextEx(
                    state.speechFont,
                    line.c_str(),
                    textPos,
                    SPEECH_FONT_SIZE,
                    SPEECH_FONT_SPACING,
                    textColor);
            textPos.y += lineHeight;
        }
    }
}
