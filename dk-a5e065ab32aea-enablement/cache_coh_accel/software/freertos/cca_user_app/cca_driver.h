/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator (CCA) - FreeRTOS Public API
 */

#ifndef CCA_DRIVER_H
#define CCA_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "osal.h"

/*
 * Accelerator command codes
 *
 * Written to CCA_ACCEL_CONTROL before DMA starts.
 *   PASS_THROUGH  - effectively no op by accelerator, data remains unchanged
 *   PROCESS_DATA  - accelerator applies its data transform to the data stream
 */
#define CCA_CMD_PASS_THROUGH 0x02U
#define CCA_CMD_PROCESS_DATA 0x01U

/* Maximum number of jobs in a single chained transfer. 2 KB / 32 bytes = 64 */
#define CCA_MAX_JOBS 64U

/*
 * Job descriptor
 *
 * Each job describes one contiguous src to dst segment in a chained transfer
 */
typedef struct {
    const void *src; /* source buffer (Page (4KB) aligned)                  */
    void *dst; /* destination buffer (Page (4KB) aligned)             */
    uint32_t size; /* transfer size in bytes (non-zero, multiple of 32)  */
} cca_job_t;

/*
 * Return codes
 */
typedef enum {
    CCA_OK = 0,
    CCA_ERR_BRIDGE, /* LWH2F bridge enable/disable failed               */
    CCA_ERR_HW_ID, /* FPGA_ID or FPGA_REV register mismatch            */
    CCA_ERR_ACCEL_INIT, /* Accelerator did not assert ALIVE after reset      */
    CCA_ERR_DMA_INIT, /* MSGDMA prefetcher or dispatcher reset timed out  */
    CCA_ERR_ISR, /* Semaphore creation or ISR registration failed     */
    CCA_ERR_ARGS, /* NULL pointer or bad alignment/size passed in      */
    CCA_ERR_TIMEOUT, /* cca_transfer_wait() timed out                    */
    CCA_ERR_ACCEL, /* Accelerator status register reported an error     */
} cca_status_t;

/*
 * Driver context
 *
 * Allocate as a static or global variable
 * User should NOT edit this struct directly
 */
typedef struct {
    osal_semaphore_def_t sem_mem; /* FreeRTOS semaphore storage    */
    osal_semaphore_t transfer_complete; /* Binary semaphore: ISR → task  */
    volatile bool rd_complete; /* READ prefetcher IRQ fired      */
    volatile bool wr_complete; /* WRITE prefetcher IRQ fired     */
} cca_driver_t;

/*
 * API
 */

/**
 * cca_init() - Initialize CCA hardware
 * @drv: Pointer to driver context
 * @return CCA_OK on success
 */
cca_status_t cca_init(cca_driver_t *drv);

/**
 * cca_transfer_submit() - Submit a chained transfer
 * @drv:       Pointer to driver context
 * @jobs:      Array of job descriptors (must stay valid until cca_transfer_wait returns)
 * @num_jobs:  Number of jobs [1, CCA_MAX_JOBS]
 * @accel_cmd: Accelerator operation mode (CCA_CMD_PASS_THROUGH or CCA_CMD_PROCESS_DATA)
 * @return CCA_OK on success
 */
cca_status_t cca_transfer_submit(cca_driver_t *drv, const cca_job_t *jobs, uint32_t num_jobs, uint8_t accel_cmd);

/**
 * cca_transfer_wait() - Block until the in-flight transfer completes (interrupt-driven)
 *
 * Waits on the transfer_complete binary semaphore posted by the MSGDMA prefetcher
 * ISRs.  The calling task is suspended (not polling) for the duration.
 *
 * @drv:        Pointer to driver context
 * @timeout_ms: Maximum time to wait in milliseconds
 * @return CCA_OK on success, CCA_ERR_TIMEOUT if the semaphore is not posted within
 *         timeout_ms, CCA_ERR_ACCEL if the accelerator reports an error
 */
cca_status_t cca_transfer_wait(cca_driver_t *drv, uint32_t timeout_ms);

/**
 * cca_reset() - Resets hardware, call in between cca_transfer_submit() calls
 * @drv: Pointer to driver context
 * @return CCA_OK on success
 */
cca_status_t cca_reset(cca_driver_t *drv);

#endif /* CCA_DRIVER_H */
