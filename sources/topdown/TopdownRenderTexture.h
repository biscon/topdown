#pragma once

#include "raylib.h"

RenderTexture2D LoadTopdownRenderTextureWithStencil(int width, int height);
bool TopdownRenderTextureHasStencil(const RenderTexture2D& target);
