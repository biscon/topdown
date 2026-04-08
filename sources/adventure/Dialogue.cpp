#include "adventure/Dialogue.h"

#include <cmath>

#include "input/Input.h"
#include "raylib.h"
#include "audio/Audio.h"

static Rectangle GetDialogueOptionRect(int optionIndex, int optionCount)
{
    const float width = 1100.0f;
    const float lineHeight = 48.0f;
    const float totalHeight = lineHeight * static_cast<float>(optionCount);
    const float startX = (static_cast<float>(INTERNAL_WIDTH) - width) * 0.5f;
    const float startY = static_cast<float>(INTERNAL_HEIGHT) - 32.0f - totalHeight;

    return Rectangle{
            startX,
            startY + static_cast<float>(optionIndex) * lineHeight,
            width,
            lineHeight
    };
}

static void DrawShadowedTextLine(
        Font font,
        const std::string& text,
        Vector2 pos,
        float fontSize,
        float spacing,
        Color textColor,
        int shadowOffset)
{
    Color shadow = Color{0, 0, 0, 170};
    shadow.a = static_cast<unsigned char>(
            std::round(static_cast<float>(shadow.a) * (static_cast<float>(textColor.a) / 255.0f)));

    const Vector2 adjustPos = pos;
    pos.x = std::trunc(pos.x);
    pos.y = std::trunc(pos.y);

    DrawTextEx(font, text.c_str(),
               Vector2{adjustPos.x + static_cast<float>(shadowOffset), adjustPos.y + static_cast<float>(shadowOffset)},
               fontSize, spacing, shadow);

    DrawTextEx(font, text.c_str(), adjustPos, fontSize, spacing, textColor);
}

static void BuildVisibleOptionIndices(
        const DialogueChoiceSetData& choiceSet,
        const DialogueUiState& ui,
        std::vector<int>& outIndices)
{
    outIndices.clear();

    for (int i = 0; i < static_cast<int>(choiceSet.options.size()); ++i) {
        const DialogueChoiceOptionData& option = choiceSet.options[i];
        if (ui.hiddenOptionIds.find(option.id) != ui.hiddenOptionIds.end()) {
            continue;
        }

        outIndices.push_back(i);
    }
}

const DialogueChoiceSetData* FindDialogueChoiceSetById(
        const GameState& state,
        const std::string& choiceSetId)
{
    for (const DialogueChoiceSetData& choiceSet : state.adventure.dialogueChoiceSets) {
        if (choiceSet.id == choiceSetId) {
            return &choiceSet;
        }
    }

    return nullptr;
}

static int FindDialogueChoiceSetIndexById(
        const GameState& state,
        const std::string& choiceSetId)
{
    for (int i = 0; i < static_cast<int>(state.adventure.dialogueChoiceSets.size()); ++i) {
        if (state.adventure.dialogueChoiceSets[i].id == choiceSetId) {
            return i;
        }
    }

    return -1;
}

bool StartDialogueChoice(
        GameState& state,
        const std::string& choiceSetId,
        const std::vector<std::string>* hiddenOptionIds)
{
    if (state.adventure.dialogueUi.active) {
        TraceLog(LOG_WARNING, "Dialogue already active, cannot start: %s", choiceSetId.c_str());
        return false;
    }

    const int choiceSetIndex = FindDialogueChoiceSetIndexById(state, choiceSetId);
    if (choiceSetIndex < 0) {
        TraceLog(LOG_ERROR, "Dialogue choice set not found: %s", choiceSetId.c_str());
        return false;
    }

    const DialogueChoiceSetData& choiceSet = state.adventure.dialogueChoiceSets[choiceSetIndex];
    if (choiceSet.options.empty()) {
        TraceLog(LOG_ERROR, "Dialogue choice set has no options: %s", choiceSetId.c_str());
        return false;
    }

    state.adventure.dialogueUi = {};
    state.adventure.dialogueUi.active = true;
    state.adventure.dialogueUi.activeChoiceSetIndex = choiceSetIndex;
    state.adventure.dialogueUi.hoveredOptionIndex = -1;
    state.adventure.dialogueUi.resultReady = false;
    state.adventure.dialogueUi.selectedOptionId.clear();

    if (hiddenOptionIds != nullptr) {
        for (const std::string& optionId : *hiddenOptionIds) {
            state.adventure.dialogueUi.hiddenOptionIds.insert(optionId);
        }
    }

    std::vector<int> visibleOptionIndices;
    BuildVisibleOptionIndices(choiceSet, state.adventure.dialogueUi, visibleOptionIndices);
    if (visibleOptionIndices.empty()) {
        TraceLog(LOG_WARNING,
                 "Dialogue choice set '%s' has no visible options after filtering",
                 choiceSetId.c_str());
        state.adventure.dialogueUi = {};
        return false;
    }

    return true;
}

