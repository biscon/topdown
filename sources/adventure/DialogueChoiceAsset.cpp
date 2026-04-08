#include "adventure/DialogueChoiceAsset.h"

#include <filesystem>
#include <fstream>

#include "raylib.h"
#include "utils/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

static bool HasJsonExtension(const fs::path& p)
{
    return p.has_extension() && p.extension() == ".json";
}

static bool HasDuplicateChoiceSetId(
        const std::vector<DialogueChoiceSetData>& choiceSets,
        const std::string& id)
{
    for (const DialogueChoiceSetData& existing : choiceSets) {
        if (existing.id == id) {
            return true;
        }
    }

    return false;
}

static bool HasDuplicateOptionId(
        const DialogueChoiceSetData& choiceSet,
        const std::string& optionId)
{
    for (const DialogueChoiceOptionData& existing : choiceSet.options) {
        if (existing.id == optionId) {
            return true;
        }
    }

    return false;
}

bool LoadAllDialogueChoiceSets(GameState& state)
{
    state.adventure.dialogueChoiceSets.clear();

    const fs::path dialogueDir = fs::path(ASSETS_PATH "dialogue");
    if (!fs::exists(dialogueDir) || !fs::is_directory(dialogueDir)) {
        TraceLog(LOG_ERROR, "Dialogue directory missing: %s", dialogueDir.string().c_str());
        return false;
    }

    bool anyLoaded = false;

    for (const fs::directory_entry& entry : fs::directory_iterator(dialogueDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path jsonPath = entry.path();
        if (!HasJsonExtension(jsonPath)) {
            continue;
        }

        json root;
        {
            std::ifstream in(jsonPath);
            if (!in.is_open()) {
                TraceLog(LOG_ERROR, "Failed to open dialogue choice file: %s", jsonPath.string().c_str());
                continue;
            }

            in >> root;
        }

        if (!root.contains("choiceSets") || !root["choiceSets"].is_array()) {
            TraceLog(LOG_ERROR,
                     "Dialogue choice file missing choiceSets array: %s",
                     jsonPath.string().c_str());
            continue;
        }

        for (const json& setJson : root["choiceSets"]) {
            DialogueChoiceSetData choiceSet{};

            choiceSet.id = setJson.value("id", "");
            if (choiceSet.id.empty()) {
                TraceLog(LOG_ERROR,
                         "Dialogue choice set missing id in file: %s",
                         jsonPath.string().c_str());
                continue;
            }

            if (HasDuplicateChoiceSetId(state.adventure.dialogueChoiceSets, choiceSet.id)) {
                TraceLog(LOG_ERROR,
                         "Duplicate dialogue choice set id found: %s",
                         choiceSet.id.c_str());
                continue;
            }

            if (!setJson.contains("options") || !setJson["options"].is_array()) {
                TraceLog(LOG_ERROR,
                         "Dialogue choice set '%s' missing options array in file: %s",
                         choiceSet.id.c_str(),
                         jsonPath.string().c_str());
                continue;
            }

            bool setValid = true;

            for (const json& optionJson : setJson["options"]) {
                DialogueChoiceOptionData option{};
                option.id = optionJson.value("id", "");
                option.text = optionJson.value("text", "");

                if (option.id.empty()) {
                    TraceLog(LOG_ERROR,
                             "Dialogue choice set '%s' has option missing id in file: %s",
                             choiceSet.id.c_str(),
                             jsonPath.string().c_str());
                    setValid = false;
                    break;
                }

                if (option.text.empty()) {
                    TraceLog(LOG_ERROR,
                             "Dialogue choice set '%s' has option '%s' missing text in file: %s",
                             choiceSet.id.c_str(),
                             option.id.c_str(),
                             jsonPath.string().c_str());
                    setValid = false;
                    break;
                }

                if (HasDuplicateOptionId(choiceSet, option.id)) {
                    TraceLog(LOG_ERROR,
                             "Dialogue choice set '%s' has duplicate option id '%s'",
                             choiceSet.id.c_str(),
                             option.id.c_str());
                    setValid = false;
                    break;
                }

                choiceSet.options.push_back(option);
            }

            if (!setValid) {
                continue;
            }

            if (choiceSet.options.empty()) {
                TraceLog(LOG_ERROR,
                         "Dialogue choice set '%s' has no options",
                         choiceSet.id.c_str());
                continue;
            }

            state.adventure.dialogueChoiceSets.push_back(choiceSet);
            anyLoaded = true;
        }
    }

    TraceLog(LOG_INFO,
             "Loaded %d dialogue choice sets",
             static_cast<int>(state.adventure.dialogueChoiceSets.size()));

    return anyLoaded;
}
