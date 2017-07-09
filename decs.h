#include <stdint.h>

#ifndef DECS_H
#define DECS_H

struct decs;
typedef uint64_t comp_bits_type;
typedef void (*comp_func_type)(struct decs *decs, uint64_t eid, void *data);

struct component {
    size_t size;
    void *data;
};

struct comp_func {
    comp_func_type func;
    void *func_data;
    comp_bits_type comps;
};

struct decs {
    struct component *comps;
    size_t n_comps;
    comp_bits_type *entity_comp_map;
    size_t n_entities;
    struct comp_func *comp_funcs;
    size_t n_comp_funcs;
};

void decs_init(struct decs *decs);

uint64_t decs_register_comp(struct decs *decs, size_t size);

void decs_register_comp_func(struct decs *decs, comp_bits_type comps,
                             comp_func_type func, void *func_data);

uint64_t decs_alloc_entity(struct decs *decs, comp_bits_type comp_ids);

static inline void *decs_get_comp(struct decs *s, uint64_t cid, uint64_t eid)
{
    struct component *comp = s->comps + cid;
    return comp->data + eid * comp->size;
}

void decs_tick(struct decs *decs);

void decs_cleanup(struct decs *decs);
#endif
