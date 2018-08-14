#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "decs.h"
#include "sb.h"

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

    comp = sb_alloc_last(s->comps);

    comp->name      = name;
    comp->size      = size;
    comp->n_active  = 0;
    comp->data      = NULL;

    return sb_size(s->comps) - 1;
}

void *decs_get_comp_base(struct decs *decs, const char *name)
{
    size_t i;

    for (i = 0; i < sb_size(decs->comps); ++i)
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

    if (!comps)
        return 0;

    for (i = 0; comps[i]; ++i) {
        for (j = 0; j < sb_size(decs->comps); ++j) {
            if (!strcmp(decs->comps[j].name, comps[i]))
                bits |= 1<<j;
        }
    }

    return bits;
}

static size_t str_arr_len(const char **arr)
{
    size_t i;

    if (!arr)
        return 0;

    for (i = 0; arr[i]; ++i)
        ;

    return i;
}

static uint64_t decs_lookup_system_id(const struct decs *decs, const char *name)
{
    uint64_t j;

    for (j = 0; j < sb_size(decs->systems); ++j)
        if (!strcmp(decs->systems[j].name, name))
            return j;

    return DECS_INVALID_SYSTEM;
}

static uint64_t decs_lookup_comp_id(const struct decs *decs, const char *name)
{
    uint64_t j;

    for (j = 0; j < sb_size(decs->comps); ++j)
        if (!strcmp(decs->comps[j].name, name))
            return j;

    return DECS_INVALID_COMP;
}

int decs_register_system(struct decs *decs, const struct system_reg *reg,
                         void *aux_ctx, uint64_t *sid)
{
    struct system *system;
    size_t n_comps = str_arr_len(reg->comps);
    size_t n_icomps = str_arr_len(reg->icomps);
    size_t ctx_sz = 0;

    system = sb_alloc_last(decs->systems);

    ctx_sz = n_comps * sizeof(void *);
    if (aux_ctx)
        ctx_sz += sizeof(void *);

    *system = (struct system) {
        .comps          = malloc(sizeof(*system->comps) * n_comps),
        .icomps         = malloc(sizeof(*system->icomps) * n_icomps),
        .func           = reg->func,
        .prepare_func   = reg->prepare_func ?: decs_system_prepare,
        .ctx            = malloc(ctx_sz),
        .aux_ctx        = aux_ctx,
        .flags          = reg->flags,
        .comp_bits      = decs_comp_list_to_bits(decs, reg->comps),
        .icomp_bits     = decs_comp_list_to_bits(decs, reg->icomps),
        .n_comps        = n_comps,
        .n_icomps       = n_icomps,
        .name           = reg->name,
        .reg            = reg,
    };

    if (sid)
        *sid = sb_size(decs->systems) - 1;

    decs->prepared = false;

    return 0;
}

uint64_t decs_alloc_entity(struct decs *decs, comp_bits_type comp_ids)
{
    int cid;
    struct component *comp;

    sb_push(decs->entity_comp_map, comp_ids);

    for (cid = 0; cid < sb_size(decs->comps); ++cid) {
        comp = decs->comps + cid;
        comp->data = realloc(comp->data,
                             comp->size * sb_size(decs->entity_comp_map));
        if (comp_ids & (1<<cid))
            ++comp->n_active;
    }

    return sb_size(decs->entity_comp_map) - 1;
}

static void decs_system_tick(struct decs *decs, struct system *sys)
{
    comp_bits_type comps = sys->comp_bits;
    comp_bits_type icomps = sys->icomp_bits;
    system_func func = sys->func;
    system_func_batch func_batch = sys->func;
    void *data = sys->ctx;
    uint64_t eid, did, i, n;

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
    /* On average each branch hint saves a cycles per entity on -O2 */
    if (__builtin_expect(sys->flags, 0)) {
        if (sys->flags & DECS_SYS_FLAG_BATCH) {
            assert(!icomps); /* TODO */
            for (n = 0, i = 0; i < sb_size(decs->entity_comp_map); ++i) {
                if ((comps & decs->entity_comp_map[i]) == comps) {
                    if (!n) /* Start of a new range */
                        eid = i;
                    ++n;
                } else {
                    if (n) { /* End of a range */
                        func_batch(decs, eid, n, data);
                        n = 0;
                    }
                }
            }
            if (n)
                func_batch(decs, eid, n, data);
        }
    } else {
        if (__builtin_expect(icomps, 0)) {
            for (eid = 0; eid < sb_size(decs->entity_comp_map); ++eid) {
                if ((icomps & decs->entity_comp_map[eid]) == icomps)
                    continue;
                if ((comps & decs->entity_comp_map[eid]) == comps)
                    func(decs, eid, data);
            }
        } else {
            for (eid = 0; eid < sb_size(decs->entity_comp_map); ++eid)
                if ((comps & decs->entity_comp_map[eid]) == comps)
                    func(decs, eid, data);
        }
    }
    perf_measurement_end(&sys->perf_stats);

    sys->done = true;
}

