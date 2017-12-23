#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decs.h"

void decs_init(struct decs *decs)
{
    memset(decs, 0, sizeof(*decs));
    perf_measurement_init();
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

void decs_system_prepare(struct decs *decs, const uint64_t *comp_ids,
                         size_t n_comps, void *ctx_mem, void *aux_ctx)
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

    for (i = 0; i < n_comps; ++i)
        comp_bases[i] = decs->comps[comp_ids[i]].data;
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

int decs_register_system(struct decs *decs, const struct system_reg *reg,
                         uint64_t *sid)
{
    struct system *system;
    size_t n_deps, n_comps;
    size_t i, j;
    size_t ctx_sz = 0;

    ++decs->n_systems;
    decs->systems = realloc(decs->systems, decs->n_systems *
                                           sizeof(struct system));
    system = &decs->systems[decs->n_systems - 1];

    for (n_deps = 0; reg->deps && reg->deps[n_deps]; ++n_deps)
        ;
    for (n_comps = 0; reg->comps && reg->comps[n_comps]; ++n_comps)
        ;

    if (n_deps)
        system->deps = malloc(sizeof(*system->deps) * n_deps);
    else
        system->deps = NULL;

    if (n_comps)
        system->comps = malloc(sizeof(*system->comps) * n_comps);
    else
        system->comps = NULL;

    for (i = 0; i < n_deps; ++i) {
        for (j = 0; j < (decs->n_systems - 1); ++j) {
            if (!strcmp(decs->systems[j].name, reg->deps[i])) {
                system->deps[i] = j;
                break;
            }
        }
        if (j == (decs->n_systems - 1)) {
            fprintf(stderr, "Could not find id for dep system name \"%s\"\n",
                    reg->deps[i]);
            return -1;
        }
    }

    for (i = 0; i < n_comps; ++i) {
        for (j = 0; j < decs->n_comps; ++j) {
            if (!strcmp(decs->comps[j].name, reg->comps[i])) {
                system->comps[i] = j;
                break;
            }
        }
        if (j == decs->n_comps) {
            fprintf(stderr, "Could not find id for comp name \"%s\"\n",
                    reg->comps[i]);
            return -1;
        }
    }

    ctx_sz = n_comps * sizeof(void *);
    if (reg->aux_ctx)
        ctx_sz += sizeof(void *);

    system->func            = reg->func;

    system->prepare_func    = reg->prepare_func ?: decs_system_prepare;
    system->ctx             = malloc(ctx_sz);
    system->aux_ctx         = reg->aux_ctx;
    system->comp_bits       = decs_comp_list_to_bits(decs, reg->comps);
    system->n_comps         = n_comps;
    system->n_deps          = n_deps;
    system->name            = reg->name;

    if (sid)
        *sid = decs->n_systems - 1;

    return 0;
}

uint64_t decs_alloc_entity(struct decs *decs, comp_bits_type comp_ids)
{
    int cid;
    struct component *comp;
    size_t comp_maps_sz;

    ++decs->n_entities;
    comp_maps_sz = decs->n_entities * sizeof(*decs->entity_comp_map);
    decs->entity_comp_map = realloc(decs->entity_comp_map, comp_maps_sz);
    decs->entity_comp_map[decs->n_entities - 1] = comp_ids;

    for (cid = 0; cid < decs->n_comps; ++cid) {
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

    sys->prepare_func(decs, sys->comps, sys->n_comps, sys->ctx, sys->aux_ctx);
    perf_measurement_start();
    for (eid = 0; eid < decs->n_entities; ++eid)
        if ((comps & decs->entity_comp_map[eid]) == comps)
            func(decs, eid, data);
    perf_measurement_end(&sys->perf_stats);

    sys->done = true;
}

void decs_tick(struct decs *decs)
{
    uint64_t sid;

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
