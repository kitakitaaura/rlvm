// Force-included (via -include) into every engine translation unit on the
// Emscripten build. rlvm's shader code (systems/sdl/shaders.cc,
// sdl_colour_filter.cc) calls glUseProgram/glUniform*/glCreateShader etc,
// which SDL_opengl.h's bundled glext prototypes declare as extern function
// *pointers* on desktop GL (loaded via GLEW at runtime) but does not
// actually define under Emscripten. Pull in the real GLES2 declarations
// (Emscripten's -sFULL_ES2=1 provides the implementations) before any
// engine header gets a chance to include SDL_opengl.h.
#if defined(__EMSCRIPTEN__)
#include <GLES2/gl2.h>
#endif
