/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"
#include "affinity.h"

#include <stdio.h>

#define MAX_CPUS 1024

#ifdef TARGET_OS_LINUX

#include <pthread.h>
#include <sched.h>
#include <string.h>
#define CPUSET_T cpu_set_t
#define CPUSET_MASK_T __cpu_mask

#elif defined(TARGET_OS_FREEBSD)

#include <pthread.h>
#include <pthread_np.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#define CPUSET_T cpuset_t
#define CPUSET_MASK_T long
#undef MAX_CPUS
#define MAX_CPUS CPU_MAXSIZE

#elif defined(TARGET_OS_SOLARIS)
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>


#elif defined(TARGET_OS_MACOSX)

#include <sys/sysctl.h>

/*#define USE_CHUD*/
#ifdef USE_CHUD
extern int chudProcessorCount(void);
extern int utilBindThreadToCPU(int n);
extern int utilUnbindThreadFromCPU(void);
#endif

#endif

#ifdef TARGET_OS_WINDOWS
#if _WIN32_WINNT < 0x0601
typedef WORD(WINAPI *fnGetActiveProcessorGroupCount)(void);
typedef DWORD(WINAPI *fnGetActiveProcessorCount)(WORD);
typedef BOOL(WINAPI *fnSetThreadGroupAffinity)(HANDLE, const GROUP_AFFINITY *, PGROUP_AFFINITY);

static fnGetActiveProcessorGroupCount GetActiveProcessorGroupCount;
static fnGetActiveProcessorCount GetActiveProcessorCount;
static fnSetThreadGroupAffinity SetThreadGroupAffinity;
#endif
#endif

void thread_init(void)
{
#ifdef TARGET_OS_WINDOWS
#if _WIN32_WINNT < 0x0601
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    GetActiveProcessorGroupCount = (fnGetActiveProcessorGroupCount)GetProcAddress(hKernel32, "GetActiveProcessorGroupCount");
    GetActiveProcessorCount = (fnGetActiveProcessorCount)GetProcAddress(hKernel32, "GetActiveProcessorCount");
    SetThreadGroupAffinity = (fnSetThreadGroupAffinity)GetProcAddress(hKernel32, "SetThreadGroupAffinity");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif
#endif
}

int thread_bind(uint32_t id)
{
#ifdef TARGET_OS_WINDOWS

    BOOL ret = FALSE;
    HANDLE hThread = GetCurrentThread();

#if _WIN32_WINNT < 0x0601
    if (is_windows7_or_greater()) {
#endif
        DWORD threadsInGroup = 0;
        WORD groupId, groupCount;
        GROUP_AFFINITY affinity;
        ZeroMemory(&affinity, sizeof(GROUP_AFFINITY));

        groupCount = GetActiveProcessorGroupCount();

        for (groupId = 0; groupId < groupCount; groupId++) {
            threadsInGroup = GetActiveProcessorCount(groupId);
            if (id < threadsInGroup)
                break;
            id -= threadsInGroup;
        }

        if (groupId < groupCount && id < threadsInGroup) {
            affinity.Group = groupId;
            affinity.Mask = 1ULL << id;

            ret = SetThreadGroupAffinity(hThread, &affinity, NULL);
        }
#if _WIN32_WINNT < 0x0601
    } else {
        DWORD mask;

        if (id > 32)
            return 1;

        mask = (1 << id);

        ret = SetThreadAffinityMask(hThread, mask);
    }
#endif

    return (ret != FALSE) ? 0 : 1;

#elif defined(TARGET_OS_LINUX) || defined(TARGET_OS_FREEBSD)

    int ret;

#ifdef CPU_SET_S
    size_t setsize = CPU_ALLOC_SIZE(MAX_CPUS);
    CPUSET_T *set = CPU_ALLOC(MAX_CPUS);
    pthread_t pth;

    pth = pthread_self();

    CPU_ZERO_S(setsize, set);
    CPU_SET_S(id, setsize, set);
    ret = pthread_setaffinity_np(pth, setsize, set);
    CPU_FREE(set);
#else
    size_t bits_per_set = sizeof(CPUSET_T) * 8;
    size_t bits_per_subset = sizeof(CPUSET_MASK_T) * 8;
    size_t setsize = sizeof(CPUSET_T) * (MAX_CPUS / bits_per_set);
    size_t set_id, subset_id;
    unsigned long long mask;
    CPUSET_T *set = malloc(setsize);
    pthread_t pth;

    pth = pthread_self();

    for (set_id = 0; set_id < (MAX_CPUS / bits_per_set); set_id++)
        CPU_ZERO(&set[set_id]);

    set_id = id / bits_per_set;
    id %= bits_per_set;

    subset_id = id / bits_per_subset;
    id %= bits_per_subset;

    mask = 1ULL << (unsigned long long)id;

    ((unsigned long *)set[set_id].__bits)[subset_id] |= mask;
    ret = pthread_setaffinity_np(pth, setsize, set);
    free(set);
#endif

    return (ret == 0) ? 0 : 1;

#elif defined(TARGET_OS_SOLARIS)

    /*
     * This requires permissions, so can easily fail.
     */
    if (processor_bind(P_LWPID, P_MYID, id, NULL) != 0) {
        fprintf(stderr, "warning: failed to bind to CPU%u: %s\n",
            id, strerror(errno));
        return 1;
    }

    return 0;
#elif defined(TARGET_OS_MACOSX)
    int ret = 1;

#ifdef USE_CHUD
    ret = (utilBindThreadToCPU(id) == 0) ? 0 : 1;
#else
#warning "thread_bind() not implementable on macOS"
#endif

    return ret == 0 ? 0 : 1;
#else
#error "thread_bind() not defined for this platform"
#endif
}

/* vim: set ts=4 sts=4 sw=4 noet: */
