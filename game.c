#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <SDL2/SDL.h>

#include "decs.h"
#include "vec3.h"

struct phys_comp  {
    struct vec3 pos;
    struct vec3 vel;
    struct vec3 force;
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

struct sys_ids {
    uint64_t phys;
    uint64_t phys_drag;
    uint64_t force_field;
    uint64_t gravity;
    uint64_t render;
};

struct phys_drag_ctx {
    struct phys_comp *phys_base;
};

static void phys_drag_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct phys_drag_ctx *ctx = func_data;
    struct phys_comp *phys = ctx->phys_base + eid;

    const float fluid_density = 10.2f; // Air
    const float area = 10.10f;
    const float drag_coef = 0.47f; // Sphere
    const float total_drag_coef = -0.5f * fluid_density * drag_coef * area;
    const struct vec3 vel2 = vec3_spow2(phys->vel);
    struct vec3 drag_force = vec3_muls(vel2, total_drag_coef);

    phys->force = vec3_add(phys->force, drag_force);
}

struct phys_ctx {
    struct phys_comp *phys_base;
};

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
    struct phys_ctx *phys_ctx = func_data;
    struct phys_comp *phys = phys_ctx->phys_base + eid;

    float dt = 1.0f / 60.0f;

    phys_euler_tick(phys, phys->force, dt);

    phys->force = (struct vec3){ 0.0f, 0.0f, 0.0f };

    if (phys->pos.y > 1.0f) {
        phys->vel.y *= -0.9f;
    }
    if (phys->pos.x > 1.0f)
        phys->pos.x -= 1.0f;
    if (phys->pos.x < 0.0f)
        phys->pos.x += 1.0f;
}

struct gravity_ctx {
    struct phys_comp *phys_base;
};

static void gravity_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct gravity_ctx *ctx = func_data;
    struct phys_comp *phys = ctx->phys_base + eid;

    phys->force  = vec3_add(phys->force, (struct vec3) { 0.0f, 9.81f, 0.0f });
}

struct render_ctx_aux {
    SDL_Window *win;
    SDL_Renderer *rend;
};

struct render_ctx {
    struct render_ctx_aux *aux;
    struct phys_comp *phys_base;
    struct color_comp *color_base;
};

int win_w = 1280, win_h = 720;

static void render_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct render_ctx *ctx = func_data;
    struct phys_comp *phys = ctx->phys_base + eid;
    struct color_comp *color = ctx->color_base + eid;

    SDL_SetRenderDrawColor(ctx->aux->rend, 0xff * color->r, 0xff * color->g,
                           0xff * color->b, 0xff);
    SDL_RenderDrawPoint(ctx->aux->rend, phys->pos.x * win_w, phys->pos.y * win_h);

}

void create_particle(struct decs *decs, struct comp_ids *comp_ids)
{
    struct phys_comp *phys;
    struct color_comp *color;
    uint64_t eid;

    eid = decs_alloc_entity(decs, (1<<comp_ids->phys) | (1<<comp_ids->color));

    phys = decs_get_comp(decs, comp_ids->phys, eid);
    color = decs_get_comp(decs, comp_ids->color, eid);

    phys->pos.x = 0.5f;
    phys->pos.y = 0.25f;
    phys->pos.z = 0.0f;
    phys->vel.x = cos(eid * 0.25) * 0.2f;
    phys->vel.y = sin(eid * 0.25) * 0.2f;
    phys->vel.z = sin(eid * 0.25) * 0.2f;
    phys->force.x = 0.0f;
    phys->force.y = 0.0f;
    phys->force.z = 0.0f;
    phys->mass = 7.0f;
    color->r = sin(eid * 0.01) * 2;
    color->g = cos(eid * 0.03) * 2;
    color->b = eid * 0.02 * 2;
    printf("eid: %lu\n", eid);
}

int main(void)
{
    struct decs decs;
    struct comp_ids comp_ids;
    uint64_t render_comps;
    int runnig = 1;
    int i;
    int mx, my;

    struct render_ctx_aux render_ctx_aux;

    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Event event;

    SDL_Init(SDL_INIT_EVERYTHING);

    /* TODO error checks */
    win = SDL_CreateWindow("title", SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED, win_w, win_h, 0);
    rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    render_ctx_aux.rend = rend;
    render_ctx_aux.win = win;

    decs_init(&decs);

    comp_ids.phys = decs_register_comp(&decs, "phys", sizeof(struct phys_comp));
    comp_ids.color = decs_register_comp(&decs, "color",
                                        sizeof(struct color_comp));

    decs_register_system(&decs, "gravity", STR_ARR("phys"), NULL, gravity_tick,
                         NULL, NULL);
    decs_register_system(&decs, "phys_drag", STR_ARR("phys"), NULL,
                         phys_drag_tick, NULL, NULL);
    decs_register_system(&decs, "phys", STR_ARR("phys"), NULL, phys_tick, NULL,
                         STR_ARR("phys_drag", "gravity"));
    decs_register_system(&decs, "render", STR_ARR("phys", "color"), NULL,
                         render_tick, &render_ctx_aux, STR_ARR("phys"));

    while (runnig) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                runnig = 0;
        }

        create_particle(&decs, &comp_ids);

        SDL_SetRenderDrawColor(rend, 0x0, 0x0, 0x0, 0xff);
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
