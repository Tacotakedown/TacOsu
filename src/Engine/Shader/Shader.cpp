#include "Shader.h"
#include "ConVar/ConVar.h"

ConVar _debug_shaders("debug_shaders", false);

ConVar* Shader::debug_shaders = &_debug_shaders;