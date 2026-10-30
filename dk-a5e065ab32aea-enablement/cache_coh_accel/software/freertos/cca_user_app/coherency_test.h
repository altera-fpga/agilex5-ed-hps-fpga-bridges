/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * CCA Cache Coherency Verification Task
 */

#ifndef COHERENCY_TEST_H
#define COHERENCY_TEST_H

#include "FreeRTOS.h"
#include "task.h"
#include "cca_driver.h"

/**
 * coherency_test_task_create() - Create the cache coherency verification FreeRTOS task.
 *
 * The task blocks on a direct task notification until woken by the caller
 * via xTaskNotifyGive(handle), matching the same pattern used by bench_task.
 *
 * Test for proving cache coherency of mSGDMA write
 *
 * @drv:      Pointer to an already initialised cca_driver_t.
 *            The pointer must remain valid for the lifetime of the task.
 * @priority: FreeRTOS task priority.
 *
 * Returns the task handle on success, NULL on failure.
 */
TaskHandle_t coherency_test_task_create(cca_driver_t *drv, UBaseType_t priority);

#endif /* COHERENCY_TEST_H */
