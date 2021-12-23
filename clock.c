/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2021, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"
#include "clock.h"
#include "util.h"
#include "winapi.h"

#include <assert.h>
#include <limits.h>

#ifdef TARGET_OS_LINUX
#include <sched.h>
#endif

struct clockspec tsc_ref_clock = { CPERF_NONE, 0 };
struct clockspec ref_clock = { CPERF_NONE, 0 };

/*
 * Choices for ref_clock in order of preference, from best to worst.
 */
static struct clockspec ref_clock_choices[] = {
#ifdef TARGET_OS_WINDOWS
    {CPERF_GETSYSTIMEPRECISE, 0},
    {CPERF_QUERYPERFCOUNTER, 0},
#endif
#ifdef HAVE_CLOCK_GETTIME
#ifdef CLOCK_MONOTONIC_RAW
    {CPERF_GETTIME, CLOCK_MONOTONIC_RAW},
#endif
#ifdef CLOCK_MONOTONIC
    {CPERF_GETTIME, CLOCK_MONOTONIC},
#endif
    {CPERF_GETTIME, CLOCK_REALTIME},
#endif
#ifdef HAVE_GETTIMEOFDAY
    {CPERF_GTOD, 0},
#endif
    {CPERF_NONE, 0}
};

static struct clockspec wall_clock_choices[] = {
#ifdef TARGET_OS_WINDOWS
#if _WIN32_WINNT >= 0x0602
    {CPERF_GETSYSTIMEPRECISE, 0},
#endif
#endif
#ifdef HAVE_CLOCK_GETTIME
    {CPERF_GETTIME, CLOCK_REALTIME},
#endif
#ifdef HAVE_GETTIMEOFDAY
    {CPERF_GTOD, 0},
#endif
    {CPERF_NONE, 0}
};

static int choose_ref_clock(struct clockspec *ref, struct clockspec *choices, struct clockspec for_clock)
{
    int i;
    uint64_t ctr_last = 0, ctr_now = 0;
    struct clockspec *spec = choices;
#ifdef _DEBUG
    fprintf(stderr, "trying to choose reference clock for %s\n", clock_name(for_clock));
#endif
    while (spec && spec->major != CPERF_NONE)
    {
        if (memcmp(spec, &for_clock, sizeof(struct clockspec)) == 0) {
            /* You can't use the same clock as a reference for itself. */
            goto fail;
        }
#ifdef _DEBUG
        fprintf(stderr, "trying %s\n", clock_name(*spec));
#endif

        if (clock_read(*spec, &ctr_last) || !ctr_last) {
            /* This clock failed. Try the next one. */
#ifdef _DEBUG
            fprintf(stderr, "  failed initial read\n");
#endif
            goto fail;
        }

        /* Quick sanity check to ensure clock is advancing monotonically. */
        for (i = 0; i < 100; i++) {
            if (clock_read(*spec, &ctr_now) || !ctr_now) {
                /* Clock failed? If this didn't happen on the first attempt,
                 * then it really shouldn't happen at all. But checking this
                 * anyway.
                 */
#ifdef _DEBUG
                fprintf(stderr, "  failed read %d\n", i + 1);
#endif
                goto fail;
            }
            if (ctr_now < ctr_last) {
                /* Not monotonic, try the next clock. */
#ifdef _DEBUG
                fprintf(stderr, "  not monotonic at read %d (%" PRIu64 " < %" PRIu64 ")\n", i + 1, ctr_now, ctr_last);
#endif
                goto fail;
            }
            ctr_last = ctr_now;
        }

        /* This clock seems to be working and sane. */
        *ref = *spec;
#ifdef _DEBUG
        fprintf(stderr, "chose %s as reference clock\n", clock_name(*ref));
#endif
        return 0;
fail:
        spec++;
    }
    fprintf(stderr, "Could not choose a reference clock for %s! Aborting.\n",
            clock_name(for_clock));
    abort();
    return 1;
}

static uint32_t cpu_clock_known_freq;

#if defined(TARGET_CPU_X86) || defined(TARGET_CPU_X86_64)
#ifdef TARGET_COMPILER_MSVC
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

