/*
 * clockperf
 *
 * Copyright (c) 2016-2019, Steven Noonan <steven@uplinklabs.net>
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
#include "affinity.h"
#include "clock.h"
#include "drift.h"
#include "version.h"

/*
 * We run tests in pairs of clocks, attempting to corroborate the first clock
 * with the results of the second clock. If there's too much mismatch between
 * the two, then a warning is printed.
 */
static clock_pair_t clock_pairs[] = {
    /* Characterizes overhead of measurement mechanism. */
    //{ {CPERF_NONE, 0},                               &ref_clock},

#ifdef HAVE_CPU_CLOCK
    { {CPERF_TSC, 0},                                &tsc_ref_clock },
#endif
#ifdef HAVE_GETTIMEOFDAY
    { {CPERF_GTOD, 0},                               &ref_clock },
#endif
#ifdef HAVE_MACH_TIME
    { {CPERF_MACH_TIME, 0},                          &ref_clock },
#endif
#ifdef HAVE_CLOCK_GETTIME
    { {CPERF_GETTIME, CLOCK_REALTIME},               &ref_clock },
#ifdef CLOCK_REALTIME_COARSE
    { {CPERF_GETTIME, CLOCK_REALTIME_COARSE},        &ref_clock },
#endif
#ifdef CLOCK_MONOTONIC
    { {CPERF_GETTIME, CLOCK_MONOTONIC},              &ref_clock },
#endif
#ifdef CLOCK_MONOTONIC_COARSE
    { {CPERF_GETTIME, CLOCK_MONOTONIC_COARSE},       &ref_clock },
#endif
#ifdef CLOCK_MONOTONIC_RAW
    { {CPERF_GETTIME, CLOCK_MONOTONIC_RAW},          &ref_clock },
#endif
#ifdef CLOCK_MONOTONIC_RAW_APPROX // OS X
    { {CPERF_GETTIME, CLOCK_MONOTONIC_RAW_APPROX},   &ref_clock },
#endif
#ifdef CLOCK_BOOTTIME
    { {CPERF_GETTIME, CLOCK_BOOTTIME},               &ref_clock },
#endif
#ifdef CLOCK_UPTIME_RAW // OS X
    { {CPERF_GETTIME, CLOCK_UPTIME_RAW},             &ref_clock },
#endif
#ifdef CLOCK_UPTIME_RAW_APPROX // OS X
    { {CPERF_GETTIME, CLOCK_UPTIME_RAW_APPROX},      &ref_clock },
#endif
#ifdef CLOCK_PROCESS_CPUTIME_ID
    { {CPERF_GETTIME, CLOCK_PROCESS_CPUTIME_ID},     &ref_clock },
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
    { {CPERF_GETTIME, CLOCK_THREAD_CPUTIME_ID},      &ref_clock },
#endif
#endif
#ifdef HAVE_CLOCK
    { {CPERF_CLOCK, 0},                              &ref_clock },
#endif
#ifdef HAVE_GETRUSAGE
    { {CPERF_RUSAGE, 0},                             &ref_clock },
#endif
#ifdef HAVE_FTIME
    { {CPERF_FTIME, 0},                              &ref_clock },
#endif
#ifdef HAVE_TIME
    { {CPERF_TIME, 0},                               &ref_clock },
#endif
#ifdef TARGET_OS_WINDOWS
    { {CPERF_QUERYPERFCOUNTER, 0},                   &ref_clock },
    { {CPERF_GETTICKCOUNT, 0},                       &ref_clock },
    { {CPERF_GETTICKCOUNT64, 0},                     &ref_clock },
    { {CPERF_TIMEGETTIME, 0},                        &ref_clock },
    { {CPERF_GETSYSTIME, 0},                         &ref_clock },
#if _WIN32_WINNT >= 0x0602
    { {CPERF_GETSYSTIMEPRECISE, 0},                  &ref_clock },
#endif
    { {CPERF_UNBIASEDINTTIME, 0},                    &ref_clock },
#endif
    { {CPERF_NONE, 0},                               NULL }
};

