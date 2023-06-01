/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"

#include "tscemu.h"

#if defined(TARGET_OS_LINUX)
#if defined(TARGET_CPU_X86) || defined(TARGET_CPU_X64)
#define TSCEMU_SUPPORTED
#endif
#endif

#if defined(TSCEMU_SUPPORTED)

#include <assert.h>
#include <signal.h>
#include <sys/prctl.h>
#include <x86intrin.h>

// NOTE: This is a proof-of-concept for RDTSC trap/emulate. Technically we're
// just trapping the TSC and passing through the instruction execution as
// though it had happened without being trapped. So this is more of an estimate
// of the overhead of doing SIGSEGV handling for wrapping the TSC.

static uint64_t untrapped_rdtscp(uint32_t *aux)
{
    uint64_t tsc;
    int rv;

    // re-enable TSC
    rv = prctl(PR_SET_TSC, PR_TSC_ENABLE, 0, 0, 0);
    assert(rv == 0);

    tsc = __rdtscp(aux);

    // disable TSC again
    rv = prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0);
    assert(rv == 0);

    return tsc;
}

static uint64_t untrapped_rdtsc(void)
{
    uint64_t tsc;
    int rv;

    // re-enable TSC
    rv = prctl(PR_SET_TSC, PR_TSC_ENABLE, 0, 0, 0);
    assert(rv == 0);

    tsc = __rdtsc();

    // disable TSC again
    rv = prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0);
    assert(rv == 0);

    return tsc;
}

static void tsc_handler(int sig, siginfo_t *si, void *context)
{
    mcontext_t *mcontext;
    uint8_t *code;

    (void)sig;
    (void)si;

    mcontext = &((ucontext_t *)context)->uc_mcontext;

#if defined(TARGET_CPU_X64)
    code = (uint8_t *)mcontext->gregs[REG_RIP];

    if (code[0] == 0x0F && code[1] == 0x01 && code[2] == 0xF9) {
        // RDTSCP (0F 01 F9)
        uint32_t aux = 0;
        uint64_t tsc = untrapped_rdtscp(&aux);
        mcontext->gregs[REG_RAX] = (mcontext->gregs[REG_RAX] &~ 0xFFFFFFFFULL) | (tsc & 0xFFFFFFFFULL);
        mcontext->gregs[REG_RDX] = (mcontext->gregs[REG_RDX] &~ 0xFFFFFFFFULL) | ((tsc >> 32ULL) & 0xFFFFFFFFULL);
        mcontext->gregs[REG_RCX] = (mcontext->gregs[REG_RCX] &~ 0xFFFFFFFFULL) | aux;
        mcontext->gregs[REG_RIP] += 3;
        return;
    }

    if (code[0] == 0x0F && code[1] == 0x31) {
        // RDTSC (0F 31)
        uint64_t tsc = untrapped_rdtsc();
        mcontext->gregs[REG_RAX] = (mcontext->gregs[REG_RAX] &~ 0xFFFFFFFFULL) | (tsc & 0xFFFFFFFFULL);
        mcontext->gregs[REG_RDX] = (mcontext->gregs[REG_RDX] &~ 0xFFFFFFFFULL) | ((tsc >> 32ULL) & 0xFFFFFFFFULL);
        mcontext->gregs[REG_RIP] += 2;
        return;
    }
#else
    code = (uint8_t *)mcontext->gregs[REG_EIP];

    if (code[0] == 0x0F && code[1] == 0x01 && code[2] == 0xF9) {
        // RDTSCP (0F 01 F9)
        uint32_t aux = 0;
        uint64_t tsc = untrapped_rdtscp(&aux);
        mcontext->gregs[REG_EAX] = (uint32_t)(tsc & 0xFFFFFFFFULL);
        mcontext->gregs[REG_EDX] = (uint32_t)((tsc >> 32ULL) & 0xFFFFFFFFULL);
        mcontext->gregs[REG_ECX] = aux;
        mcontext->gregs[REG_EIP] += 3;
        return;
    }

    if (code[0] == 0x0F && code[1] == 0x31) {
        // RDTSC (0F 31)
        uint64_t tsc = untrapped_rdtsc();
        mcontext->gregs[REG_EAX] = (uint32_t)(tsc & 0xFFFFFFFFULL);
        mcontext->gregs[REG_EDX] = (uint32_t)((tsc >> 32ULL) & 0xFFFFFFFFULL);
        mcontext->gregs[REG_EIP] += 2;
        return;
    }
#endif

    // Unrecognized instruction, just abort
    abort();
}

static int tsc_handler_install(void)
{
    struct sigaction action;
    int rv;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    action.sa_sigaction = tsc_handler;
    rv = sigaction(SIGSEGV, &action, NULL);
    assert(rv == 0);
    return (rv == 0) ? 0 : 1;
}

static int tsc_handler_remove(void)
{
    struct sigaction action;
    int rv;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_sigaction = (void*)SIG_DFL;
    rv = sigaction(SIGSEGV, &action, NULL);
    assert(rv == 0);
    return (rv == 0) ? 0 : 1;
}

int tscemu_init(void)
{
    return tsc_handler_install();
}

int tscemu_destroy(void)
{
    return tsc_handler_remove();
}

int tscemu_enable(void)
{
    int rv;
    rv = prctl(PR_SET_TSC, PR_TSC_SIGSEGV, 0, 0, 0);
    assert(rv == 0);
    return rv == 0 ? 0 : 1;
}

int tscemu_disable(void)
{
    int rv;
    rv = prctl(PR_SET_TSC, PR_TSC_ENABLE, 0, 0, 0);
    assert(rv == 0);
    return rv == 0 ? 0 : 1;
}

#else

//
// Only supported on Linux for now
//

int tscemu_init(void)
{
    return 1;
}

int tscemu_destroy(void)
{
    return 1;
}

int tscemu_enable(void)
{
    return 1;
}

int tscemu_disable(void)
{
    return 1;
}

#endif

/* vim: set ts=4 sts=4 sw=4 et: */
