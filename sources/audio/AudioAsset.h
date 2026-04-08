#pragma once

#include <string>
#include "audio/AudioData.h"

bool LoadAudioDefinitions(const std::string& path, std::vector<AudioDefinitionData>& outDefs);
