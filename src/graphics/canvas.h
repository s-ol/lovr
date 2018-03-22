#include "graphics/texture.h"
#include "util.h"
#include <stdbool.h>

#pragma once

typedef struct {
  Texture texture;
  GLuint framebuffer;
  GLuint resolveFramebuffer;
  GLuint depthStencilBuffer;
  GLuint msaaTexture;
  int msaa;
  bool stereo;
} Canvas;

bool lovrCanvasSupportsFormat(TextureFormat format);

Canvas* lovrCanvasCreate(int width, int height, TextureFormat format, int msaa, bool depth, bool stencil, bool stereo);
void lovrCanvasDestroy(void* ref);
void lovrCanvasResolve(Canvas* canvas);
TextureFormat lovrCanvasGetFormat(Canvas* canvas);
int lovrCanvasGetMSAA(Canvas* canvas);
