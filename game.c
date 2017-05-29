#include <stdlib.h>
#include <stdio.h>

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

static void phys_tick(struct scene *scene, uint64_t eid, void *func_data)
{
    struct ids *ids = func_data;
    struct phys_comp *phys = scene_get_comp(scene, ids->phys_comp, eid);

    /* XXX */
    printf("\n%s(%lu):\nphys.pos.x: %f, phys.pos.y: %f, phys.pos.z: %f,\n"
           "phys.vel.x: %f, phys.vel.y: %f, phys.vel.z: %f\n", __func__, eid,
           phys->pos.x, phys->pos.y, phys->pos.z,
           phys->vel.x, phys->vel.y, phys->vel.z);
}

static void render_tick(struct scene *scene, uint64_t eid, void *func_data)
{
    struct ids *ids = func_data;
    struct phys_comp *phys = scene_get_comp(scene, ids->phys_comp, eid);
    struct color_comp *color = scene_get_comp(scene, ids->color_comp, eid);

    /* XXX */
    printf("\n%s(%lu):\ncolor.r: %f, color.g: %f, color.b: %f\n", __func__, eid,
           color->r, color->g, color->b);
}

void create_particle(struct scene *scene, struct ids *ids)
{
    struct phys_comp *phys;
    struct color_comp *color;
    uint64_t eid;

    eid = scene_alloc_entity(scene, (1<<ids->phys_comp) | (1<<ids->color_comp));

    phys = scene_get_comp(scene, ids->phys_comp, eid);
    color = scene_get_comp(scene, ids->color_comp, eid);

    /* XXX */
    phys->pos.x = eid * 1.0f;
    phys->pos.y = eid * 2.0f;
    phys->pos.z = eid * 3.0f;
    phys->vel.x = eid * 0.04f;
    phys->vel.y = eid * 0.05f;
    phys->vel.z = eid * 0.06f;
    color->r = eid * 0.1;
    color->g = eid * 0.3;
    color->b = eid * 0.2;
}

int main(void)
{
    static struct scene scene;
    static struct ids ids;

    ids.phys_comp = scene_register_comp(&scene, sizeof(struct phys_comp)); 
    ids.color_comp = scene_register_comp(&scene, sizeof(struct color_comp));

    scene_register_comp_func(&scene, 1<<ids.phys_comp, phys_tick, &ids);

    scene_register_comp_func(&scene, (1<<ids.phys_comp) | (1<<ids.color_comp),
                             render_tick, &ids);

    create_particle(&scene, &ids);
    create_particle(&scene, &ids);
    create_particle(&scene, &ids);

    scene_tick(&scene);

    scene_cleanup(&scene);

    return 0;
}
