#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <SDL2/SDL.h>

#include "decs.h"
#include "vec3.h"

struct phys_comp  {
    struct vec3 pos;
    struct vec3 vel;
    float mass;
};

struct color_comp {
    union {
        struct vec3 color;
        struct {
            float r, g, b;
        };
    };
};

struct comp_ids {
    uint64_t phys;
    uint64_t color;
};

struct render_ctx {
    struct comp_ids *comp_ids;
    SDL_Window *win;
    SDL_Renderer *rend;
};

struct phys_ctx {
    struct comp_ids *comp_ids;
    struct vec3 force_point;
};

static struct vec3 phys_drag_force(const struct phys_comp *phys)
{
    const float fluid_density = 1.2f; // Air
    const float area = 10.10f;
    const float drag_coef = 0.47f; // Sphere
    const float total_drag_coef = -0.5f * fluid_density * drag_coef * area;
    const struct vec3 vel2 = vec3_pow2(phys->vel);
    struct vec3 drag_force = vec3_muls(vel2, total_drag_coef);

    return drag_force;
}

static void phys_euler_tick(struct phys_comp *phys, struct vec3 force, float dt)
{
    struct vec3 acc = vec3_muls(force, 1.0f / phys->mass);

    struct vec3 d_vel = vec3_muls(acc, dt);
    struct vec3 d_pos = vec3_muls(phys->vel, dt);

    phys->vel = vec3_add(phys->vel, d_vel);
    phys->pos = vec3_add(phys->pos, d_pos);
}

static void phys_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct phys_ctx *ctx = func_data;
    struct phys_comp *phys = decs_get_comp(decs, ctx->comp_ids->phys, eid);

    float dt = 1.0f / 60.0f;

    struct vec3 force_field = { phys->pos.x - ctx->force_point.x,
                                phys->pos.y - ctx->force_point.y,
                                0.0f };
    struct vec3 drag_force = phys_drag_force(phys);
    struct vec3 total_force = vec3_add(force_field, drag_force);

    phys_euler_tick(phys, total_force, dt);

    /* Wraparound */
    phys->pos.x = fmod(phys->pos.x, 1.0f);
    phys->pos.y = fmod(phys->pos.y, 1.0f);
    if (phys->pos.x < 0.0f)
        phys->pos.x += 1.0f;
    if (phys->pos.y < 0.0f)
        phys->pos.y += 1.0f;
}

static void render_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct render_ctx *ctx = func_data;
    struct phys_comp *phys = decs_get_comp(decs, ctx->comp_ids->phys, eid);
    struct color_comp *color = decs_get_comp(decs, ctx->comp_ids->color, eid);

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

void create_particle(struct decs *decs, struct comp_ids *comp_ids)
{
    struct phys_comp *phys;
    struct color_comp *color;
    uint64_t eid;

    eid = decs_alloc_entity(decs, (1<<comp_ids->phys) | (1<<comp_ids->color));

    phys = decs_get_comp(decs, comp_ids->phys, eid);
    color = decs_get_comp(decs, comp_ids->color, eid);

    /* XXX */
    phys->pos.x = eid * 0.01f;
    phys->pos.y = eid * 0.02f;
    phys->pos.z = eid * 0.03f;
    phys->vel.x = sin(eid * 0.1) * 0.004f;
    phys->vel.y = sin(eid * 0.1) * 0.005f;
    phys->vel.z = sin(eid * 0.1) * 0.006f;
    phys->mass = 1.0f;
    color->r = eid * 0.1 * 2;
    color->g = eid * 0.3 * 2;
    color->b = eid * 0.2 * 2;
}

int main(void)
{
    struct decs decs;
    struct comp_ids comp_ids;
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

    render_ctx.comp_ids = &comp_ids;
    render_ctx.rend = rend;
    render_ctx.win = win;

    phys_ctx.comp_ids = &comp_ids;

    decs_init(&decs);

    comp_ids.phys = decs_register_comp(&decs, sizeof(struct phys_comp));
    comp_ids.color = decs_register_comp(&decs, sizeof(struct color_comp));

    decs_register_system(&decs, 1<<comp_ids.phys, phys_tick, &phys_ctx);

    decs_register_system(&decs, (1<<comp_ids.phys) | (1<<comp_ids.color),
                         render_tick, &render_ctx);

    for (i = 0; i < 2048; ++i)
        create_particle(&decs, &comp_ids);

    while (runnig) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                runnig = 0;
        }

        create_particle(&decs, &comp_ids);

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
