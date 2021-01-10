clockperf [![Build Status](https://github.com/tycho/clockperf/workflows/CI/badge.svg)](https://github.com/tycho/clockperf/actions) [![License: ISC](https://img.shields.io/badge/License-ISC-blue.svg)](https://opensource.org/licenses/ISC)
=======

This is a small utility for measuring clocksource behavior on Linux, BSDs, OS
X, and Windows.

Building
--------

The build system is super straightforward:

```
$ make
```

Output Format
--------------

The **Cost(ns)** column is the time cost to read that clock in nanoseconds, as
measured by the TSC (except when measuring the cost of reading the TSC itself,
which is measured by looking at gettimeofday).

**Resol** is the observable tick rate of the clock. Note that if the clock
is so high resolution that we see a distinct value on every read, then the
observable resolution would merely be a function of how fast we could read it.
For this reason, a value of "----" indicates that the clock advances too
quickly for the resolution to be estimated with any precision.

**Mono** indicates whether the clock source is monotonic, with an additional
restriction. Not only must the clock only move forward, it must never return
the same value (i.e. high frequency).

**Fail** indicates the number of times the clock source failed to advance in >=
200 reads.

**Warp** indicates the number of times the clock jumped forward by an
unexpectedly large span (threshold for this is currently 1ms).

**Stal** indicates the number of times the clock source failed to advance in
more than 1, but less than 200 reads.

**Regr** indicates the number of times the clock moved backwards. This can
happen due to hypervisor behavior, NTP adjustments, and other clock changes.
It's extremely bad behavior if you use the clock source for timespan
measurement (e.g. profiling, benchmarks, etc).


Example Runs
------------

Linux 4.5.0 on x86_64 with
`/sys/devices/system/clocksource/clocksource0/current_clocksource` set to
`tsc`:
```
$ ./clockperf
clockperf v1.0.0

Invariant TSC: Yes

tsc              resolution = 2401MHz
realtime         resolution = 1000MHz
realtime_crs     resolution = 100Hz
monotonic        resolution = 1000MHz
monotonic_crs    resolution = 100Hz
monotonic_raw    resolution = 1000MHz
boottime         resolution = 1000MHz
process          resolution = 1000MHz
thread           resolution = 1000MHz
clock            resolution = 1000KHz
time             resolution = 1Hz

Name          Cost(ns)      +/-    Resol  Mono  Fail  Warp  Stal  Regr
tsc              15.93    0.55%     ----   Yes     0     0     0     0
gettimeofday     22.87    0.07%  1000KHz    No     0     0   999     0
realtime         23.69    0.16%     ----   Yes     0     0     0     0
realtime_crs      9.68    0.35%    100Hz    No   999     0     0     0
                  8.34   84.91%
monotonic        23.40    0.10%     ----   Yes     0     0     0     0
monotonic_crs     9.03    0.63%    100Hz    No   999     0     0     0
                  8.34   84.91%
monotonic_raw    73.34    0.08%     ----   Yes     0     0     0     0
boottime         77.66    0.17%     ----   Yes     0     0     0     0
process         132.47    0.08%   143MHz    No     0     0     0     0
thread          125.87    0.04%  1000MHz    No     0     0     0     0
clock           134.26    0.09%  1000KHz    No     0     0   999     0
getrusage       210.87    0.06%    100Hz    No   995     4     4     0
ftime            27.86    0.03%   1000Hz    No   994     0     5     0
time              5.36    0.05%      1Hz    No  1000     0     0     0
```

Linux 4.5.0 on x86_64 with
`/sys/devices/system/clocksource/clocksource0/current_clocksource` set to
`hpet`:
```
$ ./clockperf
clockperf v1.0.0

Invariant TSC: Yes

tsc              resolution = 2402MHz
realtime         resolution = 1000MHz
realtime_crs     resolution = 100Hz
monotonic        resolution = 1000MHz
monotonic_crs    resolution = 100Hz
monotonic_raw    resolution = 1000MHz
boottime         resolution = 1000MHz
process          resolution = 1000MHz
thread           resolution = 1000MHz
clock            resolution = 1000KHz
time             resolution = 1Hz

Name          Cost(ns)      +/-    Resol  Mono  Fail  Warp  Stal  Regr
tsc              16.13    0.51%     ----   Yes     0     0     0     0
gettimeofday    564.88    0.13%  1000KHz    No     0     0   292     0
realtime        567.63    0.94%     ----   Yes     0     0     0     0
realtime_crs      9.71    0.55%    100Hz    No   999     0     0     0
                  8.34   84.91%
monotonic       561.19    0.06%     ----   Yes     0     0     0     0
monotonic_crs     8.84    0.49%    100Hz    No   999     0     0     0
                  8.34   84.91%
monotonic_raw   618.30    0.82%     ----   Yes     0     0     0     0
boottime        619.19    0.12%     ----   Yes     0     0     0     0
process         132.17    0.25%    56MHz    No     0     0     0     0
thread          125.85    0.07%    40MHz    No     0     0     0     0
clock           134.24    0.11%  1000KHz    No     0     0   999     0
getrusage       210.51    0.11%    100Hz    No   995     4     4     0
ftime           570.64    0.28%   1000Hz    No   888     0   111     0
time              5.36    0.45%      1Hz    No  1000     0     0     0
```

Linux 3.10.40 on ARMv7a Tegra TK1 Jetson board with
`/sys/devices/system/clocksource/clocksource0/available_clocksource` set to
`arch_sys_counter`:
```
$ ./clockperf
clockperf v1.0.0


realtime         resolution = 1000MHz
realtime_crs     resolution = 1000Hz
monotonic        resolution = 1000MHz
monotonic_crs    resolution = 1000Hz
monotonic_raw    resolution = 1000MHz
boottime         resolution = 1000MHz
process          resolution = 1000MHz
thread           resolution = 1000MHz
clock            resolution = 1000KHz
time             resolution = 1Hz

Name          Cost(ns)      +/-    Resol  Mono  Fail  Warp  Stal  Regr
gettimeofday    681.59   20.29%     ----    No     0     0   848     0
                681.59   20.29%
realtime        266.57    0.18%     ----   Yes     0     0     0     0
realtime_crs    234.29    0.08%   1000Hz    No   954     0    45     0
monotonic       269.15    0.41%     ----   Yes     0     0     0     0
monotonic_crs   244.34    0.14%   1000Hz    No   952     0    46     0
monotonic_raw   282.09    0.18%     ----   Yes     0     0     0     0
boottime        271.64    0.07%     ----   Yes     0     0     0     0
process         719.18    0.22%     ----   Yes     0     0     0     0
thread          635.93    0.34%     ----   Yes     0     0     0     0
clock           728.26    0.08%  1000KHz    No     0     0   225     0
getrusage       867.63    0.23%   1000Hz    No   833     0   166     0
ftime           287.70    0.04%   1000Hz    No   944     0    55     0
time            265.54    0.22%      1Hz    No   999     0     0     0
                166.77  204.50%
```
