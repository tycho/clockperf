/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2021, Steven Noonan <steven@uplinklabs.net>
 *
 */

#pragma once

#include "platform.h"
#include "clock.h"

#ifdef _OPENMP
#define HAVE_DRIFT_TESTS
#endif

void drift_init(void);
void drift_run(uint32_t runtime_ms, struct clockspec clkid, struct clockspec refid);
