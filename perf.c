#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <stdint.h>

#include "perf.h"


static int cpu_cycles_fd;
static int cache_refs_fd;
static int cache_misses_fd;
static int branch_instrs_fd;
static int branch_misses_fd;

static const struct event {
    uint64_t config;
    int *fd;
} events[] = {
    {
        .config = PERF_COUNT_HW_INSTRUCTIONS,
        .fd = &cpu_cycles_fd,
    }, {
        .config = PERF_COUNT_HW_CACHE_REFERENCES,
        .fd = &cache_refs_fd,
    }, {
        .config = PERF_COUNT_HW_CACHE_MISSES,
        .fd = &cache_misses_fd,
    },{
        .config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
        .fd = &branch_instrs_fd,
    }, {
        .config = PERF_COUNT_HW_BRANCH_MISSES,
        .fd = &branch_misses_fd,
    },
};

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int perf_measurement_init(void)
{
    struct perf_event_attr pe;
    size_t i;

    for (i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        memset(&pe, 0, sizeof(pe));
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(pe);
        pe.config = events[i].config;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        *events[i].fd = perf_event_open(&pe, 0,  -1, -1, 0);
        if (*events[i].fd < 0) {
            fprintf(stderr, "Error opening leader %llx\n", pe.config);
            return -1;
        }
    }

    return 0;
}

int perf_measurement_start(void)
{
    size_t i;
    int ret;

    for (i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        ret = ioctl(*events[i].fd, PERF_EVENT_IOC_RESET, 0);
        if (ret < 0)
            return ret;

        ret = ioctl(*events[i].fd, PERF_EVENT_IOC_ENABLE, 0);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int perf_measurement_end(struct perf_stats *stats)
{
    size_t i;
    int ret;

    for (i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        ret = ioctl(*events[i].fd, PERF_EVENT_IOC_DISABLE, 0);
        if (ret < 0) {
            perror("perf event ioctl failed");
            return ret;
        }
    }

    ret = read(cpu_cycles_fd, &stats->cpu_cycles, sizeof(stats->cpu_cycles));
    if (ret < 0)
        goto read_fail;
    ret = read(cache_refs_fd, &stats->cache_refs, sizeof(stats->cache_refs));
    if (ret < 0)
        goto read_fail;
    ret = read(cache_misses_fd, &stats->cache_misses, sizeof(stats->cache_misses));
    if (ret < 0)
        goto read_fail;
    ret = read(branch_instrs_fd, &stats->branch_instrs, sizeof(stats->branch_instrs));
    if (ret < 0)
        goto read_fail;
    ret = read(branch_misses_fd, &stats->branch_misses, sizeof(stats->branch_misses));
    if (ret < 0)
        goto read_fail;

    return 0;
read_fail:
    perror("Reading perf events failed");
    return ret;
}
