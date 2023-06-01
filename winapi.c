/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"
#include "winapi.h"

#ifdef TARGET_OS_WINDOWS

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#define DEFINE_FUNCTION(name) \
    name##_t p##name = NULL
#define LOAD_FUNCTION(handle, name) \
    p##name = (name##_t)GetProcAddress(handle, #name); \
    if (!p##name) \
        printf("NOTICE: Failed to load %s from %s\n", #name, #handle);

DEFINE_FUNCTION(NtSetTimerResolution);
DEFINE_FUNCTION(NtQueryTimerResolution);

DEFINE_FUNCTION(GetSystemTimePreciseAsFileTime);
DEFINE_FUNCTION(QueryUnbiasedInterruptTime);

DEFINE_FUNCTION(QueryUnbiasedInterruptTimePrecise);

void winapi_init(void)
{
    HANDLE hNtDll = GetModuleHandleA("ntdll.dll"),
           hKernel32 = GetModuleHandleA("kernel32.dll"),
           hKernelBase = GetModuleHandleA("kernelbase.dll");

    if (hNtDll)
    {
        LOAD_FUNCTION(hNtDll, NtSetTimerResolution);
        LOAD_FUNCTION(hNtDll, NtQueryTimerResolution);
    }

    if (hKernel32)
    {
        LOAD_FUNCTION(hKernel32, GetSystemTimePreciseAsFileTime);
        LOAD_FUNCTION(hKernel32, QueryUnbiasedInterruptTime);
    }

    if (hKernelBase)
    {
        LOAD_FUNCTION(hKernelBase, QueryUnbiasedInterruptTimePrecise);
    }
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#else

void winapi_init(void) {}

#endif

/* vim: set ts=4 sts=4 sw=4 et: */
