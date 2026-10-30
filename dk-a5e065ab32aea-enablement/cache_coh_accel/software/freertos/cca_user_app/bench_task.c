/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * CCA Throughput Benchmark Task Implementation
 *
 * See bench_task.h for the public API and configuration knobs.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "osal_log.h"
#include "cca_driver.h"
#include "bench_task.h"

/*
 * Benchmark types
 */

typedef struct {
    uint32_t num_jobs;
    uint32_t job_size; // Job size in bytes
} bench_config_t;

typedef struct {
    const char *title;
    const bench_config_t *configs;
    uint32_t num_configs;
} bench_group_t;

// Benchmark 1: fixed job size = 32 KB, job count = 1 / 2 / 4 / 8 / 16 / 32
static const bench_config_t benchmark1[] = {
    { 1, 1024U * 32U }, //  1 job  x  32 KB =   32 KB
    { 2, 1024U * 32U }, //  2 jobs x  32 KB =   64 KB
    { 4, 1024U * 32U }, //  4 jobs x  32 KB =  128 KB
    { 8, 1024U * 32U }, //  8 jobs x  32 KB =  256 KB
    { 16, 1024U * 32U }, // 16 jobs x  32 KB =  512 KB
    { 32, 1024U * 32U }, // 32 jobs x  32 KB = 1024 KB
};

// Benchmark 2: fixed job count = 8, job size = 4 / 16 / 32 / 64 / 96 KB
static const bench_config_t benchmark2[] = {
    { 8, 4096U * 1U }, //  8 jobs x   4 KB =   32 KB
    { 8, 4096U * 4U }, //  8 jobs x  16 KB =  128 KB
    { 8, 4096U * 8U }, //  8 jobs x  32 KB =  256 KB
    { 8, 4096U * 16U }, //  8 jobs x  64 KB =  512 KB
    { 8, 4096U * 24U }, //  8 jobs x  96 KB =  768 KB
};

static const bench_group_t bench_groups[] = {
    { "Benchmark 1: job_size=32 KB, n_jobs = 1 to 32", benchmark1, sizeof(benchmark1) / sizeof(benchmark1[0]) },
    { "Benchmark 2: n_jobs=8, job_size = 4 to 96 KB", benchmark2, sizeof(benchmark2) / sizeof(benchmark2[0]) },
};

#define BENCH_NUM_GROUPS (sizeof(bench_groups) / sizeof(bench_groups[0]))

// ARM generic timer helpers

static inline uint64_t read_cntvct(void)
{
    uint64_t val;
    __asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(val)::"memory");
    return val;
}

