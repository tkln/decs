#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decs.h"

void scene_init(struct scene *scene)
{
    memset(scene, 0, sizeof(*scene));
}

/* TODO:
 * - Entity removal
 * - Support for better allocation schemes
 * - Separate scene tick into multiple buckets
 */

uint64_t scene_register_comp(struct scene *s, size_t size)
{
    struct component *info;

    ++s->n_comps;
    s->comps = realloc(s->comps, s->n_comps * sizeof(s->comps[0]));

    info = &s->comps[s->n_comps - 1];

    info->size  = size;
    info->data  = NULL;

    return s->n_comps - 1;
}

void scene_register_comp_func(struct scene *s, comp_bits_type comps,
                              comp_func_type func, void *func_data)
{
    struct comp_func *comp_func;
    size_t i;

    s->comp_funcs = realloc(s->comp_funcs, (++s->n_comp_funcs) *
                            sizeof(struct comp_func));
    comp_func = &s->comp_funcs[s->n_comp_funcs - 1];

    comp_func->func         = func;
    comp_func->func_data    = func_data;
    comp_func->comps        = comps;
}

uint64_t scene_alloc_entity(struct scene *s, comp_bits_type comp_ids)
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

void scene_tick(struct scene *s)
{
    int fid, eid; 
    comp_bits_type comps;
    comp_func_type func;
    void *data;

    for (fid = 0; fid < s->n_comp_funcs; ++fid) {
        comps = s->comp_funcs[fid].comps;
        func = s->comp_funcs[fid].func;
        data = s->comp_funcs[fid].func_data;
        for (eid = 0; eid < s->n_entities; ++eid)
            if ((comps & s->entity_comp_map[fid]) == comps)
                func(s, eid, data);
    }
}

void scene_cleanup(struct scene *s)
{
    int cid;

    for (cid = 0; cid < s->n_comps; ++cid)
        free(s->comps[cid].data);

    free(s->comp_funcs);
    free(s->entity_comp_map);
    free(s->comps);
}
