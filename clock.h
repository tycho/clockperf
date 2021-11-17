/*
 * clockperf
 *
 * Copyright (c) 2016-2021, Steven Noonan <steven@uplinklabs.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#pragma once

enum {
    CPERF_NONE,
    CPERF_GETTIME,
    CPERF_GTOD,
    CPERF_TSC,
    CPERF_CLOCK,
    CPERF_RUSAGE,
    CPERF_FTIME,
    CPERF_TIME,
    CPERF_MACH_TIME,
    CPERF_QUERYPERFCOUNTER,
    CPERF_GETTICKCOUNT,
    CPERF_GETTICKCOUNT64,
    CPERF_TIMEGETTIME,
    CPERF_GETSYSTIME,
    CPERF_GETSYSTIMEPRECISE,
    CPERF_UNBIASEDINTTIME,
    CPERF_NUM_CLOCKS
};

struct clockspec {
    uint32_t major;
    uint32_t minor;
};

extern struct clockspec tsc_ref_clock;
extern struct clockspec ref_clock;

void clock_choose_ref(struct clockspec spec);
void clock_choose_ref_wall(void);
int clock_read(struct clockspec spec, uint64_t *output);
const char *clock_name(struct clockspec spec);
int clock_resolution(const struct clockspec spec, uint64_t *output);

void cpu_clock_init(void);
void cpu_clock_calibrate(void);

typedef struct _clock_pair_t {
    struct clockspec primary;
    struct clockspec *ref;
} clock_pair_t;

#if    defined(TARGET_CPU_X86) \
    || defined(TARGET_CPU_X86_64) \
    || defined(TARGET_CPU_PPC) \
    || (defined(TARGET_CPU_ARM) && TARGET_CPU_BITS == 64)
#  define HAVE_CPU_CLOCK
#endif
#if defined(TARGET_CPU_ARM) && TARGET_CPU_BITS == 64
#  define HAVE_KNOWN_TSC_FREQUENCY
#endif

/* vim: set ts=4 sts=4 sw=4 et: */
