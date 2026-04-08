#include "debug/DebugConsoleInternal.h"
#include "debug/DebugConsole.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "input/Input.h"
#include "raylib.h"
#include "scripting/ScriptSystem.h"

static bool IsPrintableConsoleCodepoint(unsigned int codepoint)
{
    return codepoint >= 32 && codepoint != 127;
}

static bool IsPointInsideDebugConsolePanel(Vector2 p)
{
    const Rectangle panelRect{
            static_cast<float>(CONSOLE_PADDING),
            static_cast<float>(CONSOLE_TOP),
            static_cast<float>(INTERNAL_WIDTH - CONSOLE_PADDING * 2),
            static_cast<float>(CONSOLE_HEIGHT)
    };

    return CheckCollisionPointRec(p, panelRect);
}

void DebugConsoleClampCaret(DebugConsoleData& console)
{
    if (console.caretIndex < 0) {
        console.caretIndex = 0;
    }

    const int maxIndex = static_cast<int>(console.input.size());
    if (console.caretIndex > maxIndex) {
        console.caretIndex = maxIndex;
    }
}

void DebugConsoleResetBlink(DebugConsoleData& console)
{
    console.caretBlinkMs = 0.0f;
    console.caretVisible = true;
}

void DebugConsoleClampState(DebugConsoleData& console)
{
    if (console.maxLines < 1) {
        console.maxLines = 1;
    }

    if (console.visibleLines < 1) {
        console.visibleLines = 1;
    }

    const int maxScroll = std::max(0, static_cast<int>(console.lines.size()) - console.visibleLines);
    if (console.scrollOffset < 0) {
        console.scrollOffset = 0;
    }
    if (console.scrollOffset > maxScroll) {
        console.scrollOffset = maxScroll;
    }

    if (console.history.empty()) {
        console.historyIndex = -1;
        console.historyBrowsing = false;
    } else if (console.historyIndex < -1) {
        console.historyIndex = -1;
    } else if (console.historyIndex >= static_cast<int>(console.history.size())) {
        console.historyIndex = static_cast<int>(console.history.size()) - 1;
    }

    DebugConsoleClampCaret(console);
}

static std::string SanitizeClipboardTextForConsole(const char* text)
{
    if (text == nullptr) {
        return {};
    }

    std::string out;
    out.reserve(std::strlen(text));

    bool previousWasSpace = false;

    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != 0; ++p) {
        const unsigned char ch = *p;

        if (ch == '\r' || ch == '\n' || ch == '\t') {
            if (!previousWasSpace) {
                out.push_back(' ');
                previousWasSpace = true;
            }
            continue;
        }

        if (ch < 32 || ch == 127) {
            continue;
        }

        out.push_back(static_cast<char>(ch));
        previousWasSpace = (ch == ' ');
    }

    return out;
}

static void StopConsoleHistoryBrowsing(DebugConsoleData& console)
{
    console.historyBrowsing = false;
    console.historyIndex = -1;
    console.historyDraftInput.clear();
    console.historyDraftCaretIndex = 0;
}

static void InsertConsoleText(DebugConsoleData& console, const std::string& text)
{
    if (console.historyBrowsing) {
        StopConsoleHistoryBrowsing(console);
    }

    DebugConsoleClampCaret(console);
    console.input.insert(static_cast<size_t>(console.caretIndex), text);
    console.caretIndex += static_cast<int>(text.size());
}

static void BackspaceConsoleText(DebugConsoleData& console)
{
    if (console.historyBrowsing) {
        StopConsoleHistoryBrowsing(console);
    }

    DebugConsoleClampCaret(console);

    if (console.caretIndex <= 0 || console.input.empty()) {
        return;
    }

    console.input.erase(static_cast<size_t>(console.caretIndex - 1), 1);
    console.caretIndex--;
}

static void DeleteConsoleText(DebugConsoleData& console)
{
    if (console.historyBrowsing) {
        StopConsoleHistoryBrowsing(console);
    }

    DebugConsoleClampCaret(console);

    if (console.caretIndex < 0 ||
        console.caretIndex >= static_cast<int>(console.input.size())) {
        return;
    }

    console.input.erase(static_cast<size_t>(console.caretIndex), 1);
}

static void MoveConsoleCaretHome(DebugConsoleData& console)
{
    console.caretIndex = 0;
    DebugConsoleResetBlink(console);
}

