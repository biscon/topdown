#include "ui/TopdownSpeechBubbles.h"

#include <algorithm>
#include <utility>

#include "data/GameState.h"

namespace {

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
    (void) state;
    // TODO Slice 2: implement anchored speech bubble rendering for player, NPCs, and props.
}
