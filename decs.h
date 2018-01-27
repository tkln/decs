#ifndef DECS_H
#define DECS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "perf.h"

#define STR_ARR(...) ((const char * []){ __VA_ARGS__, NULL })

enum decs_system_nums {
    DECS_INVALID_SYSTEM = 0xffffffffffffffffull,
};

enum decs_comp_nums {
    DECS_INVALID_COMP   = 0xffffffffffffffffull,
};

enum decs_system_flags {
    /*
     * System function gets passed a contiguous range of eids and it's expected
     * to handle them all at once, instead of decs core calling it for each eid
     * separately. The calls to system functions are done through
     * system_func_batch.
     */
    DECS_SYS_FLAG_BATCH = 1<<0,
};

struct decs;
typedef uint64_t comp_bits_type;
typedef void (*system_func)(struct decs *decs, uint64_t eid, void *data);
typedef void (*system_func_batch)(struct decs *decs, uint64_t eid, uint64_t n,
                                  void *data);
typedef void (*system_prepare_func)(struct decs *decs, const uint64_t *comp_ids,
                                    uint64_t n_comps, void *ctx, void *aux_ctx);

struct component {
    const char *name;
    size_t size;
    size_t n_active;
    void *data;
};

struct system_reg {
    void *func;
    system_prepare_func prepare_func;
    uint32_t flags;
    const char **comps;     /* Names of components processed by the system */
    const char **icomps;    /* Mask for non-accepting components */
    const char **deps;      /* Names of systems that need to be run first */
    const char *name;       /* Name of the system */
};

struct system {
    const char *name;
    void *func;
    system_prepare_func prepare_func;
    void *aux_ctx;
    void *ctx;
    uint32_t flags;
    uint64_t *comps;
    size_t n_comps;
    comp_bits_type comp_bits;
    uint64_t *icomps;
    size_t n_icomps;
    comp_bits_type icomp_bits;
    uint64_t *deps;
    size_t n_deps;
    bool done;
    struct perf_stats perf_stats;
};

struct decs {
    struct component *comps;
    size_t n_comps;
    comp_bits_type *entity_comp_map;
    size_t n_entities;
    struct system *systems;
    size_t n_systems;
};

struct decs_ctx {
    union {
        struct {
            void *aux_ctx;
            void *comp_bases_aux[0];
        };
        void *comp_bases_noaux[0];
    };
};

void decs_init(struct decs *decs);

uint64_t decs_register_comp(struct decs *decs, const char *name, size_t size);

void *decs_get_comp_base(struct decs *decs, const char *comp_name);
void decs_system_prepare(struct decs *decs, const uint64_t *comp_ids,
                         size_t n_comps, void *ctx, void *aux_ctx);

int decs_register_system(struct decs *decs, const struct system_reg *reg,
                         void *aux_ctx, uint64_t *sid);

uint64_t decs_alloc_entity(struct decs *decs, comp_bits_type comp_ids);

static inline void *decs_get_comp(struct decs *s, uint64_t cid, uint64_t eid)
{
    struct component *comp = s->comps + cid;
    return comp->data + eid * comp->size;
}

void decs_tick(struct decs *decs);

void decs_tick_dryrun(struct decs *decs);

void decs_cleanup(struct decs *decs);
#endif
