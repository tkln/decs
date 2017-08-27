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

uint64_t decs_register_comp(struct decs *s, const char *name, size_t size)
{
    struct component *comp;

    ++s->n_comps;
    s->comps = realloc(s->comps, s->n_comps * sizeof(s->comps[0]));

    comp = &s->comps[s->n_comps - 1];

    comp->name = name;
    comp->size  = size;
    comp->data  = NULL;

    return s->n_comps - 1;
}

void *decs_get_comp_base(struct decs *decs, const char *name)
{
    size_t i;

    for (i = 0; i < decs->n_comps; ++i)
        if (!strcmp(name, decs->comps[i].name))
            return decs->comps[i].data;

    return NULL;
}

void decs_system_prepare(struct decs *decs, const char **comp_names, void *ctx_mem,
                         void *aux_ctx)
{
    size_t i;
    struct decs_ctx *ctx = ctx_mem;
    void **comp_bases;

    if (aux_ctx) {
        ctx->aux_ctx = aux_ctx;
        comp_bases = ctx->comp_bases_aux;
    } else {
        comp_bases = ctx->comp_bases_noaux;
    }

    for (i = 0; comp_names[i]; ++i)
        comp_bases[i] = decs_get_comp_base(decs, comp_names[i]);
}

static uint64_t decs_comp_list_to_bits(struct decs *decs, const char **comps)
{
    uint64_t bits = 0;
    size_t i, j;

    for (i = 0; comps[i]; ++i) {
        for (j = 0; j < decs->n_comps; ++j) {
            if (!strcmp(decs->comps[j].name, comps[i]))
                bits |= 1<<j;
        }
    }

    return bits;
}

uint64_t decs_register_system(struct decs *decs, const char *name,
                              const char **comps,
                              system_prepare_func prepare_func,
                              system_func func, void *aux_ctx,
                              const char **dep_names)
{
    struct system *system;
    size_t n_deps;
    size_t i, j;
    size_t ctx_sz = 0;

    ++decs->n_systems;
    decs->systems = realloc(decs->systems, decs->n_systems *
                                           sizeof(struct system));
    system = &decs->systems[decs->n_systems - 1];

    for (n_deps = 0; dep_names && dep_names[n_deps]; ++n_deps)
        ;

    if (n_deps)
        system->deps = malloc(sizeof(*system->deps) * n_deps);
    else
        system->deps = NULL;

    for (i = 0; i < n_deps; ++i) {
        for (j = 0; j < (decs->n_systems - 1); ++j) {
            if (!strcmp(decs->systems[j].name, dep_names[i])) {
                system->deps[i] = j;
                break;
            }
        }
        if (j == (decs->n_systems - 1))
            fprintf(stderr, "Could not find id for dep system name \"%s\"\n",
                    dep_names[i]);
    }

    for (i = 0; comps[i]; ++i)
        ctx_sz += sizeof(void *);
    if (aux_ctx)
        ctx_sz += sizeof(void *);

    system->func            = func;

    system->prepare_func    = prepare_func ?: decs_system_prepare;
    system->ctx_sz          = ctx_sz;
    system->ctx             = malloc(ctx_sz);
    system->aux_ctx         = aux_ctx;
    system->comp_names      = comps;
    system->comp_bits       = decs_comp_list_to_bits(decs, comps);
    system->n_deps          = n_deps;
    system->name            = name;

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
    comp_bits_type comps = sys->comp_bits;
    system_func func = sys->func;
    void *data = sys->ctx;
    uint64_t eid, did, i;

    if (sys->done)
        return;

    /* Run all the dependencies before the system itself */
    for (i = 0; i < sys->n_deps; ++i) {
        did = sys->deps[i];
        if (!decs->systems[did].done)
            decs_system_tick(decs, decs->systems + did);
    }

    sys->prepare_func(decs, sys->comp_names, sys->ctx, sys->aux_ctx);
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

    for (sid = 0; sid < decs->n_systems; ++sid) {
        free(decs->systems[sid].deps);
        free(decs->systems[sid].ctx);
    }

    free(decs->systems);
    free(decs->entity_comp_map);
    free(decs->comps);
}