void CancelDialogueChoice(GameState& state)
{
    state.adventure.dialogueUi = {};
}

bool IsDialogueUiActive(const GameState& state)
{
    return state.adventure.dialogueUi.active;
}

void UpdateDialogueUi(GameState& state)
{
    DialogueUiState& ui = state.adventure.dialogueUi;
    ui.hoveredOptionIndex = -1;

    if (!ui.active) {
        return;
    }

    if (ui.activeChoiceSetIndex < 0 ||
        ui.activeChoiceSetIndex >= static_cast<int>(state.adventure.dialogueChoiceSets.size())) {
        ui = {};
        return;
    }

    const DialogueChoiceSetData& choiceSet =
            state.adventure.dialogueChoiceSets[ui.activeChoiceSetIndex];

    if (choiceSet.options.empty()) {
        ui = {};
        return;
    }

    std::vector<int> visibleOptionIndices;
    BuildVisibleOptionIndices(choiceSet, ui, visibleOptionIndices);

    if (visibleOptionIndices.empty()) {
        ui = {};
        return;
    }

    const Vector2 mouse = GetMousePosition();

    for (int visibleIndex = 0; visibleIndex < static_cast<int>(visibleOptionIndices.size()); ++visibleIndex) {
        const Rectangle optionRect =
                GetDialogueOptionRect(visibleIndex, static_cast<int>(visibleOptionIndices.size()));
        if (CheckCollisionPointRec(mouse, optionRect)) {
            ui.hoveredOptionIndex = visibleIndex;
            break;
        }
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
        if (ev.mouse.button != MOUSE_BUTTON_LEFT &&
            ev.mouse.button != MOUSE_BUTTON_RIGHT) {
            continue;
        }

        if (ui.hoveredOptionIndex >= 0 &&
            ui.hoveredOptionIndex < static_cast<int>(visibleOptionIndices.size())) {
            const int optionIndex = visibleOptionIndices[ui.hoveredOptionIndex];
            ui.selectedOptionId = choiceSet.options[optionIndex].id;
            ui.resultReady = true;
            PlaySoundById(state, "ui_click");
        }

        ConsumeEvent(ev);
        return;
    }
}

void RenderDialogueUi(const GameState& state)
{
    const DialogueUiState& ui = state.adventure.dialogueUi;
    if (!ui.active) {
        return;
    }

    if (ui.activeChoiceSetIndex < 0 ||
        ui.activeChoiceSetIndex >= static_cast<int>(state.adventure.dialogueChoiceSets.size())) {
        return;
    }

    const DialogueChoiceSetData& choiceSet =
            state.adventure.dialogueChoiceSets[ui.activeChoiceSetIndex];

    if (choiceSet.options.empty()) {
        return;
    }

    std::vector<int> visibleOptionIndices;
    BuildVisibleOptionIndices(choiceSet, ui, visibleOptionIndices);

    if (visibleOptionIndices.empty()) {
        return;
    }

    //Font font = GetFontDefault();
    //Font font = state.dialogueFont;
    const float fontSize = 46.0f;
    const float spacing = 2.0f;
    const int shadowOffset = 2;

    for (int visibleIndex = 0; visibleIndex < static_cast<int>(visibleOptionIndices.size()); ++visibleIndex) {
        const int optionIndex = visibleOptionIndices[visibleIndex];
        const DialogueChoiceOptionData& option = choiceSet.options[optionIndex];

        const Rectangle optionRect =
                GetDialogueOptionRect(visibleIndex, static_cast<int>(visibleOptionIndices.size()));

        const bool hovered = (ui.hoveredOptionIndex == visibleIndex);
        const Color textColor = hovered ? Color{255, 230, 120, 255} : WHITE;

        const std::string line = hovered ? ("> " + option.text) : option.text;
        const Vector2 size = MeasureTextEx(state.dialogueFont, line.c_str(), fontSize, spacing);

        Vector2 pos{
                optionRect.x + (optionRect.width - size.x) * 0.5f,
                optionRect.y + (optionRect.height - size.y) * 0.5f
        };
        pos.x = std::round(pos.x);
        pos.y = std::round(pos.y);

        DrawShadowedTextLine(state.dialogueFont, line, pos, fontSize, spacing, textColor, shadowOffset);
    }
}
