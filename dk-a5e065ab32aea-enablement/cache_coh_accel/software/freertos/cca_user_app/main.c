/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * CCA FreeRTOS Demo Application
 *
 * Demonstrates how to use the CCA driver with a chained job transfer:
 * multiple independent src -> dst segments are submitted in a single DMA pass
 * using cca_transfer_submit() with a cca_job_t array.
 *
 * Flow:
 *   1. Load FPGA bitstream from QSPI flash
 *   2. Initialise the CCA driver (bridge, hardware, ISRs)
 *   3. Allocate per-job source and destination buffers
 *   4. Fill each source buffer with a distinct pattern and print it
 *   5. Submit all jobs as a single chained transfer
 *   6. Block until the accelerator is done
 *   7. Print each destination buffer and verify it matches the source
 *   8. Report the result and free all buffers
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "socfpga_interrupt.h"
#include "socfpga_console.h"
#include "socfpga_smmu.h"
#include "socfpga_flash.h"
#include "socfpga_fpga_manager.h"
#include "socfpga_cache.h"

#include "osal.h"
#include "osal_log.h"

#include "cca_driver.h"

/* Set to 1 to enable the throughput benchmark task, 0 to disable */
#define ENABLE_BENCH_TASK 1
#if ENABLE_BENCH_TASK
#include "bench_task.h"
#endif

/* Set to 1 to enable the cache coherency verification task, 0 to disable */
#define ENABLE_COHERENCY_TEST 0
#if ENABLE_COHERENCY_TEST
#include "coherency_test.h"
#endif

#define TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define BITSTREAM_OFFSET 0x08000000

#define DEMO_NUM_JOBS 3U // Number of jobs to run in the app
#define DEMO_JOB_SIZE 128U // The size of each job, this must be a multiple of 32
#define DEMO_WORDS_PER_JOB (DEMO_JOB_SIZE / sizeof(uint32_t)) /* 32 words */

/* DMA transfer timeout */
#define DEMO_TIMEOUT_MS 5000U /* 5 seconds */

/* Static driver context */
static cca_driver_t cca;

/* Handles used by the demo task to wake additional tasks */
#if ENABLE_BENCH_TASK
static TaskHandle_t bench_task_handle = NULL;
#endif
#if ENABLE_COHERENCY_TEST
static TaskHandle_t coherency_task_handle = NULL;
#endif

/*
 * FreeRTOS hook functions
 */
void vApplicationTickHook(void)
{
}
void vApplicationIdleHook(void)
{
}
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}

/*
 * Bitstream loading
 */
static int load_bitstream_from_qspi(void)
{
    int status = 1;

    flash_handle_t flash_handle = flash_open(QSPI_DEV0);
    if (flash_handle == NULL) {
        ERROR("Failed to open QSPI flash device");
        return status;
    }

    /* Read the 4-byte bitstream size header stored at BITSTREAM_OFFSET */
    uint8_t size_buf[4];
    int ret = flash_read_sync(flash_handle, BITSTREAM_OFFSET, size_buf, sizeof(uint32_t));
    if (ret != 0) {
        ERROR("Failed to read bitstream size header: %d", ret);
        flash_close(flash_handle);
        return status;
    }

    size_t bitstream_size = ((uint32_t)size_buf[3] << 24) | ((uint32_t)size_buf[2] << 16) |
                            ((uint32_t)size_buf[1] << 8) | (uint32_t)size_buf[0];

    PRINT("  Bitstream size: %zu bytes", bitstream_size);

    const size_t max_bitstream_size = (size_t)(64u * 1024u * 1024u);
    if (bitstream_size == 0 || bitstream_size > max_bitstream_size) {
        ERROR("Invalid bitstream size in flash header: %zu", bitstream_size);
        flash_close(flash_handle);
        return status;
    }

    void *buffer = malloc(bitstream_size);
    if (buffer == NULL) {
        ERROR("Failed to allocate bitstream buffer");
        flash_close(flash_handle);
        return status;
    }

    ret = flash_read_sync(flash_handle, BITSTREAM_OFFSET + sizeof(uint32_t), buffer, bitstream_size);
    if (ret != 0) {
        ERROR("Failed to read bitstream data: %d", ret);
        free(buffer);
        flash_close(flash_handle);
        return status;
    }

    /* Flush bitstream from CPU cache to DDR before the SDM reads it */
    cache_flush(buffer, bitstream_size);
    __asm__ volatile("dsb sy" ::: "memory");

    ret = load_fpga_bitstream(buffer, bitstream_size);
    if (ret != 0) {
        ERROR("load_fpga_bitstream failed: %d", ret);
        free(buffer);
        flash_close(flash_handle);
        return status;
    }

    status = 0;
    free(buffer);
    flash_close(flash_handle);
    return status;
}

