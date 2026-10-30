/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * LWH2F Task for SoC FPGA Cache Coherent Accelerator Design Example
 * Lightweight HPS-to-FPGA
 *
 * Accesses CCA subsystem CSR registers via the LWH2F bridge.
 * Performs hardware initialization, register validation,
 * and accelerator reset before DMA transfers begin.
 *
 * Bridge path: HPS CPU --> LWH2F bridge --> CCA subsystem CSRs
 *
 * Note: LWH2F is NOT cache coherent (no CCU on this path).
 *       CSR register access is MMIO — bypasses CPU cache entirely.
 */
#include <stdio.h> // printf()
#include <unistd.h> // usleep()
#include <stdlib.h> // malloc(), free()
#include <string.h> // memcpy()
#include <stdint.h> // uint32_t, uint64_t, etc.
#include "sys/alt_irq.h"
#include "cca_hal.h"
#include "cca_tasks.h"

//=============================================================================
// ISR Completion State (shared with cca_f2h_task.c via extern)
//=============================================================================

// Binary semaphore: WRITE ISR signals, task waits
// replacing semaphores for now with simple volatile flag for demo purposes

// osal_semaphore_def_t transfer_complete_mem;
// osal_semaphore_t     transfer_complete = NULL;
volatile bool transfer_complete = false;

// Per-channel completion flags (set independently by each ISR)
volatile bool rd_complete = false;
volatile bool wr_complete = false;

//=============================================================================
// ISR Handlers
//=============================================================================

/**
 * cca_rd_prefetch_isr()
 * Handles hw notification that the READ DMA channel has finished
 * feeding data to the accelerator.
 *
 * Lightweight: read status, clear IRQ bit (W1C), set flag, exit.
 * Does NOT signal semaphore — WRITE ISR is responsible for that.
 */
void cca_rd_prefetch_isr(void *context)
{
    (void)context; // unused — required by alt_isr_func signature

    uint32_t status = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET);

    if (status & PREFETCHER_INTERRUPT_STATUS_MSK) {
        // Clear IRQ bit (write-1-to-clear)
        cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET, PREFETCHER_INTERRUPT_STATUS_MSK);
        // Set completion flag
        rd_complete = true;
        // Exit — WRITE ISR handles semaphore signaling
    }
}

/**
 * cca_wr_prefetch_isr()
 * Handles hw notification that WRITE DMA channel has finished writing
 * results to DDR; if both channels done, signals CPU that transfer is complete.
 *
 * Full: read status, clear IRQ bit (W1C), set flag, check both channels,
 * signal semaphore if both done, request context switch.
 */
void cca_wr_prefetch_isr(void *context)
{
    (void)context; // unused — required by alt_isr_func signature

    uint32_t status = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET);

    if (status & PREFETCHER_INTERRUPT_STATUS_MSK) {
        // Clear IRQ bit (write-1-to-clear)
        cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET, PREFETCHER_INTERRUPT_STATUS_MSK);
        // Set completion flag
        wr_complete = true;
        // Check if BOTH channels are complete
        if (rd_complete && wr_complete) {
            // OSAL automatically calls xSemaphoreGiveFromISR +
            // portYIELD_FROM_ISR (context switch if higher priority task woken)
            // replacing semaphores for now with simple volatile flag for demo purposes
            transfer_complete = true;
        }
    }
}

//=============================================================================
// Private Helper: Register Read/Verify
//=============================================================================
static int check_reg(const char *name, uint32_t base, uint32_t offset, uint32_t expected)
{
    uint32_t val = cca_reg_read(base, offset);
    if (val != expected) {
        ERROR("  FAIL %s: got 0x%08x, expected 0x%08x", name, (unsigned)val, (unsigned)expected);
        return -1;
    }
    PRINT("  PASS %s = 0x%08x", name, (unsigned)val);
    return 0;
}

