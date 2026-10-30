/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * CCA Throughput Benchmark Task
 */

#ifndef BENCH_TASK_H
#define BENCH_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "cca_driver.h"

/*
 * Tuning knobs
 */
#define BENCH_WARMUP 5U // Warmup iterations before timing
#define BENCH_ITERATIONS 50U // Timed iterations per configuration
#define BENCH_TIMEOUT_MS 10000U // Per-transfer timeout (10 s)

/* Maximum job count used across all configurations (sweep 1 reaches CCA_MAX_JOBS) */
#define BENCH_MAX_JOBS CCA_MAX_JOBS

/**
 * bench_task_create() - Create the CCA throughput benchmark FreeRTOS task.
 *
 * The task blocks on a direct task notification until woken by the caller
 * via xTaskNotifyGive(handle), so that it will only run when the other task is finished
 *
 * @drv:      Pointer to an already initialised cca_driver_t.
 *            The pointer must remain valid for the lifetime of the task.
 * @priority: FreeRTOS task priority.
 */
TaskHandle_t bench_task_create(cca_driver_t *drv, UBaseType_t priority);

#endif /* BENCH_TASK_H */