static INLINE uint64_t cpu_clock_read(void)
{
    uint32_t aux;
    return __rdtscp(&aux);
}
#elif defined(TARGET_CPU_ARM) && TARGET_CPU_BITS == 64

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_ReadStatusReg)
#ifndef ARM64_CNTVCT
#define ARM64_CNTVCT ARM64_SYSREG(3, 3, 14, 0, 2)
#endif
#ifndef ARM64_CNTFRQ
#define ARM64_CNTFRQ ARM64_SYSREG(3, 3, 14, 0, 0)
#endif
#endif

static const char *cpu_clock_name(void)
{
    return "cntvct";
}

void cpu_clock_init(void)
{
    uint64_t cntfrq_el0;
#ifdef _MSC_VER
    cntfrq_el0 = _ReadStatusReg(ARM64_CNTFRQ);
#else
    unsigned long long cval;
    asm volatile("mrs %0, cntfrq_el0"
                 : "=r"(cval));
    cntfrq_el0 = cval;
#endif
    if (cntfrq_el0)
        cpu_clock_known_freq = cntfrq_el0 / 1000;
}

static INLINE uint64_t cpu_clock_read()
{
#ifdef _MSC_VER
    return _ReadStatusReg(ARM64_CNTVCT);
#else
    unsigned long long cval;
    asm volatile("mrs %0, cntvct_el0"
                 : "=r"(cval));
    return cval;
#endif
}

#elif defined(TARGET_CPU_PPC)

/* Indirect stringification.  Doing two levels allows the parameter to be
 * a macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */
#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)

#define mfspr(rn) ({ \
        uint32_t rval; \
        asm volatile("mfspr %0," __stringify(rn) \
                    : "=r" (rval)); rval; \
    })

#define SPRN_TBRL       0x10C   /* Time Base Register Lower */
#define SPRN_TBRU       0x10D   /* Time Base Register Upper */
#define SPRN_ATBL       0x20E   /* Alternate Time Base Lower */
#define SPRN_ATBU       0x20F   /* Alternate Time Base Upper */

static int supports_atb = 0;

static INLINE uint64_t cpu_clock_read(void)
{
    uint64_t result = 0;
    uint32_t upper, lower, tmp;
    do {
        if (supports_atb) {
            upper = mfspr(SPRN_ATBU);
            lower = mfspr(SPRN_ATBL);
            tmp = mfspr(SPRN_ATBU);
        } else {
            upper = mfspr(SPRN_TBRU);
            lower = mfspr(SPRN_TBRL);
            tmp = mfspr(SPRN_TBRU);
        }
    } while (tmp != upper);
    result = upper;
    result = result << 32;
    result = result | lower;
    return result;
}

static void atb_child(void)
{
    supports_atb = 1;
    cpu_clock_read();
    _exit(0);
}

static const char *cpu_clock_name(void)
{
    return supports_atb ? "atb" : "tbr";
}

void cpu_clock_init(void)
{
    pid_t pid;

    pid = fork();
    if (!pid)
        atb_child();
    else if (pid != -1) {
        int status;

        pid = wait(&status);
        if (pid != -1 && WIFEXITED(status))
            supports_atb = 1;
    }
}
#else
void cpu_clock_init(void)
{
}
#endif

/* Common for x86 32/64-bit */
#if defined(TARGET_CPU_X86) || defined(TARGET_CPU_X86_64)
static const char *cpu_clock_name(void)
{
    return "tsc";
}

void cpu_clock_init(void)
{
    uint32_t max_leaf;
    uint32_t regs[4];

    /* Check maximum supported base leaf */
    memset(regs, 0, sizeof(regs));
    cpuid_read(regs);
    max_leaf = regs[0];

    if (max_leaf >= 0x15) {
        uint32_t numer, denom, crystal_khz;

        /* Read TSC information leaf */
        memset(regs, 0, sizeof(regs));
        regs[0] = 0x15;
        cpuid_read(regs);

        denom = regs[0];
        numer = regs[1];
        crystal_khz = regs[2];

        if (denom && numer) {
            if (!crystal_khz && max_leaf >= 0x16) {
                /* Skylake and Kaby Lake don't set a valid ecx value in leaf
                 * 0x15, but we can infer it from leaf 0x16 and the ratio in
                 * leaf 0x15
                 */
                memset(regs, 0, sizeof(regs));
                regs[0] = 0x16;
                cpuid_read(regs);

                crystal_khz = regs[0] * 1000 * denom / numer;
            }

            if (crystal_khz) {
                cpu_clock_known_freq = crystal_khz * numer / denom;
            }
        }
    }

#ifdef TARGET_OS_LINUX
    if (!cpu_clock_known_freq) {
        FILE *fd = fopen("/sys/devices/system/cpu/tsc_khz", "rt");
        if (fd) {
            if (fscanf(fd, "%u", &cpu_clock_known_freq) != 1)
                cpu_clock_known_freq = 0;
            fclose(fd);
        }
    }
#endif
}
#endif

