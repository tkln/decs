#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decs.h"

void decs_init(struct decs *decs)
{
    memset(decs, 0, sizeof(*decs));
}

/* TODO:
 * - Entity removal
 * - Support for better allocation schemes
 * - Separate decs tick into multiple buckets
 */

uint64_t decs_register_comp(struct decs *s, size_t size)
{
    struct component *comp;

    ++s->n_comps;
    s->comps = realloc(s->comps, s->n_comps * sizeof(s->comps[0]));

    comp = &s->comps[s->n_comps - 1];

    comp->size  = size;
    comp->data  = NULL;

    return s->n_comps - 1;
}

void decs_register_comp_func(struct decs *s, comp_bits_type comps,
                             comp_func_type func, void *func_data)
{
    struct comp_func *comp_func;
    size_t i;

    ++s->n_comp_funcs;
    s->comp_funcs = realloc(s->comp_funcs,
                            s->n_comp_funcs * sizeof(struct comp_func));
    comp_func = &s->comp_funcs[s->n_comp_funcs - 1];

    comp_func->func         = func;
    comp_func->func_data    = func_data;
    comp_func->comps        = comps;
}

uint64_t decs_alloc_entity(struct decs *s, comp_bits_type comp_ids)
{
    int cid, i;
    struct component *comp;

    ++s->n_entities;
    s->entity_comp_map = realloc(s->entity_comp_map,
                                  s->n_entities * sizeof(*s->entity_comp_map));
    s->entity_comp_map[s->n_entities - 1] = comp_ids;

    for (cid = 0, i = 0; cid < s->n_comps; ++cid) {
        comp = s->comps + cid;
        comp->data = realloc(comp->data, comp->size * s->n_entities);
    }

    return s->n_entities - 1;
}

void decs_tick(struct decs *s)
{
    int fidx, eid;
    comp_bits_type comps;
    comp_func_type func;
    void *data;

    for (fidx = 0; fidx < s->n_comp_funcs; ++fidx) {
        comps = s->comp_funcs[fidx].comps;
        func = s->comp_funcs[fidx].func;
        data = s->comp_funcs[fidx].func_data;
        for (eid = 0; eid < s->n_entities; ++eid)
            if ((comps & s->entity_comp_map[fidx]) == comps)
                func(s, eid, data);
    }
}

void decs_cleanup(struct decs *s)
{
    int cid;

    for (cid = 0; cid < s->n_comps; ++cid)
        free(s->comps[cid].data);

    free(s->comp_funcs);
    free(s->entity_comp_map);
    free(s->comps);
}
