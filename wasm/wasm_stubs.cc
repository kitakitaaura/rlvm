// v1 wasm build skips the NWA-format manual resampler (systems/sdl/resample.cc
// + zresample.cc + audiofile.cc + dither.cc), which depends on libsndfile and
// vendored zita-resampler -- neither has an Emscripten port. This stub
// passes audio through unresampled; games whose voice sample rate doesn't
// match SDL_mixer's output rate may sound pitch-shifted until this is
// revisited (see wasm/CMakeLists.txt SYSTEM_SDL_SOURCES comment).

#include "systems/sdl/resample.h"

char* EnsureDataIsCorrectBitrate(char* incoming_data, int* length) {
  return incoming_data;
}

// Emscripten's legacy GL emulation (-sLEGACY_GL_EMULATION=1) implements
// glClientActiveTexture + per-unit glTexCoord2f (see
// src/lib/libglemu.js: glTexCoord2f reads
// GLImmediate.clientAttributes[TEXTURE0 + clientActiveTexture]), but never
// implements glMultiTexCoord2f itself even though SDL_opengl.h declares it.
// systems/sdl/texture.cc uses glMultiTexCoord2f to feed per-unit texcoords
// into a two-texture shader (color-mask compositing) inside a single
// glBegin/glEnd block. Route it through the primitive that's actually
// implemented. glMultiTexCoord2f must not change the client active texture
// as a side effect: rlvm only ever uses unit 0 through plain glTexCoord2f,
// so leaving it on GL_TEXTURE1 sends every later sprite/background texcoord
// to the wrong unit and corrupts the emulation's vertex layout (everything
// drawn after the first text window renders black).
extern "C" {
void glClientActiveTexture(GLenum texture);
void glTexCoord2f(GLfloat s, GLfloat t);

void glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t) {
  glClientActiveTexture(target);
  glTexCoord2f(s, t);
  glClientActiveTexture(GL_TEXTURE0);
}
}