int decs_systems_comp_dep_prepare(struct decs *decs)
{
    const struct system_reg *reg;
    struct system *post_dep;
    struct system *sys;
    uint64_t dep, comp;
    uint64_t i, j;
    uint64_t n_post_deps;

    for (i = 0; i < sb_size(decs->systems); ++i) {
        sys = decs->systems + i;
        reg = sys->reg;

        sys->n_deps = str_arr_len(reg->pre_deps);
        sys->deps = malloc(sizeof(*sys->deps) * sys->n_deps);

        for (j = 0; j < sys->n_deps; ++j) {
            dep = decs_lookup_system_id(decs, reg->pre_deps[j]);
            if (dep == DECS_INVALID_SYSTEM) {
                fprintf(stderr, "Could not find id for dep system name \"%s\"\n",
                        reg->pre_deps[j]);
                return -1;
            }
            sys->deps[j] = dep;
        }

        for (j = 0; j < sys->n_comps; ++j) {
            comp = decs_lookup_comp_id(decs, reg->comps[j]);
            if (comp == DECS_INVALID_COMP) {
                fprintf(stderr, "Could not find id for comp name \"%s\"\n",
                        reg->comps[j]);
                return -1;
            }
            sys->comps[j] = comp;
        }

        for (j = 0; j < sys->n_icomps; ++j) {
            comp = decs_lookup_comp_id(decs, reg->icomps[j]);
            if (comp == DECS_INVALID_COMP) {
                fprintf(stderr, "Could not find id for icomp name \"%s\"\n",
                        reg->comps[j]);
                return -1;
            }
            sys->icomps[j] = comp;
        }
    }

    for (i = 0; i < sb_size(decs->systems); ++i) {
        sys = decs->systems + i;
        reg = sys->reg;
        n_post_deps = str_arr_len(reg->post_deps);

        for (j = 0; j < n_post_deps; ++j) {
            /*
             * Post-deps are handled by adding the system with a given post-dep
             * to the pre-deps of the post-dep.
             */
            dep = decs_lookup_system_id(decs, reg->post_deps[j]);
            post_dep = decs->systems + dep;

            ++post_dep->n_deps;
            post_dep->deps = realloc(post_dep->deps, sizeof(*post_dep->deps) *
                                                     post_dep->n_deps);
            post_dep->deps[post_dep->n_deps - 1] = i;
        }
    }

    decs->prepared = true;

    return 0;
}

void decs_tick(struct decs *decs)
{
    uint64_t sid;

    if (!decs->prepared)
        decs_systems_comp_dep_prepare(decs);

    for (sid = 0; sid < sb_size(decs->systems); ++sid)
        decs->systems[sid].done = false;

    for (sid = 0; sid < sb_size(decs->systems); ++sid)
        decs_system_tick(decs, decs->systems + sid);
}


static void decs_system_tick_dryrun(struct decs *decs, struct system *sys)
{
    uint64_t did, i;

    if (sys->done)
        return;

    /* Run all the dependencies before the system itself */
    for (i = 0; i < sys->n_deps; ++i) {
        did = sys->deps[i];
        if (!decs->systems[did].done)
            decs_system_tick_dryrun(decs, decs->systems + did);
    }

    printf("%s  (deps:", sys->name);
    for (i = 0; i < sys->n_deps; ++i)
        printf(" %s,", decs->systems[sys->deps[i]].name);
    printf(")\n");

    sys->done = true;
}

void decs_tick_dryrun(struct decs *decs)
{
    uint64_t sid;

    if (!decs->prepared)
        decs_systems_comp_dep_prepare(decs);

    printf("System execution order:\n");
    for (sid = 0; sid < sb_size(decs->systems); ++sid)
        decs->systems[sid].done = false;

    for (sid = 0; sid < sb_size(decs->systems); ++sid)
        decs_system_tick_dryrun(decs, decs->systems + sid);
}

void decs_cleanup(struct decs *decs)
{
    int cid;
    uint64_t sid;

    for (cid = 0; cid < sb_size(decs->comps); ++cid)
        free(decs->comps[cid].data);

    for (sid = 0; sid < sb_size(decs->systems); ++sid) {
        free(decs->systems[sid].deps);
        free(decs->systems[sid].ctx);
    }

    sb_free(decs->systems);
    sb_free(decs->comps);
    sb_free(decs->entity_comp_map);
}
