#ifndef PERF_H
#define PERF_H

struct perf_stats {
    long long cpu_cycles;
    long long cache_refs;
    long long cache_misses;
    long long branch_instrs;
    long long branch_misses;
};

int perf_measurement_init(void);
int perf_measurement_start(void);
int perf_measurement_end(struct perf_stats *stats);

#endif