#ifdef HAVE_CPU_CLOCK

static unsigned long long cycles_per_msec;
static unsigned long long cycles_start;
static unsigned long long clock_mult;
static unsigned long long max_cycles_mask;
static unsigned long long nsecs_for_max_cycles;
static unsigned int clock_shift;
static unsigned int max_cycles_shift;
#define MAX_CLOCK_SEC 60*60
#define NR_TIME_ITERS 50

static unsigned long get_cycles_per_msec(void)
{
    uint64_t wc_s, wc_e;
    uint64_t c_s, c_e;
    uint64_t elapsed;

    /* Early out if we have an already-known CPU frequency and we don't need to
     * infer it.
     */
    if (cpu_clock_known_freq)
        return cpu_clock_known_freq;

    if (clock_read(tsc_ref_clock, &wc_s)) {
        fprintf(stderr, "Reference clock '%s' died while measuring TSC frequency\n",
                clock_name(tsc_ref_clock));
        abort();
    }
    c_s = cpu_clock_read();
    do {
        if (clock_read(tsc_ref_clock, &wc_e)) {
            fprintf(stderr, "Reference clock '%s' died while measuring TSC frequency\n",
                    clock_name(tsc_ref_clock));
            abort();
        }
        c_e = cpu_clock_read();
        elapsed = wc_e - wc_s;
        if (elapsed >= 1280000ULL) {
            break;
        }
    } while (1);

    return (c_e - c_s) * 1000000 / elapsed;
}

#ifndef min
#define min(x,y) ({ \
    __typeof__(x) _x = (x); \
    __typeof__(y) _y = (y); \
    (void) (&_x == &_y);        \
    _x < _y ? _x : _y; })
#endif

#ifndef max
#define max(x,y) ({ \
    __typeof__(x) _x = (x); \
    __typeof__(y) _y = (y); \
    (void) (&_x == &_y);        \
    _x > _y ? _x : _y; })
#endif

static void cpu_clock_init_ref(void)
{
    struct clockspec for_clock = {CPERF_TSC, 0};
    choose_ref_clock(&tsc_ref_clock, ref_clock_choices, for_clock);
}

void cpu_clock_calibrate(void)
{
    double delta, mean, S;
    uint64_t minc, maxc, avg, cycles[NR_TIME_ITERS];
    int i, samples, sft = 0;
    unsigned long long tmp, max_ticks, max_mult;

#ifdef TARGET_OS_LINUX
    /* Allow the kernel to reschedule us so we get a full time slice */
    sched_yield();
#endif

    cpu_clock_init_ref();

    cycles[0] = get_cycles_per_msec();
    S = delta = mean = 0.0;
    for (i = 0; i < NR_TIME_ITERS; i++) {
        cycles[i] = get_cycles_per_msec();
        delta = cycles[i] - mean;
        if (delta) {
            mean += delta / (i + 1.0);
            S += delta * (cycles[i] - mean);
        }
    }

    /*
     * The most common platform clock breakage is returning zero
     * indefinitely. Check for that and return failure.
     */
    if (!cycles[0] && !cycles[NR_TIME_ITERS - 1]) {
        fprintf(stderr, "CPU clock calibration failed!\n");
        abort();
    }

    S = sqrt(S / (NR_TIME_ITERS - 1.0));

    minc = ~0ULL;
    maxc = samples = avg = 0;
    for (i = 0; i < NR_TIME_ITERS; i++) {
        double this = cycles[i];

        minc = min(cycles[i], minc);
        maxc = max(cycles[i], maxc);

        if ((fmax(this, mean) - fmin(this, mean)) > S)
            continue;
        samples++;
        avg += this;
    }

    S /= (double) NR_TIME_ITERS;

    avg /= samples;
    cycles_per_msec = avg;

    max_ticks = MAX_CLOCK_SEC * cycles_per_msec * 1000ULL;
    max_mult = ULLONG_MAX / max_ticks;

    /*
     * Find the largest shift count that will produce
     * a multiplier that does not exceed max_mult
     */
    tmp = max_mult * cycles_per_msec / 1000000;
    while (tmp > 1) {
            tmp >>= 1;
            sft++;
    }

    clock_shift = sft;
    clock_mult = (1ULL << sft) * 1000000 / cycles_per_msec;

    /*
     * Find the greatest power of 2 clock ticks that is less than the
     * ticks in MAX_CLOCK_SEC_2STAGE
     */
    max_cycles_shift = max_cycles_mask = 0;
    tmp = MAX_CLOCK_SEC * 1000ULL * cycles_per_msec;
    while (tmp > 1) {
        tmp >>= 1;
        max_cycles_shift++;
    }
    /*
     * if use use (1ULL << max_cycles_shift) * 1000 / cycles_per_msec
     * here we will have a discontinuity every
     * (1ULL << max_cycles_shift) cycles
     */
    nsecs_for_max_cycles = ((1ULL << max_cycles_shift) * clock_mult)
                    >> clock_shift;

    /* Use a bitmask to calculate ticks % (1ULL << max_cycles_shift) */
    for (tmp = 0; tmp < max_cycles_shift; tmp++)
        max_cycles_mask |= 1ULL << tmp;

    cycles_start = cpu_clock_read();
}
#else
void cpu_clock_calibrate(void)
{
}
#endif

