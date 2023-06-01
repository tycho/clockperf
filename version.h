/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#ifndef __version_h
#define __version_h

void license(void);

const char *clockperf_version_short(void);
const char *clockperf_version_long(void);
int clockperf_version_major(void);
int clockperf_version_minor(void);
int clockperf_version_revision(void);
int clockperf_version_build(void);

#endif

/* vim: set ts=4 sts=4 sw=4 et: */
