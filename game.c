#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <SDL2/SDL.h>

#include "decs.h"
#include "vec3.h"
#include "ttf.h"

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

static const struct system_reg phys_drag_sys = {
    .name       = "phys_drag",
    .comp_names = STR_ARR("phys"),
    .func       = phys_drag_tick,
};

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

static const struct system_reg phys_sys = {
    .name       = "phys",
    .comp_names = STR_ARR("phys"),
    .func       = phys_tick,
    .dep_names  = STR_ARR("phys_drag", "phys_gravity"),
};

struct gravity_ctx {
    struct phys_comp *phys_base;
};

static void gravity_tick(struct decs *decs, uint64_t eid, void *func_data)
{
    struct gravity_ctx *ctx = func_data;
    struct phys_comp *phys = ctx->phys_base + eid;

    phys->force  = vec3_add(phys->force, (struct vec3) { 0.0f, 9.81f, 0.0f });
}

static const struct system_reg gravity_sys = {
    .name       = "phys_gravity",
    .comp_names = STR_ARR("phys"),
    .func       = gravity_tick,
};

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

static struct system_reg render_sys = {
    .name       = "render",
    .comp_names = STR_ARR("phys", "color"),
    .func       = render_tick,
    .dep_names  = STR_ARR("phys"),
};

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
}

static void render_system_perf_stats(const struct decs *decs)
{
    int pt_size = 16;
    size_t i;
    struct perf_stats *stats;

    ttf_printf(0, 0, "entity count: %zu", decs->n_entities);
    for (i = 0; i < decs->n_systems; ++i) {
        stats = &decs->systems[i].perf_stats;
        ttf_printf(0, pt_size * (1 + i * 6), "%s:", decs->systems[i].name);
        ttf_printf(64, pt_size * (2 + i * 6), "cpu cycles: %lld, (%lld)",
                 stats->cpu_cycles, stats->cpu_cycles / decs->n_entities);
        ttf_printf(64, pt_size * (3 + i * 6), "l3 cache refs: %lld, (%lld)",
                 stats->cache_refs, stats->cache_refs / decs->n_entities);
        ttf_printf(64, pt_size * (4 + i * 6), "l3 cache misses: %lld, (%lld)",
                   stats->cache_misses,
                   stats->cache_misses / decs->n_entities);
        ttf_printf(64, pt_size * (5 + i * 6),
                   "branch instructions: %lld, (%lld)", stats->branch_instrs,
                   stats->branch_instrs / decs->n_entities);
        ttf_printf(64, pt_size * (6 + i * 6), "branch misses: %lld, (%lld)",
                   stats->branch_misses,
                   stats->branch_misses / decs->n_entities);
    }
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
    render_sys.aux_ctx = &render_ctx_aux;

    decs_init(&decs);
    ttf_init(rend, win, NULL);

    comp_ids.phys = decs_register_comp(&decs, "phys", sizeof(struct phys_comp));
    comp_ids.color = decs_register_comp(&decs, "color",
                                        sizeof(struct color_comp));

    decs_register_system(&decs, &gravity_sys);
    decs_register_system(&decs, &phys_drag_sys);
    decs_register_system(&decs, &phys_sys);
    decs_register_system(&decs, &render_sys);

    while (runnig) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                runnig = 0;
        }

        create_particle(&decs, &comp_ids);

        SDL_SetRenderDrawColor(rend, 0x0, 0x0, 0x0, 0xff);
        SDL_RenderClear(rend);

        decs_tick(&decs);
        render_system_perf_stats(&decs);

        SDL_RenderPresent(rend);

        SDL_Delay(16);
    }

    decs_cleanup(&decs);

    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