/* Read a clock, in nanoseconds. */
int clock_read(struct clockspec spec, uint64_t *output)
{
    static uint64_t CLOCK_RATIO = 1000000000ULL / CLOCKS_PER_SEC;
#ifdef TARGET_OS_WINDOWS
    static LARGE_INTEGER qpc_freq;
#endif
#ifndef TARGET_COMPILER_MSVC
    union {
        struct timespec ts;
        struct timeval tv;
    } u;
#endif
    switch(spec.major) {
#ifdef HAVE_CLOCK_GETTIME
        case CPERF_GETTIME:
            if (clock_gettime(spec.minor, &u.ts) != 0)
                return 1;
            *output = (u.ts.tv_sec * 1000000000ULL) + u.ts.tv_nsec;
            break;
#endif
#ifdef HAVE_GETTIMEOFDAY
        case CPERF_GTOD:
            gettimeofday(&u.tv, NULL);
            *output = (u.tv.tv_sec * 1000000000ULL) + (u.tv.tv_usec * 1000ULL);
            break;
#endif
#ifdef HAVE_CPU_CLOCK
        case CPERF_TSC:
            {
                uint64_t nsecs, t, multiples;
                t = cpu_clock_read();
                multiples = t >> max_cycles_shift;
                nsecs = multiples * nsecs_for_max_cycles;
                nsecs += ((t & max_cycles_mask) * clock_mult) >> clock_shift;
                *output = nsecs;
            }
            break;
#endif
        case CPERF_CLOCK:
            *output = clock() * CLOCK_RATIO;
            if (*output == 0)
                return 1;
            break;
#ifdef HAVE_GETRUSAGE
        case CPERF_RUSAGE:
            {
                struct rusage usage;
                if (getrusage(RUSAGE_SELF, &usage))
                    return 1;
                *output = (usage.ru_utime.tv_sec * 1000000000ULL)
                    + (usage.ru_utime.tv_usec * 1000ULL);
                *output += (usage.ru_stime.tv_sec * 1000000000ULL)
                    + (usage.ru_stime.tv_usec * 1000ULL);
            }
            break;
#endif
#ifdef HAVE_TIME
        case CPERF_TIME:
            {
                *output = (time(NULL) * 1000000000ULL);
            }
            break;
#endif
#ifdef HAVE_MACH_TIME
        case CPERF_MACH_TIME:
            {
                static mach_timebase_info_data_t tb = {0, 0};
                if (!tb.numer)
                    mach_timebase_info(&tb);
                *output = mach_absolute_time() * tb.numer / tb.denom;
            }
            break;
#endif
#ifdef TARGET_OS_WINDOWS
        case CPERF_QUERYPERFCOUNTER:
            {
                LARGE_INTEGER qpc;
                if (!qpc_freq.QuadPart) {
                    if (!QueryPerformanceFrequency(&qpc_freq))
                        return 1;
                }
                if (!QueryPerformanceCounter(&qpc))
                    return 1;
                *output = qpc.QuadPart * 1000000000ULL / qpc_freq.QuadPart;
            }
            break;
        case CPERF_GETTICKCOUNT:
            *output = GetTickCount() * 1000000ULL;
            break;
        case CPERF_GETTICKCOUNT64:
            *output = GetTickCount64() * 1000000ULL;
            break;
        case CPERF_TIMEGETTIME:
            *output = timeGetTime() * 1000000ULL;
            break;
        case CPERF_GETSYSTIME:
            {
                FILETIME ft;
                GetSystemTimeAsFileTime(&ft);
                *output = ((uint64_t)ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32)) * 100ULL;
            }
            break;
        case CPERF_GETSYSTIMEPRECISE:
            {
                FILETIME ft;
                if (!pGetSystemTimePreciseAsFileTime)
                    return 1;
                pGetSystemTimePreciseAsFileTime(&ft);
                *output = ((uint64_t)ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32)) * 100ULL;
            }
            break;
        case CPERF_UNBIASEDINTTIME:
            {
                ULONGLONG t;
                if (!pQueryUnbiasedInterruptTime || !pQueryUnbiasedInterruptTime(&t))
                    return 1;
                *output = t * 100ULL;
            }
            break;
        case CPERF_UNBIASEDINTTIMEPRECISE:
            {
                ULONGLONG t;
                if (!pQueryUnbiasedInterruptTimePrecise)
                    return 1;
                pQueryUnbiasedInterruptTime(&t);
                *output = t * 100ULL;
            }
            break;
