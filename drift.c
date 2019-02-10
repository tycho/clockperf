/*
 * clockperf
 *
 * Copyright (c) 2016-2019, Steven Noonan <steven@uplinklabs.net>
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
#include "clock.h"
#include "drift.h"

#ifdef HAVE_DRIFT_TESTS

#include <assert.h>
#include <stdbool.h>
#include <sched.h>
#include <pthread.h>

struct global_cfg {
	struct clockspec clk;
	struct clockspec ref;
};

struct thread_ctx {
    int32_t thread_id;         // initialized before thread created
    uint32_t state;            // 0 = proc needs to update, 1 = valid to read, 2 = thread needs to exit, 3 = thread has exited

	struct global_cfg *cfg;

    uint64_t last_clk;
    uint64_t last_ref;

    char padding[100];         // padding to at least one L2 cache line wide
};

static cpu_set_t *old_affinity;
static size_t cpu_set_size;
static size_t cpu_set_alloc;

static void bind_to_cpu(int32_t id)
{
	cpu_set_t *cs;
	cs = CPU_ALLOC(cpu_set_size);
	assert(cs);
	memset(cs, 0, CPU_ALLOC_SIZE(cpu_set_size));
	CPU_SET(id, cs);
	sched_setaffinity(0, CPU_ALLOC_SIZE(cpu_set_size), cs);
	CPU_FREE(cs);
}

static void unbind(void)
{
	sched_setaffinity(0, cpu_set_alloc, old_affinity);
}

static void *drift_thread(void *pctx)
{
	struct thread_ctx *ctx = (struct thread_ctx *)pctx;

	struct clockspec clk_id = ctx->cfg->clk;
	struct clockspec ref_id = ctx->cfg->ref;

	bind_to_cpu(ctx->thread_id);
	do {
		uint64_t clk;
		uint64_t ref;
		while (ctx->state == 1)
			usleep(1000);

		if (ctx->state == 2)
			break;

		clock_read(clk_id, &clk);
		clock_read(ref_id, &ref);

		ctx->last_clk = clk;
		ctx->last_ref = ref;
		ctx->state = 1;
	} while(1);

	ctx->state = 3;

	return NULL;
}

static cpu_set_t *current_affinity(void)
{
	int ret;
	cpu_set_t *cs;
	cpu_set_size = 2;
	do {
		cs = CPU_ALLOC(cpu_set_size);
		assert(cs);
		memset(cs, 0, CPU_ALLOC_SIZE(cpu_set_size));
		ret = sched_getaffinity(0, CPU_ALLOC_SIZE(cpu_set_size), cs);
		if (ret == 0)
			break;
		CPU_FREE(cs);
		cpu_set_size *= 2;
	} while(1);
	cpu_set_alloc = CPU_ALLOC_SIZE(cpu_set_size);
	return cs;
}

void run_drift(uint32_t runtime_ms, struct clockspec clkid, struct clockspec refid)
{
	uint32_t idx, thread_count;
	struct thread_ctx *threads = NULL, *thread;
	cpu_set_t *affinity;
	struct global_cfg cfg;

	memset(&cfg, 0, sizeof(struct global_cfg));

	cfg.clk = clkid;
	cfg.ref = refid;

	affinity = current_affinity();
	if (!affinity)
		return;

	old_affinity = CPU_ALLOC(cpu_set_size);
	memcpy(old_affinity, affinity, cpu_set_alloc);

	thread_count = CPU_COUNT_S(cpu_set_alloc, affinity);

	threads = (struct thread_ctx *)calloc(thread_count, sizeof(struct thread_ctx));

	/* Spawn drift thread per CPU */
	thread = threads;
	for(idx = 0; CPU_COUNT_S(cpu_set_alloc, affinity); idx++) {
		pthread_t pthread;
		if (!CPU_ISSET_S(idx, cpu_set_alloc, affinity))
			continue;
		CPU_CLR_S(idx, cpu_set_alloc, affinity);

		thread->thread_id = idx;
		thread->cfg = &cfg;
		pthread_create(&pthread, NULL, drift_thread, thread);

		thread++;
	}

	bind_to_cpu(0);

	{
		uint64_t start_ref, start_clk;

		uint64_t curr_ref;
		int64_t delta_clk, expect_ms_ref;

		//uint64_t curr_clk;
		//int64_t delta_ref, expect_ms_clk;

		clock_read(cfg.clk, &start_clk);
		clock_read(cfg.ref, &start_ref);

		do {
			//clock_read(cfg.clk, &curr_clk);
			clock_read(cfg.ref, &curr_ref);

			for (idx = 0; idx < thread_count; idx++) {
				thread = &threads[idx];
				thread->state = 0;
			}

			for (idx = 0; idx < thread_count; idx++) {
				thread = &threads[idx];
				while (!thread->state)
					usleep(10);
			}

			expect_ms_ref = (curr_ref / 1000000ULL) - (start_ref / 1000000ULL);
			//expect_ms_clk = (curr_clk / 1000000ULL) - (start_clk / 1000000ULL);

			printf("%9" PRId64 ": ", expect_ms_ref);

			for (idx = 0; idx < thread_count; idx++) {
				//int64_t ref_ms;
				int64_t clk_ms;

				thread = &threads[idx];

				//ref_ms = (thread->last_ref / 1000000ULL) - (start_ref / 1000000ULL);
				clk_ms = (thread->last_clk / 1000000ULL) - (start_clk / 1000000ULL);

				//delta_ref = (ref_ms - expect_ms_ref);
				delta_clk = (clk_ms - expect_ms_ref);

				printf("%6" PRId64 ", ", delta_clk);

				//if ((idx + 1) % 4 == 0 && idx < thread_count - 1)
				//	printf("\n%11s", "");
			}

			printf("\n");

			usleep(1000000);
		} while(expect_ms_ref < runtime_ms);
	}

	for (idx = 0; idx < thread_count; idx++) {
		thread = &threads[idx];
		thread->state = 2;
	}

	for (idx = 0; idx < thread_count; idx++) {
		thread = &threads[idx];
		while (thread->state != 3)
			usleep(10);
	}

	unbind();

	free(threads);
	CPU_FREE(affinity);
	CPU_FREE(old_affinity);

	old_affinity = NULL;
}

#endif