static int compare_double(const void *pa, const void *pb)
{
    double a = *(double *)pa,
           b = *(double *)pb;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static void calc_error(double *times, uint32_t samples, double *mean, double *error)
{
    double T;
    double sum, variance, deviation, sem;
    size_t i;

    switch (samples-1) {
        case 1:   T = 12.71; break;
        case 2:   T = 4.303; break;
        case 3:   T = 3.182; break;
        case 4:   T = 2.776; break;
        case 5:   T = 2.571; break;
        case 6:   T = 2.447; break;
        case 7:   T = 2.365; break;
        case 8:   T = 2.306; break;
        case 9:   T = 2.262; break;
        case 10:  T = 2.228; break;
        case 11:  T = 2.201; break;
        case 12:  T = 2.179; break;
        case 13:  T = 2.160; break;
        case 14:  T = 2.145; break;
        case 15:  T = 2.131; break;
        case 16:  T = 2.120; break;
        case 17:  T = 2.110; break;
        case 18:  T = 2.101; break;
        case 19:  T = 2.093; break;
        case 20:  T = 2.086; break;
        case 21:  T = 2.080; break;
        case 22:  T = 2.074; break;
        case 23:  T = 2.069; break;
        case 24:  T = 2.064; break;
        case 25:  T = 2.060; break;
        case 26:  T = 2.056; break;
        case 27:  T = 2.052; break;
        case 28:  T = 2.048; break;
        case 29:  T = 2.045; break;
        case 30:  T = 2.042; break;
        case 190 ... 200: T = 1.960; break;
        default:
            printf("Don't know how to deal with statistics for %u samples\n", samples);
            abort();
            break;
    }

    qsort(times, samples, sizeof(double), compare_double);

    sum = 0.0;
    for (i = 0; i < samples; i++) {
        sum += times[i];
    }

    *mean = sum / (double)samples;

    variance = 0.0;
    for (i = 0; i < samples; i++)
        variance += (times[i] - *mean) * (times[i] - *mean);
    variance /= (double)(samples - 1);

    deviation = sqrt(variance);

    sem = T * (deviation / sqrt((double)samples));

    *error = sem / *mean * 100.0;
}

static int range_intersects(double m1, double e1, double m2, double e2)
{
    /* Turn error % into literal values, then test for range intersection. */
    e1 = m1 * (e1 / 100.0);
    e2 = m2 * (e2 / 100.0);
    return !(m1 + e1 < m2 - e2 || m2 + e2 < m1 - e1);
}

const char *rate_suffixes[] = { "Hz", "KHz", "MHz", "GHz", NULL };

static const char *pretty_print(char *buffer, size_t bufsz, double v,
                                const char **suffixes, uint32_t bar)
{
    const char **suffix = suffixes;
    if (v < 0) {
        buffer[0] = 0;
        return buffer;
    }
    while (v >= bar * 1000.0 && suffix[1]) {
        v /= 1000.0;
        suffix++;
    }
    snprintf(buffer, bufsz, "%.0lf%s", v, *suffix);
    return buffer;
}


const uint32_t ITERS = 1000;

static void clock_compare(const struct clockspec self, const struct clockspec other)
{
    static double overhead = 0.0;
    uint32_t i, j;
    uint32_t ticks = 0, reads = 0, backwards = 0, jumps = 0, stalls = 0, failures = 0;
    uint64_t s[2], o[2], t[2];
    char strbuf[2][16];
    long long delta;
    uint64_t observed_res = (uint64_t)-1;

    double *cost_self, *cost_other;
    double cost_self_mean, cost_self_error, cost_other_mean, cost_other_error;

    uint32_t samples = 4;

    if (clock_read(self, &t[0]) != 0) {
        printf("Failed to read from %s clock (%u, %u)\n",
                clock_name(self), self.major, self.minor);
        return;
    }

    if (clock_read(other, &t[0]) != 0) {
        printf("Failed to read from %s clock (%u, %u)\n",
                clock_name(other), other.major, other.minor);
        return;
    }

    /* To make GCC shut up about "possibly uninitialized" variables */
    s[0] = s[1] = 0;
    o[0] = o[1] = 0;
    t[0] = t[1] = 0;

baseline:
    clock_read(other, &o[0]);

    /*
     * Wait for one tick.
     */
    clock_read(self, &t[0]);
    t[1] = t[0];
    while (t[1] == t[0])
        clock_read(self, &t[0]);

    /*
     * Measure time between ticks.
     */
    reads = 0;
    ticks = samples * 2;
    clock_read(other, &o[0]);
    clock_read(self, &t[1]);
    for (j = 0; j < ticks; j++) {

        /*
         * Read clock until it ticks.
         */
        t[0] = t[1];
        while (t[1] == t[0]) {
            clock_read(self, &t[1]);
            reads++;
        }

        /*
         * We now have a time delta from this clock which we can use to infer
         * resolution.
         */
        delta = t[1] - t[0];
        if (delta > 0 && (uint64_t)delta < observed_res)
            observed_res = delta;

        /*
         * If the clock is taking too long per tick, we don't want to sit here
         * for the entire 'ticks' time.
         */
        if (delta > 1e8) {
            ticks = j + 1;
            break;
        }
    }
    clock_read(other, &o[1]);
    delta = o[1] - o[0];

    /*
     * Baseline the clock for at least 10ms.
     */
    if (delta < 1e7) {
        samples *= 2;
        goto baseline;
    }

    delta /= ticks;

    /*
     * Clamp to either 30 or 200.
     */
    samples = fmax(30.0, 1e6 / observed_res);
    if (samples > 200)
        samples = 200;
    else if (samples > 30)
        samples = 30;

    cost_self = malloc(sizeof(double) * samples);
    cost_other = malloc(sizeof(double) * samples);

    if (reads == ticks) {
        /*
         * We got a distinct value on every read, so we cannot meaningfully
         * measure the resolution of this clock.
         */
        observed_res = 0;
    }

    ticks = 0;
    reads = 0;

    for (j = 0; j < samples; j++) {
        uint32_t sample_reads = 0;

        /* "Warm" the two clocks up */
        clock_read(other, &o[1]);
        clock_read(self, &s[1]);

        /* Begin timespan measurement */
        clock_read(other, &o[0]);
        clock_read(self, &s[0]);

        for (i = 0; i < ITERS; i++) {
            uint32_t iter_reads = 1;

            clock_read(self, &t[0]);

            /*
             * Clocks with a low resolution or without a monotonicity guarantee can
             * return the same value multiple times in a row. Read the clock until
             * it changes.
             */
            t[1] = t[0];
            while (t[1] == t[0] && iter_reads < 200) {
                clock_read(self, &t[1]);
                iter_reads++;
            }
            delta = t[1] - t[0];

            if (delta == 0)
                /*
                 * Clock didn't advance in over 200 reads! Really terrible clock.
                 */
                failures++;
            else if (iter_reads > 2)
                /*
                 * Clock advanced but not monotonically.
                 */
                stalls++;

            /*
             * Under virtualization some clocks can jump backwards due to the
             * hypervisor trying to overcorrect for lost time in rescheduling. We
             * detect that here and record it.
             */
            if (delta < 0)
                backwards++;

            /*
             * It's also possible for the clock to jump forward by a large step,
             * either due to hypervisor overcorrection, or not being
             * a monotonic clock source.
             */
            if (delta > 1000000LL)
                jumps++;

            sample_reads += iter_reads;
        }

        clock_read(other, &o[1]);
        clock_read(self, &s[1]);

        cost_self[j] = (double)(s[1] - s[0]) / (double)sample_reads;
        cost_other[j] = (double)(o[1] - o[0]) / (double)sample_reads;

        reads += sample_reads;
    }

    calc_error(cost_self, samples, &cost_self_mean, &cost_self_error);
    calc_error(cost_other, samples, &cost_other_mean, &cost_other_error);

    /* If we're measuring CPERF_NONE, then we're attempting to detect
     * measurement overhead.
     */
    if (self.major == CPERF_NONE) {
        /* Assume best case overhead. */
        overhead = cost_other_mean - (cost_other_mean * (cost_other_error / 100.0));
        printf("%-20s %7.2lf %7.2lf%%\n",
            "(overhead)", cost_other_mean, cost_other_error);
        goto cleanup;
    }

    cost_self_mean -= overhead;
    cost_other_mean -= overhead;

    if (observed_res > 0)
        pretty_print(strbuf[0], sizeof(strbuf[0]), 1e9 / observed_res, rate_suffixes, 10);
    else
        strcpy(strbuf[0], "----");

    printf("%-20s %7.2lf %7.2lf%% %8s %5s %5d %5d %5d %5d\n",
        clock_name(self), cost_other_mean, cost_other_error,
        strbuf[0],
        (!stalls && !backwards && !jumps && !failures) ? "Yes" : "No",
        failures / samples, jumps / samples, stalls / samples, backwards / samples);


    if ((!range_intersects(cost_self_mean, cost_self_error * 2,
                          cost_other_mean, cost_other_error * 2)
        || cost_self_error > 10.0)
        && cost_self_mean >= __FLT_EPSILON__)
    {
        printf("%-20s %7.2lf %7.2lf%%\n",
            "", cost_self_mean, cost_self_error);
    }


cleanup:
    free(cost_self);
    free(cost_other);
}

#if 0
#if defined(TARGET_CPU_X86_64) || defined(TARGET_CPU_X86)
static int cpuid(uint32_t *_regs)
{
#ifdef TARGET_COMPILER_MSVC
    __cpuidex(_regs, _regs[0], _regs[2]);
#else
#ifdef TARGET_CPU_X86
    static int cpuid_support = 0;
    if (!cpuid_support) {
        uint32_t pre_change, post_change;
        const uint32_t id_flag = 0x200000;
        asm ("pushfl\n\t"      /* Save %eflags to restore later.  */
             "pushfl\n\t"      /* Push second copy, for manipulation.  */
             "popl %1\n\t"     /* Pop it into post_change.  */
             "movl %1,%0\n\t"  /* Save copy in pre_change.   */
             "xorl %2,%1\n\t"  /* Tweak bit in post_change.  */
             "pushl %1\n\t"    /* Push tweaked copy... */
             "popfl\n\t"       /* ... and pop it into %eflags.  */
             "pushfl\n\t"      /* Did it change?  Push new %eflags... */
             "popl %1\n\t"     /* ... and pop it into post_change.  */
             "popfl"           /* Restore original value.  */
             : "=&r" (pre_change), "=&r" (post_change)
             : "ir" (id_flag));
        if (((pre_change ^ post_change) & id_flag) == 0)
            return 1;
        cpuid_support = 1;
    }
#endif
    asm volatile(
        "cpuid"
        : "=a" (_regs[0]),
          "=b" (_regs[1]),
          "=c" (_regs[2]),
          "=d" (_regs[3])
        : "0" (_regs[0]), "2" (_regs[2]));
#endif
    return 0;
}
#endif

/*
 * int have_invariant_tsc(void)
 *
 * returns nonzero if CPU has invariant TSC
 */
static int have_invariant_tsc(void)
{
    static int ret = -1;

    if (ret != -1)
        return ret;

    ret = 0;

#if defined(TARGET_CPU_X86) || defined(TARGET_CPU_X86_64)
    {
    uint32_t regs[4];
    char vendor[13];

    memset(regs, 0, sizeof(regs));
    if (cpuid(regs)) {
        /* CPUID couldn't be queried */
        return ret;
    }
    vendor[12] = 0;
    *(uint32_t *)(&vendor[0]) = regs[1];
    *(uint32_t *)(&vendor[4]) = regs[3];
    *(uint32_t *)(&vendor[8]) = regs[2];

    if (!strcmp(vendor, "GenuineIntel") ||
        !strcmp(vendor, "AuthenticAMD")) {
        memset(regs, 0, sizeof(regs));
        regs[0] = 0x80000000;
        cpuid(regs);
        if (regs[0] >= 0x80000007) {
            memset(regs, 0, sizeof(regs));
            regs[0] = 0x80000007;
            cpuid(regs);
            ret = (regs[3] & 0x100) ? 1 : 0;
        }
    }
    }
#endif

    return ret;
}
#endif

int main(UNUSED int argc, UNUSED char **argv)
{
    clock_pair_t *p;

    printf("clockperf v%s\n\n", clockperf_version_long());

#ifdef TARGET_OS_WINDOWS
    timeBeginPeriod(1);
#endif

    thread_init();
    init_cpu_clock();
    calibrate_cpu_clock();

#if 0
    printf("Invariant TSC: %s\n\n", have_invariant_tsc() ? "Yes" : "No");
#endif

    printf("== Reported Clock Frequencies ==\n\n");

    for (p = clock_pairs; p && p->ref; p++) {
        uint64_t res;
        char buf[16];

        if (clock_resolution(p->primary, &res) != 0)
            continue;

        printf("%-22s %s\n",
                clock_name(p->primary),
                pretty_print(buf, sizeof(buf), res, rate_suffixes, 10));
    }
    printf("\n\n");

    printf("== Clock Behavior Tests ==\n\n");

    printf("Name                Cost(ns)      +/-    Resol  Mono  Fail  Warp  Stal  Regr\n");
    for (p = clock_pairs; p && p->ref; p++) {
        clock_choose_ref(p->primary);
        clock_compare(p->primary, *p->ref);
    }

#ifdef HAVE_DRIFT_TESTS
    printf("\n\n");
    printf("== Clock Drift Tests ==\n");

    for (p = clock_pairs; p && p->ref; p++) {
        clock_choose_ref(p->primary);
        printf("\n%9s: %s\n%9s: %s\n",
            "Primary", clock_name(p->primary),
            "Reference", clock_name(*p->ref));
        run_drift(10000, p->primary, *p->ref);
    }
#endif

    return 0;
}

/* vim: set ts=4 sts=4 sw=4 et: */
