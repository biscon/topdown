#pragma once

#include <string>
#include <vector>

enum class TopdownNarrationPopupPhase {
    PanelBuild,
    BorderReveal,
    TextReveal,
    Hold,
    Exit
};

struct TopdownNarrationPopupEntry {
    bool active = false;

    std::string title;
    std::string body;

    TopdownNarrationPopupPhase phase = TopdownNarrationPopupPhase::PanelBuild;
    float phaseElapsed = 0.0f;
    float holdDuration = 0.0f;

    float measuredHeight = 0.0f;
};

struct TopdownNarrationPopupsRuntime {
    std::vector<TopdownNarrationPopupEntry> entries;
};