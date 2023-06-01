/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#pragma once

#include "platform.h"

#ifdef TARGET_OS_WINDOWS

/* ntdll */
typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *NtSetTimerResolution_t)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
typedef NTSTATUS (NTAPI *NtQueryTimerResolution_t)(PULONG MinimumResolution, PULONG MaximumResolution, PULONG CurrentResolution);

extern NtSetTimerResolution_t pNtSetTimerResolution;
extern NtQueryTimerResolution_t pNtQueryTimerResolution;

/* kernel32 */
typedef VOID (WINAPI *GetSystemTimePreciseAsFileTime_t)(LPFILETIME lpSystemTimeAsFileTime);
typedef VOID (WINAPI *QueryUnbiasedInterruptTimePrecise_t)(PULONGLONG lpUnbiasedInterruptTimePrecise);
typedef BOOL (WINAPI *QueryUnbiasedInterruptTime_t)(PULONGLONG UnbiasedTime);

extern GetSystemTimePreciseAsFileTime_t pGetSystemTimePreciseAsFileTime;
extern QueryUnbiasedInterruptTimePrecise_t pQueryUnbiasedInterruptTimePrecise;
extern QueryUnbiasedInterruptTime_t pQueryUnbiasedInterruptTime;

#endif

void winapi_init(void);

/* vim: set ts=4 sts=4 sw=4 et: */
