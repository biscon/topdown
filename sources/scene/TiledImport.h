#pragma once

#include "scene/SceneData.h"
#include "resources/ResourceData.h"

bool ImportTiledSceneIntoSceneData(SceneData& scene, ResourceData& resources, const char* tiledFilePath);
