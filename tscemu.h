/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#pragma once

int tscemu_enable(void);
int tscemu_disable(void);

int tscemu_init(void);
int tscemu_destroy(void);

/* vim: set ts=4 sts=4 sw=4 et: */
