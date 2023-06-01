/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#ifndef __prefix_h
#define __prefix_h

#include "platform.h"

#define _GNU_SOURCE

#ifdef TARGET_OS_WINDOWS
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WIN32_LEAN_AND_MEAN
#endif

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef TARGET_COMPILER_MSVC
#define snprintf _snprintf
#define inline __inline
#include <float.h>
#ifndef __FLT_EPSILON__
#define __FLT_EPSILON__ FLT_EPSILON
#endif
#else
#define HAVE_GETTIMEOFDAY
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef TARGET_OS_LINUX
#include <sys/resource.h>
#define HAVE_GETRUSAGE
#endif

#ifdef TARGET_OS_WINDOWS
#include <windows.h>
#endif

#if !defined(TARGET_COMPILER_MINGW)
#define HAVE_TIME
#endif

#ifndef TARGET_OS_WINDOWS
#define HAVE_CLOCK
#endif

#include <sys/timeb.h>

#ifdef TARGET_OS_MACOSX
#include <mach/mach_time.h>
#define HAVE_MACH_TIME
#endif

#ifdef _POSIX_TIMERS
#if _POSIX_TIMERS > 0 && !defined(TARGET_COMPILER_MINGW)
#define HAVE_CLOCK_GETTIME
#endif
#endif

#ifdef TARGET_OS_MACOSX
#ifdef CLOCK_REALTIME
#define HAVE_CLOCK_GETTIME
#endif
#endif

#ifdef TARGET_OS_WINDOWS
#undef CLOCK_THREAD_CPUTIME_ID
#undef CLOCK_PROCESS_CPUTIME_ID
#endif

#ifdef TARGET_CPU_PPC
#include <sys/wait.h>
#endif

#if defined(TARGET_COMPILER_GCC) || defined(TARGET_COMPILER_CLANG)
#define INLINE __attribute__((always_inline)) inline
#define UNUSED __attribute__((unused))
#else
#define INLINE inline
#define UNUSED
#endif

#endif

/* vim: set ts=4 sts=4 sw=4 et: */
