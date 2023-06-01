/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"
#include "util.h"
#include "winapi.h"

#ifdef TARGET_COMPILER_VC
#if defined(TARGET_CPU_X86_64) || defined(TARGET_CPU_X86)
#include <intrin.h>
#endif
#endif

#ifdef TARGET_OS_WINDOWS
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static HANDLE s_timer;
#endif

int thread_sleep(unsigned long usec)
{
#if defined(TARGET_OS_WINDOWS)
    if (s_timer) {
        long long nt_ticks = usec * 10;
        LARGE_INTEGER li;
        li.QuadPart = -nt_ticks;
        if(!SetWaitableTimer(s_timer, &li, 0, NULL, NULL, FALSE)){
            return 1;
        }
        WaitForSingleObject(s_timer, INFINITE);
    } else {
        usec /= 1000;
        if (usec < 1)
            usec = 1;
        Sleep(usec);
    }
    return 0;
#else
    return usleep(usec);
#endif
}

void timers_init(void)
{
#ifdef TARGET_OS_WINDOWS
    if (pNtSetTimerResolution && pNtQueryTimerResolution)
    {
        NTSTATUS result;
        ULONG ulMinimum = 0, ulMaximum = 0, ulCurrent = 0;

        result = pNtQueryTimerResolution(&ulMinimum, &ulMaximum, &ulCurrent);
        if (result != ERROR_SUCCESS)
            return;

        result = pNtSetTimerResolution(ulMaximum, TRUE, &ulCurrent);
        if (result != ERROR_SUCCESS)
            return;

        result = pNtQueryTimerResolution(&ulMinimum, &ulMaximum, &ulCurrent);
        if (result != ERROR_SUCCESS)
            return;
    }

    /* Disabled for now, need to understand why a waitable timer would fire too
     * early (e.g. run with --drift and you'll see it fire multiple times per
     * second)
     */
#if 0
    s_timer = CreateWaitableTimerEx(
            NULL, NULL,
            CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
#endif
#endif
}

void timers_destroy(void)
{
#ifdef TARGET_OS_WINDOWS
    if (s_timer)
        CloseHandle(s_timer);
    s_timer = NULL;
#endif
}

#if defined(TARGET_CPU_X86_64) || defined(TARGET_CPU_X86)
int cpuid_read(uint32_t *_regs)
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
