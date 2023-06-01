/* SPDX-License-Identifier: ISC */

/*
 * clockperf
 *
 * Copyright (c) 2016-2023, Steven Noonan <steven@uplinklabs.net>
 *
 */

#include "prefix.h"

#include "build.h"
#include "license.h"

#include "version.h"

#include <stdio.h>

void license(void)
{
    puts(CLOCKPERF_LICENSE);
}

const char *clockperf_version_short(void)
{
    return CLOCKPERF_VERSION_TAG;
}

const char *clockperf_version_long(void)
{
    return CLOCKPERF_VERSION_LONG;
}

int clockperf_version_major(void)
{
    return CLOCKPERF_VERSION_MAJOR;
}

int clockperf_version_minor(void)
{
    return CLOCKPERF_VERSION_MINOR;
}

int clockperf_version_revision(void)
{
    return CLOCKPERF_VERSION_REVISION;
}

int clockperf_version_build(void)
{
    return CLOCKPERF_VERSION_BUILD;
}

/* vim: set ts=4 sts=4 sw=4 et: */