#endif
        default:
            return 1;
    }
    return 0;
}

const char *clock_name(struct clockspec spec)
{
    switch(spec.major) {
    case CPERF_NONE:
        return "null";

    case CPERF_GETTIME:
        switch(spec.minor) {
#ifdef CLOCK_REALTIME
        case CLOCK_REALTIME:
            return "realtime";
#endif
#ifdef CLOCK_REALTIME_COARSE
        case CLOCK_REALTIME_COARSE:
            return "realtime_crs";
#endif
#ifdef CLOCK_MONOTONIC
        case CLOCK_MONOTONIC:
            return "monotonic";
#endif
#ifdef CLOCK_MONOTONIC_COARSE
        case CLOCK_MONOTONIC_COARSE:
            return "monotonic_crs";
#endif
#ifdef CLOCK_MONOTONIC_RAW
        case CLOCK_MONOTONIC_RAW:
            return "monotonic_raw";
#endif
#ifdef CLOCK_MONOTONIC_RAW_APPROX // OS X
        case CLOCK_MONOTONIC_RAW_APPROX:
            return "monotonic_raw_approx";
#endif
#ifdef CLOCK_BOOTTIME
        case CLOCK_BOOTTIME:
            return "boottime";
#endif
#ifdef CLOCK_UPTIME_RAW // OS X
        case CLOCK_UPTIME_RAW:
            return "uptime_raw";
#endif
#ifdef CLOCK_UPTIME_RAW_APPROX // OS X
        case CLOCK_UPTIME_RAW_APPROX:
            return "uptime_raw_approx";
#endif
#ifdef CLOCK_PROCESS_CPUTIME_ID
        case CLOCK_PROCESS_CPUTIME_ID:
            return "process";
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
        case CLOCK_THREAD_CPUTIME_ID:
            return "thread";
#endif
        default:
            abort();
        }
#ifdef HAVE_GETTIMEOFDAY
    case CPERF_GTOD:
        return "gettimeofday";
#endif
#ifdef HAVE_CPU_CLOCK
    case CPERF_TSC:
        return cpu_clock_name();
#endif
    case CPERF_CLOCK:
        return "clock";
#ifdef HAVE_GETRUSAGE
    case CPERF_RUSAGE:
        return "getrusage";
#endif
#ifdef HAVE_TIME
    case CPERF_TIME:
        return "time";
#endif
#ifdef HAVE_MACH_TIME
    case CPERF_MACH_TIME:
        return "mach_time";
#endif
#ifdef TARGET_OS_WINDOWS
    case CPERF_QUERYPERFCOUNTER:
        return "PerfCounter";
    case CPERF_GETTICKCOUNT:
        return "GetTickCount";
    case CPERF_GETTICKCOUNT64:
        return "GetTickCount64";
    case CPERF_TIMEGETTIME:
        return "timeGetTime";
    case CPERF_GETSYSTIME:
        return "SysTimeAsFile";
    case CPERF_GETSYSTIMEPRECISE:
        return "SysTimePrecAsFile";
    case CPERF_UNBIASEDINTTIME:
        return "UnbiasIntTime";
    case CPERF_UNBIASEDINTTIMEPRECISE:
        return "UnbiasIntTimePrec";
#endif
    default:
        return "unknown";
    }
}

