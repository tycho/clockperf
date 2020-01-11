/*
 * clockperf
 *
 * Copyright (c) 2016-2020, Steven Noonan <steven@uplinklabs.net>
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

#include "prefix.h"
#include "clock.h"

struct clockspec tsc_ref_clock = { CPERF_NONE, 0 };
struct clockspec ref_clock = { CPERF_NONE, 0 };

/*
 * Choices for ref_clock in order of preference, from best to worst.
 */
static struct clockspec ref_clock_choices[] = {
#ifdef HAVE_CPU_CLOCK
/* Not safe to use TSC as a reference clock, unless we can empirically verify
 * it's trustworthy. e.g. are TSCs on all cores synced?
 *
 * TODO: Add a CPU clock sanity test.
 */
#if 0
    {CPERF_TSC, 0},
#endif
#endif
#ifdef TARGET_OS_WINDOWS
#if _WIN32_WINNT >= 0x0602
    {CPERF_GETSYSTIMEPRECISE, 0},
#endif
    {CPERF_QUERYPERFCOUNTER, 0},
#endif
#ifdef HAVE_MACH_TIME
    {CPERF_MACH_TIME, 0},
#endif
#ifdef HAVE_CLOCK_GETTIME
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

static int choose_ref_clock(struct clockspec *ref, struct clockspec for_clock)
{
    int i;
    uint64_t ctr_last = 0, ctr_now = 0;
    struct clockspec *spec = ref_clock_choices;
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

#if defined(TARGET_CPU_X86)
void init_cpu_clock(void)
{
}

static INLINE uint64_t get_cpu_clock(void)
{
    uint32_t lo, hi;

#ifdef TARGET_COMPILER_MSVC
    __asm {
        rdtsc
        mov lo, eax
        mov hi, edx
    }
#else
    __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
#endif
    return ((uint64_t) hi << 32ULL) | lo;
}
#elif defined(TARGET_CPU_X86_64)
void init_cpu_clock(void)
{
}

static INLINE uint64_t get_cpu_clock(void)
{
#ifdef TARGET_COMPILER_MSVC
    return __rdtsc();
#else
    uint32_t lo, hi;

    __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t) hi << 32ULL) | lo;
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

static INLINE uint64_t get_cpu_clock(void)
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
    get_cpu_clock();
    _exit(0);
}

void init_cpu_clock(void)
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
void init_cpu_clock(void)
{
}
#endif

#ifdef HAVE_CPU_CLOCK
static uint32_t cycles_per_usec;

static uint32_t get_cycles_per_usec(void)
{
    uint64_t wc_s, wc_e;
    uint64_t c_s, c_e;

    if (clock_read(tsc_ref_clock, &wc_s)) {
        fprintf(stderr, "Reference clock '%s' died while measuring TSC frequency\n",
                clock_name(tsc_ref_clock));
        abort();
    }
    c_s = get_cpu_clock();
    do {
        uint64_t elapsed;

        if (clock_read(tsc_ref_clock, &wc_e)) {
            fprintf(stderr, "Reference clock '%s' died while measuring TSC frequency\n",
                    clock_name(tsc_ref_clock));
            abort();
        }
        elapsed = wc_e - wc_s;
        if (elapsed >= 1280000ULL) {
            c_e = get_cpu_clock();
            break;
        }
    } while (1);

    return (c_e - c_s + 127) >> 7;
}

#define NR_TIME_ITERS 10
void calibrate_cpu_clock(void)
{
    double delta, mean, S;
    uint32_t avg, cycles[NR_TIME_ITERS];
    int i, samples;

    struct clockspec for_clock = {CPERF_TSC, 0};
    choose_ref_clock(&tsc_ref_clock, for_clock);

    cycles[0] = get_cycles_per_usec();
    S = delta = mean = 0.0;
    for (i = 0; i < NR_TIME_ITERS; i++) {
        cycles[i] = get_cycles_per_usec();
        delta = cycles[i] - mean;
        if (delta) {
            mean += delta / (i + 1.0);
            S += delta * (cycles[i] - mean);
        }
    }

    S = sqrt(S / (NR_TIME_ITERS - 1.0));

    samples = avg = 0;
    for (i = 0; i < NR_TIME_ITERS; i++) {
        double this = cycles[i];

        if ((fmax(this, mean) - fmin(this, mean)) > S)
            continue;
        samples++;
        avg += this;
    }

    S /= (double)NR_TIME_ITERS;
    mean /= 10.0;

    avg /= samples;
    avg = (avg + 9) / 10;

    cycles_per_usec = avg;
}
#else
void calibrate_cpu_clock(void)
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
        case CPERF_NONE:
            *output = 0;
            break;
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
            *output = (get_cpu_clock() * 1000ULL) / cycles_per_usec;
            break;
#endif
        case CPERF_CLOCK:
            *output = clock() * CLOCK_RATIO;
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
#ifdef HAVE_FTIME
        case CPERF_FTIME:
            {
                struct timeb time;
                if (ftime(&time))
                    return 1;
                *output = (time.time * 1000000000ULL) + (time.millitm * 1000000ULL);
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
            *output = mach_absolute_time();
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
#if _WIN32_WINNT >= 0x0602
        case CPERF_GETSYSTIMEPRECISE:
            {
                FILETIME ft;
                GetSystemTimePreciseAsFileTime(&ft);
                *output = ((uint64_t)ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32)) * 100ULL;
            }
            break;
#endif
        case CPERF_UNBIASEDINTTIME:
            {
                ULONGLONG t;
                if (!QueryUnbiasedInterruptTime(&t))
                    return 1;
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
        break;
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
        return "tsc";
#endif
    case CPERF_CLOCK:
        return "clock";
#ifdef HAVE_GETRUSAGE
    case CPERF_RUSAGE:
        return "getrusage";
#endif
#ifdef HAVE_FTIME
    case CPERF_FTIME:
        return "ftime";
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
#endif
    default:
        return "unknown";
    }
}

void clock_choose_ref(struct clockspec spec)
{
    choose_ref_clock(&ref_clock, spec);
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
            return 1;
            break;
#endif
#ifdef HAVE_CPU_CLOCK
        case CPERF_TSC:
            hz = cycles_per_usec * 1000000ULL;
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
#ifdef HAVE_FTIME
        case CPERF_FTIME:
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
        case CPERF_UNBIASEDINTTIME:
        case CPERF_GETSYSTIME:
        case CPERF_GETSYSTIMEPRECISE:
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
