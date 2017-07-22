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

uint64_t decs_register_system(struct decs *decs, comp_bits_type comps,
                              system_func func, void *func_data,
                              uint64_t *deps, size_t n_deps)
{
    struct system *system;
    size_t i;

    ++decs->n_systems;
    decs->systems = realloc(decs->systems, decs->n_systems *
                                           sizeof(struct system));
    system = &decs->systems[decs->n_systems - 1];

    system->func        = func;
    system->func_data   = func_data;
    system->comps       = comps;
    system->n_deps      = n_deps;
    system->deps        = malloc(sizeof(*deps) * n_deps);
    memcpy(system->deps, deps, sizeof(*deps) * n_deps);

    return decs->n_systems - 1;
}

uint64_t decs_alloc_entity(struct decs *decs, comp_bits_type comp_ids)
{
    int cid, i;
    struct component *comp;
    size_t comp_maps_sz;

    ++decs->n_entities;
    comp_maps_sz = decs->n_entities * sizeof(*decs->entity_comp_map);
    decs->entity_comp_map = realloc(decs->entity_comp_map, comp_maps_sz);
    decs->entity_comp_map[decs->n_entities - 1] = comp_ids;

    for (cid = 0, i = 0; cid < decs->n_comps; ++cid) {
        comp = decs->comps + cid;
        comp->data = realloc(comp->data, comp->size * decs->n_entities);
    }

    return decs->n_entities - 1;
}

static void decs_system_tick(struct decs *decs, struct system *sys)
{
    comp_bits_type comps = sys->comps;
    system_func func = sys->func;
    void *data = sys->func_data;
    uint64_t eid, did, i;

    if (sys->done)
        return;

    /* Run all the dependencies before the system itself */
    for (i = 0; i < sys->n_deps; ++i) {
        did = sys->deps[i];
        if (!decs->systems[did].done)
            decs_system_tick(decs, decs->systems + did);
    }

    for (eid = 0; eid < decs->n_entities; ++eid)
        if ((comps & decs->entity_comp_map[eid]) == comps)
            func(decs, eid, data);

    sys->done = true;
}

void decs_tick(struct decs *decs)
{
    uint64_t sid, eid, did;
    struct system *sys;

    for (sid = 0; sid < decs->n_systems; ++sid)
        decs->systems[sid].done = false;

    for (sid = 0; sid < decs->n_systems; ++sid)
        decs_system_tick(decs, decs->systems + sid);
}

void decs_cleanup(struct decs *decs)
{
    int cid;
    uint64_t sid;

    for (cid = 0; cid < decs->n_comps; ++cid)
        free(decs->comps[cid].data);

    for (sid = 0; sid < decs->n_systems; ++sid)
        free(decs->systems[sid].deps);

    free(decs->systems);
    free(decs->entity_comp_map);
    free(decs->comps);
}