// Buffer printing helper function
static void print_buffer(const uint32_t *buf, uint32_t nwords)
{
    for (uint32_t i = 0; i < nwords; i++) {
        if ((i % 4) == 0)
            printf("    [%02u-%02u]  ", i, i + 3U < nwords ? i + 3U : nwords - 1U);

        printf("0x%08x  ", (unsigned)buf[i]);

        if ((i % 4) == 3)
            printf("\r\n");
    }
    if (nwords % 4)
        printf("\r\n");
}

/*
 * Demo task
 */
static void run(void *arg)
{
    (void)arg;

    PRINT("========================================");
    PRINT("  CCA FreeRTOS App - Interrupt Mode");
    PRINT("  Cache Coherent Accelerator Example Usage");
    PRINT("  %u-job chained transfer", DEMO_NUM_JOBS);
    PRINT("========================================");

    // The flow for how to use the accelerator is shown below
    /* ------------------------------------------------------------------
     * Step 1: Load FPGA bitstream from QSPI flash
     * ------------------------------------------------------------------ */
    PRINT("[1/5] Loading FPGA bitstream from QSPI flash...");
    if (load_bitstream_from_qspi() != 0) {
        ERROR("Bitstream load failed. Halting.");
        vTaskSuspend(NULL);
    }
    PRINT("      Bitstream loaded successfully.");

    /* ------------------------------------------------------------------
     * Step 2: Initialise the CCA driver
     * ------------------------------------------------------------------ */
    PRINT("[2/5] Initialising CCA driver...");
    cca_status_t rc = cca_init(&cca);
    if (rc != CCA_OK) {
        ERROR("cca_init failed (code %d). Halting.", (int)rc);
        vTaskSuspend(NULL);
    }
    PRINT("      Driver ready.");

    /* ------------------------------------------------------------------
     * Step 3: Allocate per-job buffers
     *
     * Each job gets its own 4096-byte-aligned src and dst buffer.
     * Buffers must always be page aligned (4KB/4096 bytes)
     * ------------------------------------------------------------------ */
    PRINT("[3/5] Allocating %u jobs x %u bytes...", DEMO_NUM_JOBS, DEMO_JOB_SIZE);

    uint32_t *src[DEMO_NUM_JOBS];
    uint32_t *dst[DEMO_NUM_JOBS];

    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
        src[j] = (uint32_t *)aligned_alloc(4096U, (DEMO_JOB_SIZE + 4095U) & ~4095U);
        dst[j] = (uint32_t *)aligned_alloc(4096U, (DEMO_JOB_SIZE + 4095U) & ~4095U);
        if (!src[j] || !dst[j]) {
            ERROR("Buffer allocation failed at job %u. Halting.", j);
            for (uint32_t k = 0; k <= j; k++) {
                free(src[k]);
                free(dst[k]);
            }
            vTaskSuspend(NULL);
        }
        /* Set destination to all zeros so if data ends up there, it is from the DMA */
        memset(dst[j], 0x00, DEMO_JOB_SIZE);

        /* Source pattern: upper byte encodes job index (0xA0 = job 0, 0xA1 = job 1)
         * lower bits are the offset*/
        uint32_t base = (0xA0U + j) << 24;
        for (uint32_t i = 0; i < DEMO_WORDS_PER_JOB; i++)
            src[j][i] = base | i;
    }

    /* ------------------------------------------------------------------
     * Step 4: Print source data, build the job array, and submit
     *
     * All DEMO_NUM_JOBS descriptors are written to OCM and both
     * prefetchers are started when cca_transfer_submit() is called
     * ------------------------------------------------------------------ */
    PRINT("[4/5] Submitting %u-job chain...", DEMO_NUM_JOBS);

    // Informational print, jobs are starting, preview input data
    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
        PRINT("  Job %u  src=%p  dst=%p", j, (void *)src[j], (void *)dst[j]);
        PRINT("  Sending:");
        print_buffer(src[j], DEMO_WORDS_PER_JOB);
    }

    // Constructing job array
    cca_job_t jobs[DEMO_NUM_JOBS];
    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++)
        jobs[j] = (cca_job_t){ .src = src[j], .dst = dst[j], .size = DEMO_JOB_SIZE };

    // Submit the job
    rc = cca_transfer_submit(&cca, jobs, DEMO_NUM_JOBS, CCA_CMD_PROCESS_DATA);
    if (rc != CCA_OK) {
        ERROR("cca_transfer_submit failed (code %d). Halting.", (int)rc);
        for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
            free(src[j]);
            free(dst[j]);
        }
        vTaskSuspend(NULL);
    }

    PRINT("\r\n      Transfer running...");

    rc = cca_transfer_wait(&cca, DEMO_TIMEOUT_MS);
    if (rc != CCA_OK) {
        ERROR("cca_transfer_wait failed (code %d). Halting.", (int)rc);
        for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
            free(src[j]);
            free(dst[j]);
        }
        vTaskSuspend(NULL);
    }
    PRINT("      Transfer complete.");

    /* ------------------------------------------------------------------
     * Step 5: Print and verify each job's destination against its source
     * ------------------------------------------------------------------ */
    PRINT("[5/5] Verifying %u job results...", DEMO_NUM_JOBS);
    bool pass = true;

    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
        PRINT("  Job %u  received:", j);
        print_buffer(dst[j], DEMO_WORDS_PER_JOB);

        bool job_pass = true;
        for (uint32_t i = 0; i < DEMO_WORDS_PER_JOB; i++) {
            if (dst[j][i] != ~src[j][i]) {
                ERROR("  Job %u word[%u]: sent 0x%08x, received 0x%08x", j, i, (unsigned)src[j][i],
                      (unsigned)dst[j][i]);
                job_pass = false;
                break;
            }
        }
        PRINT("  Job %u: %s", j, job_pass ? "PASS" : "FAIL");
        if (!job_pass)
            pass = false;
    }

    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
        free(src[j]);
        free(dst[j]);
    }

    PRINT("\r\n========================================");
    PRINT("  Overall result: %s", pass ? "PASSED" : "FAILED");
    PRINT("========================================");

