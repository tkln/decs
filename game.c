#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <SDL2/SDL.h>

#include "decs.h"

struct vec3 {
    union {
        float e[3];
        struct {
            float x, y, z;
        };
    };
};

struct phys_comp  {
    struct vec3 pos;
    struct vec3 vel;
};

struct color_comp {
    union {
        struct vec3 color;
        struct {
            float r, g, b;
        };
    };
};

struct ids {
    uint64_t phys_comp;
    uint64_t color_comp;
};

struct render_ctx {
    struct ids *ids;
    SDL_Window *win;
    SDL_Renderer *rend;
};

struct phys_ctx {
    struct ids *ids;
    struct vec3 force_point;
};

static void phys_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct phys_ctx *ctx = func_data;
    struct phys_comp *phys = decs_get_comp(decs, ctx->ids->phys_comp, eid);
    struct vec3 force;

    float dx = -(phys->pos.x - ctx->force_point.x);
    float dy = -(phys->pos.y - ctx->force_point.y);

    force.x = (0.0001f / (sqrtf(dx * dx) + 0.75));
    force.y = (0.0001f / (sqrtf(dy * dy) + 0.75));
    if (dx < 0)
        force.x *= -1.0f;
    if (dy < 0)
        force.y *= -1.0f;

#if 0
    /* XXX */
    printf("\n%s(%lu):\nphys.pos.x: %f, phys.pos.y: %f, phys.pos.z: %f,\n"
           "phys.vel.x: %f, phys.vel.y: %f, phys.vel.z: %f\n", __func__, eid,
           phys->pos.x, phys->pos.y, phys->pos.z,
           phys->vel.x, phys->vel.y, phys->vel.z);
#endif

    phys->pos.x += phys->vel.x;
    phys->pos.y += phys->vel.y;

    phys->pos.x = fmod(phys->pos.x, 1.0f);
    phys->pos.y = fmod(phys->pos.y, 1.0f);
    if (phys->pos.x < 0.0f)
        phys->pos.x += 1.0f;
    if (phys->pos.y < 0.0f)
        phys->pos.y += 1.0f;

    /* Friction */
    phys->vel.x *= 0.99f;
    phys->vel.y *= 0.99f;

    phys->vel.x += force.x;
    phys->vel.y += force.y;
}

static void render_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct render_ctx *ctx = func_data;
    struct phys_comp *phys = decs_get_comp(decs, ctx->ids->phys_comp, eid);
    struct color_comp *color = decs_get_comp(decs, ctx->ids->color_comp, eid);

#if 0
    /* XXX */
    printf("\n%s(%lu):\ncolor.r: %f, color.g: %f, color.b: %f\n", __func__, eid,
           color->r, color->g, color->b);
    printf("\n%s(%lu):\nphys.pos.x: %f, phys.pos.y: %f, phys.pos.z: %f,\n"
           "phys.vel.x: %f, phys.vel.y: %f, phys.vel.z: %f\n", __func__, eid,
           phys->pos.x, phys->pos.y, phys->pos.z,
           phys->vel.x, phys->vel.y, phys->vel.z);
#endif

    SDL_SetRenderDrawColor(ctx->rend, 0xff * color->r, 0xff * color->g,
                           0xff * color->b, 0xff);
    SDL_RenderDrawPoint(ctx->rend, phys->pos.x * 640, phys->pos.y * 480);

}

void create_particle(struct decs *decs, struct ids *ids)
{
    struct phys_comp *phys;
    struct color_comp *color;
    uint64_t eid;

    eid = decs_alloc_entity(decs, (1<<ids->phys_comp) | (1<<ids->color_comp));

    phys = decs_get_comp(decs, ids->phys_comp, eid);
    color = decs_get_comp(decs, ids->color_comp, eid);

    /* XXX */
    phys->pos.x = eid * 0.01f;
    phys->pos.y = eid * 0.02f;
    phys->pos.z = eid * 0.03f;
    phys->vel.x = sin(eid * 0.1) * 0.004f;
    phys->vel.y = sin(eid * 0.1) * 0.005f;
    phys->vel.z = sin(eid * 0.1) * 0.006f;
    color->r = eid * 0.1 * 2;
    color->g = eid * 0.3 * 2;
    color->b = eid * 0.2 * 2;
}

int main(void)
{
    struct decs decs;
    struct ids ids;
    struct render_ctx render_ctx;
    struct phys_ctx phys_ctx;
    int runnig = 1;
    int i;
    int mx, my;

    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Event event;

    SDL_Init(SDL_INIT_EVERYTHING);

    /* TODO error checks */
    win = SDL_CreateWindow("title", SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
    rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    render_ctx.ids = &ids;
    render_ctx.rend = rend;
    render_ctx.win = win;

    phys_ctx.ids = &ids;

    decs_init(&decs);

    ids.phys_comp = decs_register_comp(&decs, sizeof(struct phys_comp));
    ids.color_comp = decs_register_comp(&decs, sizeof(struct color_comp));

    decs_register_comp_func(&decs, 1<<ids.phys_comp, phys_tick, &phys_ctx);

    decs_register_comp_func(&decs, (1<<ids.phys_comp) | (1<<ids.color_comp),
                             render_tick, &render_ctx);

    for (i = 0; i < 2048; ++i)
        create_particle(&decs, &ids);

    while (runnig) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                runnig = 0;
        }

        create_particle(&decs, &ids);

        SDL_GetMouseState(&mx, &my);
        phys_ctx.force_point.x = mx / 640.0f;
        phys_ctx.force_point.y = my / 480.0f;

        SDL_SetRenderDrawColor(rend, 0x22, 0x22, 0x22, 0xff);
        SDL_RenderClear(rend);

        decs_tick(&decs);

        SDL_RenderPresent(rend);

        SDL_Delay(16);
    }

    decs_cleanup(&decs);

    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
