#ifndef TTF_H
#define TTF_H
#include <SDL2/SDL.h>

int ttf_init(SDL_Renderer *renderer, SDL_Window *window, const char *font_path);
void ttf_render(int x, int y, const char *str);
int ttf_printf(int x, int y, const char *fmt, ...);

#endif
