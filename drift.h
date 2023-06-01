/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#pragma once

#include "platform.h"
#include "clock.h"

#if defined(_OPENMP)
#define HAVE_DRIFT_TESTS
#endif

void drift_init(void);
void drift_run(uint32_t runtime_ms, struct clockspec clkid, struct clockspec refid);

/* vim: set ts=4 sts=4 sw=4 et: */