//=============================================================================
// Function 1: setup_lwhps2fpga_bridge()
// Enable LWH2F bridge for CSR access to CCA subsystem
//=============================================================================
static int setup_lwhps2fpga_bridge(void)
{
    PRINT("\n--- LWH2F Bridge Setup --- (no-op on Nios V)");
    // only for FreeRTOS on HPS; Nios V has bridge always enabled for MMIO access to CCA CSRs
    return 0;
}

//=============================================================================
// Function 2: cca_verify_id_regs()
// Read and verify CCA FPGA ID, Revision, and Reserved registers
//=============================================================================
static int cca_verify_id_regs(void)
{
    int ret = 0;

    PRINT("\n--- CCA Identity Registers ---");

    ret |= check_reg("CCA_FPGA_ID", CCA_BASE, CCA_FPGA_ID, CCA_FPGA_ID_VAL);
    ret |= check_reg("CCA_FPGA_REV", CCA_BASE, CCA_FPGA_REV, CCA_FPGA_REV_VAL);

    if (ret != 0) {
        ERROR("CCA identity check failed — is FPGA programmed?");
        return ret;
    }

    PRINT("\n--- CCA Reserved Registers ---");

    ret |= check_reg("CCA_RESERVED_08", CCA_BASE, CCA_RESERVED_08, CCA_RESERVED_08_VAL);
    ret |= check_reg("CCA_RESERVED_0C", CCA_BASE, CCA_RESERVED_0C, CCA_RESERVED_0C_VAL);
    ret |= check_reg("CCA_RESERVED_20", CCA_BASE, CCA_RESERVED_20, CCA_RESERVED_20_VAL);
    ret |= check_reg("CCA_RESERVED_24", CCA_BASE, CCA_RESERVED_24, CCA_RESERVED_24_VAL);
    ret |= check_reg("CCA_RESERVED_28", CCA_BASE, CCA_RESERVED_28, CCA_RESERVED_28_VAL);
    ret |= check_reg("CCA_RESERVED_2C", CCA_BASE, CCA_RESERVED_2C, CCA_RESERVED_2C_VAL);
    ret |= check_reg("CCA_RESERVED_30", CCA_BASE, CCA_RESERVED_30, CCA_RESERVED_30_VAL);
    ret |= check_reg("CCA_RESERVED_34", CCA_BASE, CCA_RESERVED_34, CCA_RESERVED_34_VAL);
    ret |= check_reg("CCA_RESERVED_38", CCA_BASE, CCA_RESERVED_38, CCA_RESERVED_38_VAL);
    ret |= check_reg("CCA_RESERVED_3C", CCA_BASE, CCA_RESERVED_3C, CCA_RESERVED_3C_VAL);

    if (ret != 0) {
        ERROR("Reserved register check failed");
    }

    return ret;
}

