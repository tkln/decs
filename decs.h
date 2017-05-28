#include <stdint.h>

#ifndef DECS_H
#define DECS_H

struct scene;
typedef uint64_t comp_bits_type;
typedef void (*comp_func_type)(struct scene *scene, uint64_t eid, void *data);

struct component {
    size_t size;
    void *data;
};

struct comp_func {
    comp_func_type func;
    void *func_data;
    comp_bits_type comps;
};

struct scene {
    struct component *comps;
    size_t n_comps;
    comp_bits_type *entity_comp_map;
    size_t n_entities;
    struct comp_func *comp_funcs;
    size_t n_comp_funcs;
};

uint64_t scene_register_comp(struct scene *scene, size_t size);

void scene_register_comp_func(struct scene *scene, comp_bits_type comps,
                              comp_func_type func, void *func_data);

uint64_t scene_alloc_entity(struct scene *scene, comp_bits_type comp_ids);

static inline void *scene_get_comp(struct scene *s, uint64_t cid, uint64_t eid)
{
    struct component *comp = s->comps + cid;
    return comp->data + eid * comp->size;
}

void scene_tick(struct scene *scene);

#endif