static void MoveConsoleCaretEnd(DebugConsoleData& console)
{
    console.caretIndex = static_cast<int>(console.input.size());
    DebugConsoleResetBlink(console);
}

static void PasteClipboardIntoConsole(DebugConsoleData& console)
{
    const char* clipboardText = GetClipboardText();
    const std::string sanitized = SanitizeClipboardTextForConsole(clipboardText);

    if (sanitized.empty()) {
        return;
    }

    InsertConsoleText(console, sanitized);
    DebugConsoleResetBlink(console);
}

static void SubmitConsoleLine(GameState& state)
{
    DebugConsoleData& console = state.debug.console;

    if (console.input.empty()) {
        return;
    }

    const std::string submitted = console.input;

    DebugConsoleAddLine(state, "> " + submitted, WHITE);

    if (console.history.empty() || console.history.back() != submitted) {
        console.history.push_back(submitted);
    }

    console.historyIndex = -1;
    console.input.clear();
    console.caretIndex = 0;
    console.historyDraftInput.clear();
    console.historyDraftCaretIndex = 0;
    console.historyBrowsing = false;

    if (!submitted.empty() && submitted[0] == '/') {
        ExecuteConsoleSlashCommand(state, submitted);
        return;
    }

    std::string outResult;
    std::string outError;
    const bool ok = ScriptSystemExecuteConsoleLine(
            state.script,
            submitted,
            outResult,
            outError);

    if (!ok) {
        DebugConsoleAddLine(state, outError, RED);
        return;
    }

    if (!outResult.empty()) {
        DebugConsoleAddLine(state, outResult, SKYBLUE);
    }
}

static void RecallHistoryUp(DebugConsoleData& console)
{
    if (console.history.empty()) {
        return;
    }

    if (!console.historyBrowsing) {
        console.historyDraftInput = console.input;
        console.historyDraftCaretIndex = console.caretIndex;
        console.historyBrowsing = true;
        console.historyIndex = static_cast<int>(console.history.size()) - 1;
    } else if (console.historyIndex > 0) {
        console.historyIndex--;
    }

    if (console.historyIndex >= 0 &&
        console.historyIndex < static_cast<int>(console.history.size())) {
        console.input = console.history[console.historyIndex];
        console.caretIndex = static_cast<int>(console.input.size());
    }

    DebugConsoleClampCaret(console);
}

static void RecallHistoryDown(DebugConsoleData& console)
{
    if (console.history.empty()) {
        return;
    }

    if (!console.historyBrowsing) {
        return;
    }

    if (console.historyIndex < static_cast<int>(console.history.size()) - 1) {
        console.historyIndex++;
        console.input = console.history[console.historyIndex];
        console.caretIndex = static_cast<int>(console.input.size());
    } else {
        console.input = console.historyDraftInput;
        console.caretIndex = console.historyDraftCaretIndex;
        console.historyIndex = -1;
        console.historyBrowsing = false;
        console.historyDraftInput.clear();
        console.historyDraftCaretIndex = 0;
    }

    DebugConsoleClampCaret(console);
}

static bool IsConsoleRepeatableKey(int key)
{
    switch (key) {
        case KEY_BACKSPACE:
        case KEY_DELETE:
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_UP:
        case KEY_DOWN:
        case KEY_HOME:
        case KEY_END:
        case KEY_PAGE_UP:
        case KEY_PAGE_DOWN:
            return true;

        default:
            return false;
    }
}