static inline uint64_t read_cntfrq(void)
{
    uint64_t val;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

/*
 * bench_run_config() - bench config_t specifies one of the benchmarks (fixed job count vs fixed job size)
 *
 * Assumes the hardware has already been reset by the caller before entry.
 * Allocates page aligned buffers, runs BENCH_WARMUP warmup iterations
 * to stabilize cache and TLB state
 *
 * Returns false on a buffer allocation error.
 */
static bool bench_run_config(cca_driver_t *drv, const bench_config_t *cfg, uint64_t cntfrq)
{
    const uint32_t num_jobs = cfg->num_jobs;
    const uint32_t job_size = cfg->job_size;
    const uint32_t total_kib = num_jobs * job_size / 1024U;

    /* Allocate page aligned source and destination buffers */
    uint32_t *src[BENCH_MAX_JOBS];
    uint32_t *dst[BENCH_MAX_JOBS];

    for (uint32_t j = 0; j < num_jobs; j++) {
        src[j] = (uint32_t *)aligned_alloc(4096, job_size);
        dst[j] = (uint32_t *)aligned_alloc(4096, job_size);
        if (!src[j] || !dst[j]) {
            ERROR("  Benchmark: buffer alloc failed (%u jobs x %u KB)", (unsigned)num_jobs,
                  (unsigned)(job_size / 1000U));
            for (uint32_t k = 0; k <= j; k++) {
                free(src[k]);
                free(dst[k]);
            }
            return false;
        }
        memset(src[j], 0xA5, job_size);
        memset(dst[j], 0x00, job_size);
    }

    cca_job_t jobs[BENCH_MAX_JOBS];
    for (uint32_t j = 0; j < num_jobs; j++)
        jobs[j] = (cca_job_t){ .src = src[j], .dst = dst[j], .size = job_size };

    // Warmup cycles reset after each iteration (not factored into timing)
    for (uint32_t i = 0; i < BENCH_WARMUP; i++) {
        cca_status_t rc = cca_transfer_submit(drv, jobs, num_jobs, CCA_CMD_PROCESS_DATA);
        if (rc == CCA_OK)
            cca_transfer_wait(drv, BENCH_TIMEOUT_MS);
        cca_reset(drv);
    }

    // Timed portion (actual benchmark)
    uint64_t elapsed_cyc = 0;
    uint32_t completed = 0;

    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++) {
        const uint64_t t0 = read_cntvct();

        cca_status_t rc = cca_transfer_submit(drv, jobs, num_jobs, CCA_CMD_PROCESS_DATA);
        if (rc != CCA_OK) {
            ERROR("  Benchmark: submit failed iter %u (%u jobs x %u KB, code %d)", i, (unsigned)num_jobs,
                  (unsigned)(job_size / 1000U), (int)rc);
            break;
        }
        rc = cca_transfer_wait(drv, BENCH_TIMEOUT_MS);

        if (rc != CCA_OK) {
            ERROR("  Benchmark: wait failed iter %u (%u jobs x %u KB, code %d)", i, (unsigned)num_jobs,
                  (unsigned)(job_size / 1000U), (int)rc);
            break;
        }

        const uint64_t t1 = read_cntvct(); // stop the timer before reset
        elapsed_cyc += (t1 - t0);
        completed++;
    }

    for (uint32_t j = 0; j < num_jobs; j++) {
        free(src[j]);
        free(dst[j]);
    }

    // Use SI units for measurements
    // bytes_per_sec = bytes_total * cntfrq / elapsed_cyc
    const uint64_t bytes_total = (uint64_t)num_jobs * job_size * completed * 2U;
    const uint64_t bytes_per_sec = (elapsed_cyc > 0U) ? (bytes_total * cntfrq / elapsed_cyc) : 0U;

    const uint64_t kbs = bytes_per_sec / 1000ULL;

    /* MB/s (SI: 1 MB = 1000 KB) */
    const uint64_t mbs_whole = kbs / 1000ULL;
    const uint64_t mbs_frac = (kbs % 1000ULL) * 100ULL / 1000ULL;

    /* Mbit/s (1 decimal place: SI 1 Mbit = 125 KB) */
    const uint64_t mbit_x10 = kbs * 10ULL / 125ULL;
    const uint64_t mbit_whole = mbit_x10 / 10ULL;
    const uint64_t mbit_frac1 = mbit_x10 % 10ULL;

    const uint64_t elapsed_us = elapsed_cyc * 1000000ULL / cntfrq;
    const uint64_t avg_us = (completed > 0U) ? (elapsed_us / completed) : 0U;
    const uint64_t avg_ms_whole = avg_us / 1000ULL;
    const uint64_t avg_ms_frac = avg_us % 1000ULL;

    PRINT("  jobs=%2u  job=%3u KiB  xfer=%4u KiB  avg=%3llu.%03llu ms %5llu.%02llu MB/s %7llu.%01llu Mbit/s",
          (unsigned)num_jobs, (unsigned)(job_size / 1024U), (unsigned)total_kib, (unsigned long long)avg_ms_whole,
          (unsigned long long)avg_ms_frac, (unsigned long long)mbs_whole, (unsigned long long)mbs_frac,
          (unsigned long long)mbit_whole, (unsigned long long)mbit_frac1);

    /* Allow the UART FIFO to drain before the next config's header is printed */
    vTaskDelay(pdMS_TO_TICKS(50));

    return true;
}

/*
 * Benchmark task body
 */

static void bench_run(void *arg)
{
    cca_driver_t *drv = (cca_driver_t *)arg;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    PRINT("\r\n========================================");
    PRINT("  CCA Throughput Benchmark - FreeRTOS");
    PRINT("========================================");

    const uint64_t cntfrq = read_cntfrq();

    for (uint32_t g = 0; g < BENCH_NUM_GROUPS; g++) {
        const bench_group_t *grp = &bench_groups[g];

        PRINT("\r\n%s", grp->title);

        for (uint32_t c = 0; c < grp->num_configs; c++) {
            // Reset before each config for clean hardware state between runs
            cca_reset(drv);
            if (!bench_run_config(drv, &grp->configs[c], cntfrq)) {
                ERROR("  Benchmark: aborting benchmark after fatal error");
                break;
            }
        }
    }

    PRINT("\r\n========================================");
    PRINT("  Benchmark complete.");
    PRINT("========================================");

    vTaskSuspend(NULL);
}

TaskHandle_t bench_task_create(cca_driver_t *drv, UBaseType_t priority)
{
    TaskHandle_t handle = NULL;

    BaseType_t rc = xTaskCreate(bench_run, "cca_bench", configMINIMAL_STACK_SIZE * 8, drv, priority, &handle);
    if (rc != pdPASS)
        return NULL;

    return handle;
}
