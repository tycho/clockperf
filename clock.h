/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#pragma once

enum {
    CPERF_NULL,
    CPERF_NONE,
    CPERF_GETTIME,
    CPERF_GTOD,
    CPERF_TSC,
    CPERF_CLOCK,
    CPERF_RUSAGE,
    CPERF_TIME,
    CPERF_MACH_TIME,
    CPERF_QUERYPERFCOUNTER,
    CPERF_GETTICKCOUNT,
    CPERF_GETTICKCOUNT64,
    CPERF_TIMEGETTIME,
    CPERF_GETSYSTIME,
    CPERF_GETSYSTIMEPRECISE,
    CPERF_UNBIASEDINTTIME,
    CPERF_UNBIASEDINTTIMEPRECISE,
    CPERF_NUM_CLOCKS
};

struct clockspec {
    uint32_t major;
    uint32_t minor;
};

extern struct clockspec ref_clock;

void clock_choose_ref(struct clockspec spec);
void clock_choose_ref_wall(void);
void clock_set_ref(struct clockspec spec);
int clock_read(struct clockspec spec, uint64_t *output);
const char *clock_name(struct clockspec spec);
int clock_resolution(const struct clockspec spec, uint64_t *output);

void cpu_clock_init(void);
void cpu_clock_calibrate(void);

#if    defined(TARGET_CPU_X86) \
    || defined(TARGET_CPU_X86_64) \
    || defined(TARGET_CPU_PPC) \
    || (defined(TARGET_CPU_ARM) && TARGET_CPU_BITS == 64)
#  define HAVE_CPU_CLOCK
#endif

#if 0
/* Allow clocks which represent CPU time used by the thread/process? */
#define ALLOW_RUSAGE_CLOCKS
#endif

#if 0
/* Allow clocks which have worse resolution than 1ms? */
#define ALLOW_LOWRES_CLOCKS
#endif

/* vim: set ts=4 sts=4 sw=4 et: */
