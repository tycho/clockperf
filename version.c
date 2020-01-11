/*
 * clockperf
 *
 * Copyright (c) 2016-2020, Steven Noonan <steven@uplinklabs.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
