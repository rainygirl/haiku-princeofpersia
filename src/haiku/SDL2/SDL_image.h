/* PNG loading and saving for the engine, done with the Haiku Translation Kit. */
#ifndef HAIKU_POP_SDL_IMAGE_H
#define HAIKU_POP_SDL_IMAGE_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

SDL_Surface* IMG_Load(const char* file);
SDL_Surface* IMG_Load_RW(SDL_RWops* src, int freesrc);
int IMG_SavePNG(SDL_Surface* surface, const char* file);
const char* IMG_GetError(void);

#ifdef __cplusplus
}
#endif

#endif