//=============================================================================
// Function 3: cca_debug_reg_test()
// Write known patterns to debug registers, then read back and verify
//=============================================================================
static int cca_debug_reg_test(void)
{
    int ret = 0;

    PRINT("\n--- CCA Debug Register Write/Read Test ---");

    // Write test patterns to debug registers
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_0);
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_1);
    cca_reg_write(CCA_BASE, CCA_DBG_REG2, DBG_TST_VAL_2);
    cca_reg_write(CCA_BASE, CCA_DBG_REG3, DBG_TST_VAL_3);
    cca_reg_write(CCA_BASE, CCA_DBG_REG4, DBG_TST_VAL_4);
    cca_reg_write(CCA_BASE, CCA_DBG_REG5, DBG_TST_VAL_5);
    cca_reg_write(CCA_BASE, CCA_DBG_REG6, DBG_TST_VAL_6);
    cca_reg_write(CCA_BASE, CCA_DBG_REG7, DBG_TST_VAL_7);
    cca_reg_write(CCA_BASE, CCA_DBG_REG8, DBG_TST_VAL_8);
    cca_reg_write(CCA_BASE, CCA_DBG_REG9, DBG_TST_VAL_9);
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, DBG_TST_VAL_10);

    // Read back and verify
    ret |= check_reg("CCA_DBG_REG0", CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_0);
    ret |= check_reg("CCA_DBG_REG1", CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_1);
    ret |= check_reg("CCA_DBG_REG2", CCA_BASE, CCA_DBG_REG2, DBG_TST_VAL_2);
    ret |= check_reg("CCA_DBG_REG3", CCA_BASE, CCA_DBG_REG3, DBG_TST_VAL_3);
    ret |= check_reg("CCA_DBG_REG4", CCA_BASE, CCA_DBG_REG4, DBG_TST_VAL_4);
    ret |= check_reg("CCA_DBG_REG5", CCA_BASE, CCA_DBG_REG5, DBG_TST_VAL_5);
    ret |= check_reg("CCA_DBG_REG6", CCA_BASE, CCA_DBG_REG6, DBG_TST_VAL_6);
    ret |= check_reg("CCA_DBG_REG7", CCA_BASE, CCA_DBG_REG7, DBG_TST_VAL_7);
    ret |= check_reg("CCA_DBG_REG8", CCA_BASE, CCA_DBG_REG8, DBG_TST_VAL_8);
    ret |= check_reg("CCA_DBG_REG9", CCA_BASE, CCA_DBG_REG9, DBG_TST_VAL_9);
    ret |= check_reg("CCA_DBG_REG10", CCA_BASE, CCA_DBG_REG10, DBG_TST_VAL_10);

    if (ret != 0) {
        ERROR("Debug register test failed");
    }

    return ret;
}

//=============================================================================
// Function 4: cca_accel_init()
// Reset CCA accelerator and wait for ALIVE flag
//=============================================================================
static int cca_accel_init(void)
{
    PRINT("\n--- CCA Accelerator Init ---");

    // Assert reset
    cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_RESET);

    // Wait until ALIVE flag asserts after reset
    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t status = cca_reg_read(CCA_BASE, CCA_ACCEL_STATUS);
        if (status & CCA_STATUS_ALIVE_FLAG) {
            break;
        }
    }

    if (timeout == 0) {
        ERROR("CCA accelerator did not assert ALIVE after reset");
        return -1;
    }

    PRINT("CCA accelerator reset complete (ALIVE flag asserted)");
    return 0;
}

//=============================================================================
// Function 5: msgdma_rd_init()
// Reset MSGDMA READ prefetcher and dispatcher
//=============================================================================
static int msgdma_rd_init(void)
{
    PRINT("\n--- MSGDMA READ Channel Init ---");

    uint32_t timeout;

    // Reset READ prefetcher
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && timeout--)
        ;
    if (timeout == 0) {
        ERROR("MSGDMA READ prefetcher reset timeout");
        return -1;
    }
    PRINT("MSGDMA READ prefetcher reset complete");

    // Reset READ dispatcher
    cca_reg_write(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && timeout--)
        ;
    if (timeout == 0) {
        ERROR("MSGDMA READ dispatcher reset timeout");
        return -1;
    }
    PRINT("MSGDMA READ dispatcher reset complete");

    return 0;
}

//=============================================================================
// Function 6: msgdma_wr_init()
// Reset MSGDMA WRITE prefetcher and dispatcher
//=============================================================================
static int msgdma_wr_init(void)
{
    PRINT("\n--- MSGDMA WRITE Channel Init ---");

    uint32_t timeout;

    // Reset WRITE prefetcher
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && timeout--)
        ;
    if (timeout == 0) {
        ERROR("MSGDMA WRITE prefetcher reset timeout");
        return -1;
    }
    PRINT("MSGDMA WRITE prefetcher reset complete");

    // Reset WRITE dispatcher
    cca_reg_write(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && timeout--)
        ;
    if (timeout == 0) {
        ERROR("MSGDMA WRITE dispatcher reset timeout");
        return -1;
    }
    PRINT("MSGDMA WRITE dispatcher reset complete");

    return 0;
}

