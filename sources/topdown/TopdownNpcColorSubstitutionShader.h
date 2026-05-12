#pragma once

#include "data/GameState.h"

bool InitTopdownNpcColorSubstitutionShader();
void ShutdownTopdownNpcColorSubstitutionShader();

bool BeginTopdownNpcColorSubstitutionShader(
        const TopdownNpcColorSubstitutionDefinition& source,
        const TopdownNpcResolvedColorSubstitution& resolved);
void EndTopdownNpcColorSubstitutionShader();
