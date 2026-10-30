/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator (CCA) - FreeRTOS API Implementation
 *
 * See cca_driver.h for the public API and usage.
 */

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

#include "osal.h"
#include "osal_log.h"
#include "socfpga_bridge.h"
#include "socfpga_interrupt.h"
#include "socfpga_cache.h"

#include "cca.h"
#include "cca_driver.h"

_Static_assert(CCA_MAX_JOBS == NUM_OF_DESCRIPTORS, "CCA_MAX_JOBS must equal NUM_OF_DESCRIPTORS");

/* =========================================================================
 * ISR handlers
 * =========================================================================*/

static void rd_prefetch_isr(void *context)
{
    cca_driver_t *drv = (cca_driver_t *)context;

    uint32_t status = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET);
    if (status & PREFETCHER_INTERRUPT_STATUS_MSK) {
        /* Clear prefetcher IRQ flag (W1C) */
        cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET, PREFETCHER_INTERRUPT_STATUS_MSK);

        /* Clear dispatcher IRQ flag (W1C) */
        cca_reg_write(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET, DISPATCHER_IRQ_MSK);
        drv->rd_complete = true;

        if (drv->rd_complete && drv->wr_complete) {
            osal_semaphore_post(drv->transfer_complete);
        }
    }
}

static void wr_prefetch_isr(void *context)
{
    cca_driver_t *drv = (cca_driver_t *)context;

    uint32_t status = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET);
    if (status & PREFETCHER_INTERRUPT_STATUS_MSK) {
        /* Clear prefetcher IRQ flag (W1C) */
        cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET, PREFETCHER_INTERRUPT_STATUS_MSK);

        /* Clear dispatcher IRQ flag (W1C) */
        cca_reg_write(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET, DISPATCHER_IRQ_MSK);
        drv->wr_complete = true;

        if (drv->rd_complete && drv->wr_complete) {
            osal_semaphore_post(drv->transfer_complete);
        }
    }
}

/* =========================================================================
 * Private helpers
 * =========================================================================*/

/*
 * setup_bridge() 
 * Sets up the LW H2F bridge
 */
static int setup_bridge(void)
{
    if (disable_lwhps2fpga_bridge() != 0) {
        ERROR("Failed to disable LWHPS2FPGA bridge");
        return -1;
    }
    if (enable_lwhps2fpga_bridge() != 0) {
        ERROR("Failed to enable LWHPS2FPGA bridge");
        return -1;
    }
    return 0;
}

/*
 * verify_hw() 
 * Check hardware successfully programmed by checking CSR values
 */
static int verify_hw(void)
{
    uint32_t id = cca_reg_read(CCA_BASE, CCA_FPGA_ID);
    uint32_t rev = cca_reg_read(CCA_BASE, CCA_FPGA_REV);

    if (id != CCA_FPGA_ID_VAL) {
        ERROR("FPGA_ID mismatch: expected 0x%08x, got 0x%08x", CCA_FPGA_ID_VAL, (unsigned)id);
        return -1;
    }
    if (rev != CCA_FPGA_REV_VAL) {
        ERROR("FPGA_REV mismatch: expected 0x%08x, got 0x%08x", CCA_FPGA_REV_VAL, (unsigned)rev);
        return -1;
    }
    return 0;
}

/*
 * reset_accel() 
 * Sends reset to accel CSR and then waits for alive signal
 */
static int reset_accel(void)
{
    cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_RESET);

    uint32_t timeout = 100000;
    while (timeout--) {
        if (cca_reg_read(CCA_BASE, CCA_ACCEL_STATUS) & CCA_STATUS_ALIVE_FLAG)
            return 0;
    }

    ERROR("CCA accelerator did not assert ALIVE after reset");
    return -1;
}

/*
 * reset_msgdma_rd()
 * Resets the READ MSGDMA prefetcher and dispatcher, 
 * polling until hardware confirms each reset is complete.
 */