//=============================================================================
// Function 7: cca_isr_register()
// Create binary semaphore and register ISR handlers for both channels
//=============================================================================
static int cca_isr_register(void)
{
    PRINT("\n--- ISR Registration ---");

    // Create binary semaphore for ISR → task completion signaling

    // Reset transfer_complete flag (volatile bool, no RTOS semaphore needed)
    transfer_complete = false;
    PRINT("transfer_complete flag initialized");

    PRINT("Binary semaphore created: transfer_complete");

    // Register WRITE prefetcher ISR (F2H IRQ0)
    // WRITE ISR checks both channels and signals semaphore when both done
    // for FreeRTOS on HPS, use osal interrupt_register
    // interrupt_register(CCA_IRQ_MSGDMA_WRITE, cca_wr_prefetch_isr);
    // interrupt_enable(CCA_IRQ_MSGDMA_WRITE);
    // for Nios V, use alt_ic_isr_register (enhanced interrupt controller API)
    alt_ic_isr_register(0, CCA_IRQ_MSGDMA_WRITE, cca_wr_prefetch_isr, NULL, NULL);
    PRINT("Registered cca_wr_prefetch_isr on IRQ%d", CCA_IRQ_MSGDMA_WRITE);

    // Register READ prefetcher ISR (F2H IRQ1)
    // READ ISR is lightweight: clears IRQ, sets flag, exits
    // for FreeRTOS on HPS, use osal interrupt_register
    // interrupt_register(CCA_IRQ_MSGDMA_READ, cca_rd_prefetch_isr);
    // interrupt_enable(CCA_IRQ_MSGDMA_READ);
    // for Nios V, use alt_ic_isr_register (enhanced interrupt controller API)
    alt_ic_isr_register(0, CCA_IRQ_MSGDMA_READ, cca_rd_prefetch_isr, NULL, NULL);
    PRINT("Registered cca_rd_prefetch_isr on IRQ%d", CCA_IRQ_MSGDMA_READ);

    return 0;
}

//=============================================================================
// Public: lwh2f_task()
// Calls each init function in sequence
//=============================================================================
void lwh2f_task(uint32_t transaction_seconds)
{
    (void)transaction_seconds; // Not used: init runs once, not time-bounded

    PRINT("========================================");
    PRINT(" LWH2F Task: CCA Initialization");
    PRINT("========================================");

    // 1. Enable LWH2F bridge
    if (setup_lwhps2fpga_bridge() != 0) {
        return;
    }

    // 2. Verify CCA ID, Revision and Reserved registers
    if (cca_verify_id_regs() != 0) {
        return;
    }

    // 3. Debug register write/read test
    if (cca_debug_reg_test() != 0) {
        return;
    }

    // 4. Reset CCA accelerator, wait for ALIVE flag
    if (cca_accel_init() != 0) {
        return;
    }

    // 5. Reset MSGDMA READ  prefetcher + dispatcher
    if (msgdma_rd_init() != 0) {
        return;
    }

    // 6. Reset MSGDMA WRITE prefetcher + dispatcher
    if (msgdma_wr_init() != 0) {
        return;
    }

    // 7. Create semaphore and register ISR handlers
    if (cca_isr_register() != 0) {
        return;
    }

    PRINT("\n========================================");
    PRINT(" LWH2F Task COMPLETE — CCA initialized");
    PRINT("   Bridge:      ENABLED");
    PRINT("   Registers:   VERIFIED");
    PRINT("   Accelerator: ALIVE");
    PRINT("   MSGDMA:      RESET");
    PRINT("   Semaphore:   CREATED");
    PRINT("   ISRs:        REGISTERED");
    PRINT("========================================\n");
}