#include <stdint.h>
#include <stdbool.h>

#ifndef DECS_H
#define DECS_H

struct decs;
typedef uint64_t comp_bits_type;
typedef void (*system_func)(struct decs *decs, uint64_t eid, void *data);

struct component {
    size_t size;
    void *data;
};

struct system {
    system_func func;
    void *func_data;
    comp_bits_type comps;
    uint64_t *deps;
    size_t n_deps;
    bool done;
};

struct decs {
    struct component *comps;
    size_t n_comps;
    comp_bits_type *entity_comp_map;
    size_t n_entities;
    struct system *systems;
    size_t n_systems;
};

void decs_init(struct decs *decs);

uint64_t decs_register_comp(struct decs *decs, size_t size);

uint64_t decs_register_system(struct decs *decs, comp_bits_type comps,
                              system_func func, void *func_data,
                              uint64_t *deps, size_t n_deps);

uint64_t decs_alloc_entity(struct decs *decs, comp_bits_type comp_ids);

static inline void *decs_get_comp(struct decs *s, uint64_t cid, uint64_t eid)
{
    struct component *comp = s->comps + cid;
    return comp->data + eid * comp->size;
}

void decs_tick(struct decs *decs);

void decs_cleanup(struct decs *decs);
#endif