void clock_choose_ref(struct clockspec spec)
{
    choose_ref_clock(&ref_clock, ref_clock_choices, spec);
}

void clock_choose_ref_wall(void)
{
    struct clockspec nullclock = {0, 0};
    choose_ref_clock(&ref_clock, wall_clock_choices, nullclock);
}

void clock_set_ref(struct clockspec spec)
{
    ref_clock = spec;
}

/*
 * Attempts to get the clock resolution for the specified clock. Resolution is
 * returned in Hz.
 *
 * Returns zero on success, nonzero on failure.
 */
int clock_resolution(const struct clockspec spec, uint64_t *output)
{
    uint64_t hz;

    switch(spec.major) {
        case CPERF_NONE:
            return 1;
            break;
#ifdef HAVE_CLOCK_GETTIME
        case CPERF_GETTIME:
            {
                struct timespec ts;
                if (clock_getres(spec.minor, &ts) != 0)
                    return 1;
                hz = 1000000000ULL / ((ts.tv_sec * 1000000000ULL) + ts.tv_nsec);
            }
            break;
#endif
#ifdef HAVE_GETTIMEOFDAY
        case CPERF_GTOD:
            hz = 1000000ULL;
            break;
#endif
#ifdef HAVE_CPU_CLOCK
        case CPERF_TSC:
            hz = cycles_per_msec * 1000ULL;
            break;
#endif
        case CPERF_CLOCK:
            hz = CLOCKS_PER_SEC;
            break;
#ifdef HAVE_GETRUSAGE
        case CPERF_RUSAGE:
            /* This clock advances based on userspace CPU utilization, but the
             * rate at which it gets updated is implementation-dependent and
             * there is no clearly defined way to determine that update
             * frequency. Best to just error out and say we can't discover it.
             */
            return 1;
            break;
#endif
#ifdef HAVE_TIME
        case CPERF_TIME:
            /* 1 second granularity due to API design */
            hz = 1ULL;
            break;
#endif
#ifdef HAVE_MACH_TIME
        case CPERF_MACH_TIME:
            {
                mach_timebase_info_data_t tb;
                mach_timebase_info(&tb);
                hz = 1000000000ULL / (tb.numer / tb.denom);
            }
            break;
#endif
#ifdef TARGET_OS_WINDOWS
        case CPERF_QUERYPERFCOUNTER:
            {
                LARGE_INTEGER freq;
                if (!QueryPerformanceFrequency(&freq))
                    return 1;
                hz = freq.QuadPart;
            }
            break;
        case CPERF_GETTICKCOUNT:
        case CPERF_GETTICKCOUNT64:
        case CPERF_TIMEGETTIME:
            hz = 1000ULL;
            break;
        case CPERF_GETSYSTIME:
            /* NT timer ticks (100ns) */
            hz = 10000000ULL;
            break;
        case CPERF_GETSYSTIMEPRECISE:
            if (!pGetSystemTimePreciseAsFileTime)
                return 1;
            /* NT timer ticks (100ns) */
            hz = 10000000ULL;
            break;
        case CPERF_UNBIASEDINTTIME:
            if (!pQueryUnbiasedInterruptTime)
                return 1;
            /* NT timer ticks (100ns) */
            hz = 10000000ULL;
            break;
        case CPERF_UNBIASEDINTTIMEPRECISE:
            if (!pQueryUnbiasedInterruptTimePrecise)
                return 1;
            /* NT timer ticks (100ns) */
            hz = 10000000ULL;
            break;
#endif
        default:
            abort();
    }

    *output = hz;
    return 0;
}

/* vim: set ts=4 sts=4 sw=4 et: */