static int reset_msgdma_rd(void)
{
    uint32_t timeout;

    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    timeout = 100000;

    while ((cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && timeout--)
        ;
    if (cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) {
        ERROR("MSGDMA READ prefetcher reset timeout");
        return -1;
    }

    cca_reg_write(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    timeout = 100000;

    while ((cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && timeout--)
        ;
    if (cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) {
        ERROR("MSGDMA READ dispatcher reset timeout");
        return -1;
    }

    return 0;
}

/*
 * reset_msgdma_wr()
 * Identical to reset_msgdam_rd(), except it operates on the write channel
 */
static int reset_msgdma_wr(void)
{
    uint32_t timeout;

    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && timeout--)
        ;
    if (cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) {
        ERROR("MSGDMA WRITE prefetcher reset timeout");
        return -1;
    }

    cca_reg_write(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && timeout--)
        ;
    if (cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) {
        ERROR("MSGDMA WRITE dispatcher reset timeout");
        return -1;
    }

    return 0;
}

/* =========================================================================
 * Public API implementation
 * =========================================================================*/

/*
 * cca_init()
 * Startup process for initializing all hardware
 * Sets up the H2F bridge, verifies hardware via register probing, resets hardware
 * and sets up interrupts and sempahore for interrupts
 */
cca_status_t cca_init(cca_driver_t *drv)
{
    if (!drv)
        return CCA_ERR_ARGS;

    drv->rd_complete = false;
    drv->wr_complete = false;
    drv->transfer_complete = NULL;

    /* 1. Enable LWH2F bridge */
    if (setup_bridge() != 0)
        return CCA_ERR_BRIDGE;
#if DEBUG
    PRINT("  LWH2F bridge enabled");
#endif

    /* 2. Verify CCA FPGA identity registers */
    if (verify_hw() != 0)
        return CCA_ERR_HW_ID;
#if DEBUG
    PRINT("  CCA hardware verified (ID=0x%08x  REV=0x%08x)", (unsigned)cca_reg_read(CCA_BASE, CCA_FPGA_ID),
          (unsigned)cca_reg_read(CCA_BASE, CCA_FPGA_REV));
#endif

    /* 3. Reset the accelerator */
    if (reset_accel() != 0)
        return CCA_ERR_ACCEL_INIT;
#if DEBUG
    PRINT("  CCA accelerator alive after reset");
#endif

    /* 4. Reset mSGDMA READ and WRITE channels */
    if (reset_msgdma_rd() != 0)
        return CCA_ERR_DMA_INIT;
    if (reset_msgdma_wr() != 0)
        return CCA_ERR_DMA_INIT;
#if DEBUG
    PRINT("  MSGDMA channels reset");
#endif

    /* 5. Create binary semaphore for ISR task completion signalling */
    drv->transfer_complete = osal_semaphore_create(&drv->sem_mem);
    if (drv->transfer_complete == NULL) {
        ERROR("Failed to create transfer_complete semaphore");
        return CCA_ERR_ISR;
    }

    /* 6. Register ISR handlers and enable GIC lines */
    if (interrupt_register_isr(CCA_IRQ_MSGDMA_WRITE, wr_prefetch_isr, drv) != ERR_OK) {
        ERROR("Failed to register WR prefetcher ISR (IRQ%d)", CCA_IRQ_MSGDMA_WRITE);
        return CCA_ERR_ISR;
    }
    if (interrupt_enable(CCA_IRQ_MSGDMA_WRITE, GIC_INTERRUPT_PRIORITY_F2H) != ERR_OK) {
        ERROR("Failed to enable WR prefetcher interrupt (IRQ%d)", CCA_IRQ_MSGDMA_WRITE);
        return CCA_ERR_ISR;
    }
    if (interrupt_register_isr(CCA_IRQ_MSGDMA_READ, rd_prefetch_isr, drv) != ERR_OK) {
        ERROR("Failed to register RD prefetcher ISR (IRQ%d)", CCA_IRQ_MSGDMA_READ);
        return CCA_ERR_ISR;
    }
    if (interrupt_enable(CCA_IRQ_MSGDMA_READ, GIC_INTERRUPT_PRIORITY_F2H) != ERR_OK) {
        ERROR("Failed to enable RD prefetcher interrupt (IRQ%d)", CCA_IRQ_MSGDMA_READ);
        return CCA_ERR_ISR;
    }
#if DEBUG
    PRINT("  ISRs registered (WR=IRQ%d  RD=IRQ%d)", CCA_IRQ_MSGDMA_WRITE, CCA_IRQ_MSGDMA_READ);
#endif

    return CCA_OK;
}

/*
 * cca_transfer_submit()
 * Writes a chained descriptor set to OCM and starts both mSGDMA prefetchers.
 * Supports [1, CCA_MAX_JOBS] jobs in a single DMA pass.  Each job gets its own
 * RD and WR descriptor, mid-chain descriptors are linked via next_desc_ptr
 * and the last pair uses the end-of-chain control words.
 */
cca_status_t cca_transfer_submit(cca_driver_t *drv, const cca_job_t *jobs, uint32_t num_jobs, uint8_t accel_cmd)
{
    if (!drv || !jobs)
        return CCA_ERR_ARGS;
    if (num_jobs == 0 || num_jobs > CCA_MAX_JOBS)
        return CCA_ERR_ARGS;

    /* Validate every job up front before touching hardware */
    for (uint32_t i = 0; i < num_jobs; i++) {
        if (!jobs[i].src || !jobs[i].dst)
            return CCA_ERR_ARGS;
        if (jobs[i].size == 0 || (jobs[i].size & 31U))
            return CCA_ERR_ARGS;
        if ((uintptr_t)jobs[i].src & 4095U || (uintptr_t)jobs[i].dst & 4095U)
            return CCA_ERR_ARGS;
        if ((uintptr_t)jobs[i].src > UINT32_MAX || (uintptr_t)jobs[i].dst > UINT32_MAX)
            return CCA_ERR_ARGS;
    }

    // Unable to reset mSGDMA read channel
    if (reset_msgdma_rd() != 0)
        return CCA_ERR_DMA_INIT;
    // Unable to reset mSGDMA write channel
    if (reset_msgdma_wr() != 0)
        return CCA_ERR_DMA_INIT;

    /* Write READ and WRITE descriptor chains to OCM */
    for (uint32_t i = 0; i < num_jobs; i++) {
        uint32_t src_phys = (uint32_t)(uintptr_t)jobs[i].src;
        uint32_t dst_phys = (uint32_t)(uintptr_t)jobs[i].dst;
        bool last = (i == num_jobs - 1U);

        volatile msgdma_descriptor_t *rd =
            (volatile msgdma_descriptor_t *)(uintptr_t)(OCM_BASE + i * sizeof(msgdma_descriptor_t));
        rd->src_addr = src_phys;
        rd->dest_addr = 0U;
        rd->length = jobs[i].size;
        rd->next_desc_ptr = last ? 0U : (OCM_BASE_MSGDMA + (i + 1U) * sizeof(msgdma_descriptor_t));
        rd->reserved[0] = 0U;
        rd->reserved[1] = 0U;
        rd->reserved[2] = 0U;
        rd->control = last ? DESC_CTRL_LAST_CHAIN_RD : DESC_CTRL_MID_CHAIN;

        volatile msgdma_descriptor_t *wr = (volatile msgdma_descriptor_t *)(uintptr_t)(OCM_BASE + WRITE_DESC_BASE_OFF +
                                                                                       i * sizeof(msgdma_descriptor_t));
        wr->src_addr = 0U;
        wr->dest_addr = dst_phys;
        wr->length = jobs[i].size;
        wr->next_desc_ptr = last ? 0U :
                                   (OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF + (i + 1U) * sizeof(msgdma_descriptor_t));
        wr->reserved[0] = 0U;
        wr->reserved[1] = 0U;
        wr->reserved[2] = 0U;
        wr->control = last ? DESC_CTRL_LAST_CHAIN_WR : DESC_CTRL_MID_CHAIN;
    }

    __asm__ volatile("dsb sy" ::: "memory");

    /* Write the accelerator command (START | cmd) */
    cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_START | (accel_cmd & CCA_CTRL_CMD_MASK));

    /*
     * Reset semaphore in case a timeout occurred previously
     */
    osal_semaphore_reset(drv->transfer_complete);

    /* Clear completion flags before starting prefetchers */
    drv->rd_complete = false;
    drv->wr_complete = false;

    /* Point each prefetcher at the first descriptor in its chain */
    // Read prefetcher
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA);
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0U);

    // Write prefetcher
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF);
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0U);

    __asm__ volatile("dsb sy" ::: "memory");

    /* Start both prefetchers with IRQ enabled */
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RUN_MSK | PREFETCHER_IRQ_EN_MSK);
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RUN_MSK | PREFETCHER_IRQ_EN_MSK);

    return CCA_OK;
}