#if ENABLE_BENCH_TASK
    /* Signal the benchmark task to begin */
    if (bench_task_handle != NULL)
        xTaskNotifyGive(bench_task_handle);
#endif

#if ENABLE_COHERENCY_TEST
    /* Signal the coherency verification task to begin */
    if (coherency_task_handle != NULL)
        xTaskNotifyGive(coherency_task_handle);
#endif

    vTaskSuspend(NULL);
}

/*
 * Hardware setup and entry point
 */
static void prvSetupHardware(void)
{
    interrupt_init_gic();
    (void)smmu_enable();
#if configENABLE_CONSOLE_UART
    console_init(configCONSOLE_UART_ID, "115200-8N1");
#endif
}

int main(void)
{
    prvSetupHardware();

#if ENABLE_BENCH_TASK
    bench_task_handle = bench_task_create(&cca, TASK_PRIORITY);
    if (bench_task_handle == NULL)
        goto fail;
#endif

#if ENABLE_COHERENCY_TEST
    coherency_task_handle = coherency_test_task_create(&cca, TASK_PRIORITY);
    if (coherency_task_handle == NULL)
        goto fail;
#endif

    BaseType_t xReturn = xTaskCreate(run, "cca_demo", configMINIMAL_STACK_SIZE * 8, NULL, TASK_PRIORITY, NULL);
    if (xReturn != pdPASS)
        goto fail;

    vTaskStartScheduler();

fail:
    for (;;)
        ;
}
