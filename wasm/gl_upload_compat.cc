// WebGL/GLES2 (unlike desktop GL) requires internalformat == format for
// unsized texture formats, doesn't accept the legacy "1/2/3/4" numeric
// internalformat shorthand rlvm's determineProperties() emits, and has no
// GL_BGRA + GL_UNSIGNED_INT_8_8_8_8_REV combo (used throughout
// systems/sdl/texture.cc and sdl_graphics_system.cc to upload rlvm's native
// ARGB8888 surfaces without a CPU-side conversion). This wraps
// glTexImage2D/glTexSubImage2D to paper over both: it always sends a
// WebGL-legal internalformat, and when asked for BGRA+REV it swizzles into a
// real RGBA byte buffer first. texture.cc/sdl_graphics_system.cc #define
// glTexImage2D/glTexSubImage2D to these under __EMSCRIPTEN__, so every
// existing call site gets this for free.
//
// rlvm always builds its 32bit SDL surfaces with Rmask=0xFF0000,
// Gmask=0xFF00, Bmask=0xFF (see sdl_surface.cc's DefaultRmask) -- a packed
// 0xAARRGGBB int, which on this little-endian target lays out in memory as
// bytes [B, G, R, A]. Swapping bytes 0 and 2 per pixel turns that into
// [R, G, B, A], exactly what GL_RGBA + GL_UNSIGNED_BYTE expects.

#include <GLES2/gl2.h>

#include <cstdlib>
#include <cstring>

// Not part of GLES2 core (it's EXT_texture_format_BGRA8888); we only need
// the enum value to recognize and intercept it below, never to pass it to a
// real GL call.
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

namespace {

unsigned char* SwizzleBgraToRgba(const void* src, int pixel_count) {
  const unsigned char* s = static_cast<const unsigned char*>(src);
  unsigned char* out = static_cast<unsigned char*>(malloc(pixel_count * 4));
  for (int i = 0; i < pixel_count; ++i) {
    out[i * 4 + 0] = s[i * 4 + 2];  // R
    out[i * 4 + 1] = s[i * 4 + 1];  // G
    out[i * 4 + 2] = s[i * 4 + 0];  // B
    out[i * 4 + 3] = s[i * 4 + 3];  // A
  }
  return out;
}

}  // namespace

extern "C" {

void WasmTexImage2D(GLenum target,
                    GLint level,
                    GLint /* internalformat, ignored: forced == format */,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels) {
  if (format == GL_BGRA) {
    unsigned char* rgba =
        pixels ? SwizzleBgraToRgba(pixels, width * height) : nullptr;
    glTexImage2D(target, level, GL_RGBA, width, height, border, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
    if (rgba) free(rgba);
    return;
  }

  glTexImage2D(target, level, format, width, height, border, format, type,
               pixels);
}

void WasmTexSubImage2D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type,
                       const void* pixels) {
  if (format == GL_BGRA) {
    unsigned char* rgba =
        pixels ? SwizzleBgraToRgba(pixels, width * height) : nullptr;
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, rgba);
    if (rgba) free(rgba);
    return;
  }

  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                  type, pixels);
}

}  // extern "C"
