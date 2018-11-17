/*
 * clockperf
 *
 * Copyright (c) 2016-2017, Steven Noonan <steven@uplinklabs.net>
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

#ifndef __prefix_h
#define __prefix_h

#include "platform.h"

#ifdef TARGET_OS_WINDOWS
#define _WIN32_WINNT 0x0601
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
#define __FLT_EPSILON__ FLT_EPSILON
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
/* On Cygwin/MinGW, ftime has a void return, so we can't use it. */
#define HAVE_FTIME
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

#ifdef __MACH__
#ifdef CLOCK_REALTIME
#define HAVE_CLOCK_GETTIME
#endif
#endif

#ifdef TARGET_CPU_PPC
#include <sys/wait.h>
#endif

#ifdef TARGET_COMPILER_GCC
#define INLINE __attribute__((always_inline)) inline
#define UNUSED __attribute__((unused))
#else
#define INLINE inline
#define UNUSED
#endif

#endif

/* vim: set ts=4 sts=4 sw=4 et: */
