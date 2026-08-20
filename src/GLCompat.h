#pragma once

// Native builds use the generated OpenGL loader. Emscripten provides the
// WebGL/OpenGL ES entry points directly, so a desktop GL loader must not be
// initialized in the browser build.
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
