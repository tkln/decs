#include <stdarg.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "ttf.h"


static const char default_font_path[] =
		"/usr/share/fonts/dejavu/DejaVuSansMono.ttf";

static int pt_size = 12;
static TTF_Font *font;
static SDL_Renderer *rend;
static SDL_Window *win;

int ttf_init(SDL_Renderer *renderer, SDL_Window *window, const char *font_path)
{
    TTF_Init();

    rend = renderer;
    win = window;

    if (!font_path)
        font_path = default_font_path;

    font = TTF_OpenFont(font_path, pt_size);
    if (font == NULL) {
        fprintf(stderr, "Could not open font: %s", font_path);
        return -1;
    }

}

void ttf_render(int x, int y, const char *str)
{
    SDL_Rect rect;
    SDL_Surface *surf;
    SDL_Color color = {255, 255, 255, 0};
    SDL_Color bg_color = {0, 0, 0, 0};
    SDL_Texture *texture;

    surf = TTF_RenderText_Blended(font, str, color);
    texture = SDL_CreateTextureFromSurface(rend, surf);
    rect.x = x;
    rect.y = y;
    rect.w = surf->w;
    rect.h = surf->h;

    SDL_RenderCopy(rend, texture, NULL, &rect);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(texture);
}

int ttf_printf(int x, int y, const char *fmt, ...)
{
    va_list sz_vargs, vargs;
    int sz;
    char *buf;

    va_start(sz_vargs, fmt);
    va_copy(vargs, sz_vargs);
    sz = vsnprintf(NULL, 0, fmt, sz_vargs) + 1;
    va_end(sz_vargs);

    buf = malloc(sz);
    sz = vsnprintf(buf, sz, fmt, vargs);
    va_end(vargs);

    ttf_render(x, y, buf);
    free(buf);

    return sz;
}
