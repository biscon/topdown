//
// Created by bison on 18-11-25.
//

#pragma once
#include "InputData.h"

template<typename Pred, typename VecPtr>
struct EventView {

    VecPtr events;   // either vector* or const vector*
    Pred pred;

    struct Iterator {
        VecPtr events;
        Pred pred;
        size_t index;

        void advance() {
            while (index < events->size() && !pred((*events)[index])) {
                ++index;
            }
        }

        // IMPORTANT: reference type depends on VecPtr constness
        using Ref = decltype((*events)[0]);

        Ref operator*() const { return (*events)[index]; }
        Iterator& operator++() { ++index; advance(); return *this; }
        bool operator!=(const Iterator& other) const { return index != other.index; }
    };

    Iterator begin() {
        Iterator it{ events, pred, 0 };
        it.advance();
        return it;
    }

    Iterator end() {
        return Iterator{ events, pred, events->size() };
    }
};

void InitInput(InputData &st);
void UpdateInput(InputData &input);

inline void FlushEvents(InputData &input) {
    input.events.clear();
    input.lastClickTime = -1.0f;
    input.doubleClickThreshold = 0.3f;
    input.keyRepeatStates = {};
    input.keyRepeatInitialDelay = 0.45f;
    input.keyRepeatInterval = 0.04f;
}

inline const std::vector<InputEvent>& PeekEvents(const InputData& input) {
    return input.events;
}

inline void ConsumeEvent(InputEvent& evt) {
    evt.handled = true;
}

inline auto FilterEvents(
        InputData& input,
        bool unhandledOnly,
        InputEventType typeFilter)
{
    auto pred = [unhandledOnly, typeFilter](const InputEvent& e) {
        if (unhandledOnly && e.handled) return false;
        if (typeFilter != InputEventType::Any &&
            e.type != typeFilter) return false;
        return true;
    };

    using VecPtr = std::vector<InputEvent>*;
    return EventView<decltype(pred), VecPtr>{ &input.events, pred };
}

inline auto FilterEvents(
        const InputData& input,
        bool unhandledOnly,
        InputEventType typeFilter)
{
    auto pred = [unhandledOnly, typeFilter](const InputEvent& e) {
        if (unhandledOnly && e.handled) return false;
        if (typeFilter != InputEventType::Any &&
            e.type != typeFilter) return false;
        return true;
    };

    using VecPtr = const std::vector<InputEvent>*;
    return EventView<decltype(pred), VecPtr>{ &input.events, pred };
}
