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
#include "affinity.h"
#include "clock.h"
#include "drift.h"

#ifdef HAVE_DRIFT_TESTS

#include <assert.h>
#include <stdbool.h>
#include <omp.h>

struct global_cfg {
	struct clockspec clk;
	struct clockspec ref;
};

typedef enum {
	UNSTARTED = 0,  // not yet spawned
	WAITING = 1,    // waiting for requests from master
	REPORTING = 2,  // thread asked to report in
	EXITING = 3,    // thread asked to exit
	DEAD = 4,       // thread exited
} thread_state;

struct thread_ctx {
	thread_state state;

    uint64_t last_clk;
    uint64_t last_ref;

    char padding[104];         // padding to at least one L2 cache line wide
};

static inline int driftsleep(int usec)
{
#ifdef TARGET_OS_WINDOWS
	usec /= 1000;
	if (usec < 1)
		usec = 1;
	Sleep(usec);
	return 0;
#else
	return usleep(usec);
#endif
}

static uint32_t thread_count;

void drift_init(void)
{
	#pragma omp parallel
	{
		#pragma omp master
		{
			thread_count = omp_get_num_threads();
		}
	}
}

void drift_run(uint32_t runtime_ms, struct clockspec clkid, struct clockspec refid)
{
	uint32_t idx;
	struct thread_ctx *threads = NULL;
	struct global_cfg cfg;

	memset(&cfg, 0, sizeof(struct global_cfg));

	cfg.clk = clkid;
	cfg.ref = refid;

	threads = (struct thread_ctx *)calloc(thread_count, sizeof(struct thread_ctx));

	/* Spawn drift thread per CPU */
	#pragma omp parallel
	{
		#pragma omp master
		{
			struct thread_ctx *thread, *this = NULL;
			uint64_t start_ref, start_clk;
			int64_t delta_clk, expect_ms_ref;

			uint32_t unstarted;

			do {
				unstarted = 0;
				for (idx = 0; idx < thread_count; idx++) {
					thread = &threads[idx];
					if (thread->state == UNSTARTED) {
						unstarted++;
						this = thread;
					}
				}
			} while (unstarted != 1);

			thread_bind(omp_get_thread_num());

			//uint64_t curr_clk;
			//int64_t delta_ref, expect_ms_clk;

			clock_read(cfg.clk, &start_clk);
			clock_read(cfg.ref, &start_ref);

			do {
				for (idx = 0; idx < thread_count; idx++) {
					thread = &threads[idx];
					if (thread->state > UNSTARTED)
						thread->state = REPORTING;
				}

				clock_read(cfg.clk, &this->last_clk);
				clock_read(cfg.ref, &this->last_ref);

				for (idx = 0; idx < thread_count; idx++) {
					thread = &threads[idx];
					while (thread->state == REPORTING)
						driftsleep(10);
				}

				expect_ms_ref = (this->last_ref / 1000000ULL) - (start_ref / 1000000ULL);
				//expect_ms_clk = (this->last_clk / 1000000ULL) - (start_clk / 1000000ULL);

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

					if ((idx + 1) % 8 == 0 && idx < thread_count - 1)
						printf("\n%11s", "");
				}

				printf("\n");

				driftsleep(1000000);
			} while(expect_ms_ref < runtime_ms);

			for (idx = 0; idx < thread_count; idx++) {
				thread = &threads[idx];
				thread->state = EXITING;
			}
		}

		#pragma omp for
		for(uint32_t i = 0; i < thread_count; i++)
		{
			uint32_t thread_id = omp_get_thread_num();
			struct thread_ctx *ctx = &threads[thread_id];

			struct clockspec clk_id = cfg.clk;
			struct clockspec ref_id = cfg.ref;

			thread_bind(thread_id);

			//printf("starting thread %d : %d\n", thread_id, i);
			if (ctx->state != UNSTARTED)
				continue;

			do {
				uint64_t clk;
				uint64_t ref;
				while (ctx->state == WAITING) {
					//printf("thread %d:%d waiting\n", thread_id, i);
					driftsleep(100);
				}

				if (ctx->state == EXITING)
					break;

				clock_read(clk_id, &clk);
				clock_read(ref_id, &ref);

				ctx->last_clk = clk;
				ctx->last_ref = ref;
				ctx->state = WAITING;
			} while(1);

			ctx->state = DEAD;
		}
	}

	free(threads);
}

#endif