static bool HandleDebugConsoleKeyEvent(GameState& state,
                                       InputEvent& ev,
                                       bool isRepeat,
                                       bool& suppressTextInputThisFrame)
{
    DebugConsoleData& console = state.debug.console;

    if (!isRepeat && ev.key.key == KEY_GRAVE) {
        console.open = !console.open;
        DebugConsoleResetBlink(console);
        suppressTextInputThisFrame = true;
        ConsumeEvent(ev);
        return true;
    }

    if (!console.open) {
        return false;
    }

    if (isRepeat && !IsConsoleRepeatableKey(ev.key.key)) {
        return false;
    }

    switch (ev.key.key) {
        case KEY_ENTER:
            if (!isRepeat) {
                SubmitConsoleLine(state);
                DebugConsoleResetBlink(console);
                ConsumeEvent(ev);
                return true;
            }
            break;

        case KEY_BACKSPACE:
            BackspaceConsoleText(console);
            DebugConsoleResetBlink(console);
            ConsumeEvent(ev);
            return true;

        case KEY_DELETE:
            DeleteConsoleText(console);
            DebugConsoleResetBlink(console);
            ConsumeEvent(ev);
            return true;

        case KEY_HOME:
            MoveConsoleCaretHome(console);
            ConsumeEvent(ev);
            return true;

        case KEY_END:
            MoveConsoleCaretEnd(console);
            ConsumeEvent(ev);
            return true;

        case KEY_V:
        {
            if (isRepeat) {
                break;
            }

            const bool ctrlDown =
                    IsKeyDown(KEY_LEFT_CONTROL) ||
                    IsKeyDown(KEY_RIGHT_CONTROL);

            if (ctrlDown) {
                PasteClipboardIntoConsole(console);
                ConsumeEvent(ev);
                return true;
            }
            break;
        }

        case KEY_L:
        {
            if (isRepeat) {
                break;
            }

            const bool ctrlDown =
                    IsKeyDown(KEY_LEFT_CONTROL) ||
                    IsKeyDown(KEY_RIGHT_CONTROL);

            if (ctrlDown) {
                console.lines.clear();
                console.scrollOffset = 0;
                DebugConsoleResetBlink(console);
                ConsumeEvent(ev);
                return true;
            }
            break;
        }

        case KEY_LEFT:
            if (console.historyBrowsing) {
                StopConsoleHistoryBrowsing(console);
            }
            if (console.caretIndex > 0) {
                console.caretIndex--;
                DebugConsoleResetBlink(console);
            }
            ConsumeEvent(ev);
            return true;

        case KEY_RIGHT:
            if (console.historyBrowsing) {
                StopConsoleHistoryBrowsing(console);
            }
            if (console.caretIndex < static_cast<int>(console.input.size())) {
                console.caretIndex++;
                DebugConsoleResetBlink(console);
            }
            ConsumeEvent(ev);
            return true;

        case KEY_UP:
            RecallHistoryUp(console);
            DebugConsoleResetBlink(console);
            ConsumeEvent(ev);
            return true;

        case KEY_DOWN:
            RecallHistoryDown(console);
            DebugConsoleResetBlink(console);
            ConsumeEvent(ev);
            return true;

        case KEY_PAGE_UP:
            console.scrollOffset += 20;
            DebugConsoleClampState(console);
            ConsumeEvent(ev);
            return true;

        case KEY_PAGE_DOWN:
            console.scrollOffset -= 20;
            DebugConsoleClampState(console);
            ConsumeEvent(ev);
            return true;

        case KEY_ESCAPE:
            if (!isRepeat) {
                console.open = false;
                ConsumeEvent(ev);
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void UpdateDebugConsoleInputInternal(GameState& state, float dt)
{
    DebugConsoleData& console = state.debug.console;

    console.caretBlinkMs += dt * 1000.0f;
    if (console.caretBlinkMs >= 500.0f) {
        console.caretBlinkMs = 0.0f;
        console.caretVisible = !console.caretVisible;
    }

    bool suppressTextInputThisFrame = false;

    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyPressed)) {
        HandleDebugConsoleKeyEvent(state, ev, false, suppressTextInputThisFrame);
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::KeyRepeated)) {
        HandleDebugConsoleKeyEvent(state, ev, true, suppressTextInputThisFrame);
    }

    if (console.open) {
        for (auto& ev : FilterEvents(state.input, true, InputEventType::MouseClick)) {
            if (IsPointInsideDebugConsolePanel(ev.mouse.pos)) {
                ConsumeEvent(ev);
            }
        }
    }

    if (!console.open) {
        return;
    }

    if (suppressTextInputThisFrame) {
        for (auto& ev : FilterEvents(state.input, true, InputEventType::TextInput)) {
            ConsumeEvent(ev);
        }
        return;
    }

    for (auto& ev : FilterEvents(state.input, true, InputEventType::TextInput)) {
        if (!IsPrintableConsoleCodepoint(ev.text.codepoint)) {
            continue;
        }

        InsertConsoleText(console, std::string(1, static_cast<char>(ev.text.codepoint)));
        DebugConsoleResetBlink(console);
        ConsumeEvent(ev);
    }

    DebugConsoleClampState(console);
}
