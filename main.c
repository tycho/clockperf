/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"
#include "affinity.h"
#include "clock.h"
#include "drift.h"
#include "util.h"
#include "winapi.h"
#include "version.h"

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#include <getopt.h>

/*
 * We run tests in pairs of clocks, attempting to corroborate the first clock
 * with the results of the second clock. If there's too much mismatch between
 * the two, then a warning is printed.
 */
static struct clockspec clock_sources[] = {
    /* Characterizes overhead of measurement mechanism. */
    //{CPERF_NONE, 0},

#ifdef HAVE_CPU_CLOCK
    {CPERF_TSC, 0},
#endif
#ifdef HAVE_GETTIMEOFDAY
    {CPERF_GTOD, 0},
#endif
#ifdef HAVE_MACH_TIME
    {CPERF_MACH_TIME, 0},
#endif
#ifdef HAVE_CLOCK_GETTIME
    {CPERF_GETTIME, CLOCK_REALTIME},
#ifdef CLOCK_REALTIME_COARSE
    {CPERF_GETTIME, CLOCK_REALTIME_COARSE},
#endif
#ifdef CLOCK_MONOTONIC
    {CPERF_GETTIME, CLOCK_MONOTONIC},
#endif
#ifdef CLOCK_MONOTONIC_COARSE
    {CPERF_GETTIME, CLOCK_MONOTONIC_COARSE},
#endif
#ifdef CLOCK_MONOTONIC_RAW
    {CPERF_GETTIME, CLOCK_MONOTONIC_RAW},
#endif
#ifdef CLOCK_MONOTONIC_RAW_APPROX // OS X
    {CPERF_GETTIME, CLOCK_MONOTONIC_RAW_APPROX},
#endif
#ifdef CLOCK_BOOTTIME
    {CPERF_GETTIME, CLOCK_BOOTTIME},
#endif
#ifdef CLOCK_UPTIME_RAW // OS X
    {CPERF_GETTIME, CLOCK_UPTIME_RAW},
#endif
#ifdef CLOCK_UPTIME_RAW_APPROX // OS X
    {CPERF_GETTIME, CLOCK_UPTIME_RAW_APPROX},
#endif
#ifdef ALLOW_RUSAGE_CLOCKS
#ifdef CLOCK_PROCESS_CPUTIME_ID
    {CPERF_GETTIME, CLOCK_PROCESS_CPUTIME_ID},
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
    {CPERF_GETTIME, CLOCK_THREAD_CPUTIME_ID},
#endif
#endif // ALLOW_RUSAGE_CLOCKS
#endif // HAVE_CLOCK_GETTIME
#ifdef ALLOW_RUSAGE_CLOCKS
#ifdef HAVE_CLOCK
    {CPERF_CLOCK, 0},
#endif
#ifdef HAVE_GETRUSAGE
    {CPERF_RUSAGE, 0},
#endif
#endif // ALLOW_RUSAGE_CLOCKS
#ifdef ALLOW_LOWRES_CLOCKS
#ifdef HAVE_TIME
    {CPERF_TIME, 0},
#endif
#endif // ALLOW_LOWRES_CLOCKS
#ifdef TARGET_OS_WINDOWS
    {CPERF_QUERYPERFCOUNTER, 0},
    {CPERF_GETTICKCOUNT, 0},
    {CPERF_GETTICKCOUNT64, 0},
    {CPERF_TIMEGETTIME, 0},
    {CPERF_GETSYSTIME, 0},
    {CPERF_GETSYSTIMEPRECISE, 0},
    {CPERF_UNBIASEDINTTIME, 0},
    {CPERF_UNBIASEDINTTIMEPRECISE, 0},
#endif
    {CPERF_NULL, 0},
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
        case 190:
        case 191:
        case 192:
        case 193:
        case 194:
        case 195:
        case 196:
        case 197:
        case 198:
        case 199:
        case 200:
            T = 1.960;
            break;
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
        printf("Failed to read from clock '%s' (%u, %u)\n",
                clock_name(self), self.major, self.minor);
        return;
    }

    if (clock_read(other, &t[0]) != 0) {
        printf("Failed to read from clock '%s' (%u, %u)\n",
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

static void version(void)
{
    printf("clockperf v%s\n\n", clockperf_version_long());
}

static void usage(const char *argv0)
{
    printf("usage:\n");
    printf("  %s [--drift [clocksource] | --monitor [clocksource]] [--ref reference-clocksource]\n", argv0);
    printf("  %s --list\n", argv0);
}


/* Argh: https://stackoverflow.com/questions/1052746/getopt-does-not-parse-optional-arguments-to-parameters */
#define FIX_OPTARG() do { \
        if(!optarg \
           && optind < argc /* make sure optind is valid */ \
           && NULL != argv[optind] /* make sure it's not a null string */ \
           && '\0' != argv[optind][0] /* ... or an empty string */ \
           && '-' != argv[optind][0] /* ... or another option */ \
          ) { \
          /* update optind so the next getopt_long invocation skips argv[optind] */ \
          optarg = argv[optind++]; \
        } \
    } while(0);

/* < 0   do all drift tests
 *   0   do no drift tests
 * > 0   do drift test for specific clock
 */
static int do_drift;
static int do_monitor;
static int do_list;
static int ref_index;

int main(int argc, char **argv)
{
    int i;
    struct clockspec *p;

    version();

    while (1) {
        static struct option long_options[] = {
            {"version", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {"drift", optional_argument, 0, 'd'},
            {"monitor", optional_argument, 0, 'm'},
            {"ref", optional_argument, 0, 'r'},
            {"list", optional_argument, 0, 'l'},
            {0, 0, 0, 0}
        };
        int c, option_index = 0;

        c = getopt_long(argc, argv, "vhr:m:d:l", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 0:
            break;
        case 'd':
        case 'm':
        case 'r':
            {
                int v = -1;
                FIX_OPTARG();
                if (optarg) {
                    /* Find matching clock */
                    for (i = 0, p = clock_sources; p->major != CPERF_NULL; i++, p++) {
                        const char *name = clock_name(*p);
                        if (strcasecmp(optarg, name) == 0) {
                            /* exact match, we're done. */
                            v = i + 1;
                            break;
                        }
                        if (strncasecmp(optarg, name, strlen(optarg)) == 0) {
                            /* partial match, keep going in case there's an exact one. */
                            v = i + 1;
                        }
                    }
                    if (v == -1) {
                        /* no matches, but an argument was provided. */
                        printf("error: could not find clock named '%s'\n", optarg);
                        return 1;
                    }
                }
                if (c == 'd')
                    do_drift = v;
                else if (c == 'm')
                    do_monitor = v;
                else if (c == 'r')
                    ref_index = v;
            }
            break;
        case 'l':
            do_list = 1;
            break;
        case 'v':
            /* We already printed the version. Only print the license. */
            license();
            return 0;
        case 'h':
        case '?':
        default:
            usage(argv[0]);
            return 0;
        }
    }

    winapi_init();
    timers_init();
    thread_init();
    cpu_clock_init();
    cpu_clock_calibrate();
#ifdef HAVE_DRIFT_TESTS
    if (do_drift)
        drift_init();
#endif

    if (do_list) {
        printf("== Clocksources Supported in This Build ==\n\n");

        for (p = clock_sources; p->major != CPERF_NULL; p++) {
            printf("%-22s\n",
                    clock_name(*p));
        }
        printf("\n");
        return 0;
    }

    if (do_drift <= 0 && !do_monitor) {
        printf("== Reported Clock Frequencies ==\n\n");

        for (p = clock_sources; p->major != CPERF_NULL; p++) {
            uint64_t res;
            char buf[16];

            if (clock_resolution(*p, &res) != 0)
                continue;

            printf("%-22s %s\n",
                    clock_name(*p),
                    pretty_print(buf, sizeof(buf), res, rate_suffixes, 10));
        }
        printf("\n\n");

        printf("== Clock Behavior Tests ==\n\n");

        printf("Name                Cost(ns)      +/-    Resol  Mono  Fail  Warp  Stal  Regr\n");
        for (p = clock_sources; p->major != CPERF_NULL; p++) {
            uint64_t read;
            if (clock_read(*p, &read) != 0)
                continue;
            clock_choose_ref(*p);
            clock_compare(*p, ref_clock);
        }
        printf("\n\n");
    }

    if (do_drift) {
        printf("== Clock Drift Tests ==\n");
#ifdef HAVE_DRIFT_TESTS
        for (i = 0, p = clock_sources; p->major != CPERF_NULL; i++, p++) {
            if (do_drift > 0 && i != do_drift - 1)
                continue;

            if (ref_index > 0 && do_drift > 0)
                clock_set_ref(clock_sources[ref_index - 1]);
            else
                clock_choose_ref(*p);

            printf("\n%9s: %s\n%9s: %s\n",
                "Primary", clock_name(*p),
                "Reference", clock_name(ref_clock));
            drift_run(do_drift > 0 ? 60000 : 10000, *p, ref_clock);
        }
#else
        printf("error: support for clock drift tests is not compiled in to this build\n");
#endif
    }

    if (do_monitor) {
        uint64_t base_values[CPERF_NUM_CLOCKS];
        uint64_t current_values[CPERF_NUM_CLOCKS];

        uint64_t wall_time_base, wall_time;

        printf("== Monitoring Raw Clock Values ==\n");

        // Read values once to get base values
        for (i = 0, p = clock_sources; p->major != CPERF_NULL; i++, p++) {
            if (clock_read(*p, &base_values[i]))
                base_values[i] = ~0ULL;
        }

        // Choose a wall clock for reference
        if (ref_index > 0)
            clock_set_ref(clock_sources[ref_index - 1]);
        else
            clock_choose_ref_wall();

        clock_read(ref_clock, &wall_time_base);
        do {
            clock_read(ref_clock, &wall_time);

            printf("Elapsed: %" PRIu64" ms\n", (wall_time - wall_time_base) / 1000000);

            for (i = 0, p = clock_sources; p->major != CPERF_NULL; i++, p++) {
                if (do_monitor > 0 && i != do_monitor - 1)
                    continue;
                clock_read(*p, &current_values[i]);
                printf("%22s: +%-20" PRIu64 " ms (%-20" PRIu64 " ms)\n", clock_name(*p),
                        (current_values[i] - base_values[i]) / 1000000,
                        current_values[i] / 1000000);
            }
            printf("\n");

            // 1000ms
            thread_sleep(1000 * 1000);
        } while (1);
    }

    timers_destroy();
    return 0;
}

/* vim: set ts=4 sts=4 sw=4 et: */