/*
 * cca_transfer_wait()
 * Blocks the calling task on the transfer_complete binary semaphore until
 * both MSGDMA prefetcher ISRs have fired (interrupt-driven).  The ISRs set
 * rd_complete / wr_complete and post the semaphore once both are true.
 */
cca_status_t cca_transfer_wait(cca_driver_t *drv, uint32_t timeout_ms)
{
    if (!drv)
        return CCA_ERR_ARGS;

    if (!osal_semaphore_wait(drv->transfer_complete, (uint64_t)timeout_ms)) {
        ERROR("DMA transfer timed out after %u ms", (unsigned)timeout_ms);
        return CCA_ERR_TIMEOUT;
    }

    /* Check accelerator status for errors */
    uint32_t accel_status = cca_reg_read(CCA_BASE, CCA_ACCEL_STATUS);
    if (accel_status & CCA_STATUS_ACCEL_ERROR) {
        ERROR("CCA accelerator error, ACCEL_STATUS=0x%08x", (unsigned)accel_status);
        return CCA_ERR_ACCEL;
    }

    return CCA_OK;
}

/*
 * cca_reset()
 * Fully resets the hardware by resetting the accelerator and the DMA channels
 */
cca_status_t cca_reset(cca_driver_t *drv)
{
    if (!drv)
        return CCA_ERR_ARGS;

    drv->rd_complete = false;
    drv->wr_complete = false;

    // Unable to reset accelerator
    if (reset_accel() != 0)
        return CCA_ERR_ACCEL_INIT;
    // Unable to reset mSGDMA read channel
    if (reset_msgdma_rd() != 0)
        return CCA_ERR_DMA_INIT;
    // Unable to reset mSGDMA write channel
    if (reset_msgdma_wr() != 0)
        return CCA_ERR_DMA_INIT;

    return CCA_OK;
}
