/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * F2H Task for SoC FPGA Cache Coherent Accelerator Design Example
 *
 * Configures msgDMA descriptor chains and executes CCA DMA transfers.
 * Data path: HPS DRAM --> msgDMA READ --> CCA Accelerator --> msgDMA WRITE --> HPS DRAM
 *
 * Cache coherency path (data buffers):
 *   msgDMA --> AvMMU --> AXI4 --> ACE5-Lite --> CCU --> HPS DRAM
 *   Hardware cache coherency: CCU snoops A55 L1/L2 caches automatically.
 *   Defensive cache operations included per Intel best practices.
 *
 * Descriptor path (OCM via LWH2F — non-coherent MMIO):
 *   HPS writes descriptors to OCM_BASE via LWH2F bridge.
 *   MMIO writes bypass CPU cache — no cache flush needed for descriptors.
 */
#include <stdio.h> // printf()
#include <unistd.h> // usleep()
#include <stdlib.h> // malloc(), free()
#include <string.h> // memcpy()
#include <stdint.h> // uint32_t, uint64_t, etc.
// for Nios V, use alt_cache.h for cache operations;
#include "sys/alt_irq.h"
#include "sys/alt_cache.h"
#include "cca_hal.h"
#include "cca_tasks.h"
//=============================================================================
// Externals (defined in cca_lwh2f_task.c)
//=============================================================================
// replacing semaphores for now with simple volatile flag for demo purposes
extern volatile bool transfer_complete;

// Per-channel completion flags (set independently by each ISR)
extern volatile bool rd_complete;
extern volatile bool wr_complete;

//=============================================================================
// Function 1: alloc_dma_buffers()
// Allocate src and dst DMA buffers
// Nios V sim: fixed FPGA RAM addresses from cca_hal.h (CPU view != MSGDMA view)
// HPS FreeRTOS: 4KB-aligned dynamic allocation from HPS DRAM (identity mapping)
//=============================================================================
static int alloc_dma_buffers(volatile uint32_t **src, volatile uint32_t **dst, uint32_t size)
{
    PRINT("\n--- DMA Buffer Allocation ---");

    // Nios V C code: use fixed FPGA RAM addresses (no dynamic allocation)
    *src = (volatile uint32_t *)(uintptr_t)DMA_READ_BUF;
    *dst = (volatile uint32_t *)(uintptr_t)DMA_WRITE_BUF;

    // Nios V C code
    PRINT("Source buffer:      virt=0x%08lx msgdma=0x%08lx", (unsigned long)DMA_READ_BUF,
          (unsigned long)DMA_READ_BUF_MSGDMA);
    PRINT("Destination buffer: virt=0x%08lx msgdma=0x%08lx", (unsigned long)DMA_WRITE_BUF,
          (unsigned long)DMA_WRITE_BUF_MSGDMA);

    return 0;
}

//=============================================================================
// Function 2: init_src_buffer()
// Fill source buffer with test pattern, verify readback, issue memory barrier
// Pattern: 0xA0..AF (iter 0-15), 0xB0..BF (iter 16-31) + word index
//=============================================================================
static int init_src_buffer(volatile uint32_t *src, uint32_t size, uint32_t iter)
{
    PRINT("\n--- Source Buffer Initialization (iter %lu) ---", (unsigned long)iter);

    uint32_t num_words = size / sizeof(uint32_t);
    uint32_t pattern_base;

    // A0-AF for iterations 0-15, B0-BF for iterations 16-31
    if (iter < 16) {
        pattern_base = (0xA0 + iter) << 24;
    } else if (iter < 32) {
        pattern_base = (0xB0 + (iter - 16)) << 24;
    } else {
        pattern_base = (0xC0 + (iter - 32)) << 24; // Extend if needed
    }

    PRINT("DEBUG: src=%p size=%lu num_words=%lu iter=%lu pattern_base=0x%08lx", (void *)src, (unsigned long)size,
          (unsigned long)num_words, (unsigned long)iter, (unsigned long)pattern_base);

    for (uint32_t i = 0; i < num_words; i++) {
        src[i] = pattern_base | i;
    }

    // Ensure all CPU writes complete before readback
    // Nios V C code
    __asm__ volatile("fence" ::: "memory");

    // Fast readback verification: check 6 key locations first
    // If all pass, we're good. If any fail, do full scan.
    uint32_t quick_check_indices[6] = { 0, 1, num_words / 2 - 1, num_words / 2, num_words - 2, num_words - 1 };
    bool quick_check_passed = true;

    for (int j = 0; j < 6; j++) {
        uint32_t i = quick_check_indices[j];
        uint32_t expected = pattern_base | i;
        if (src[i] != expected) {
            PRINT("  Quick check FAILED at [%lu]: got=0x%08lx expected=0x%08lx", (unsigned long)i,
                  (unsigned long)src[i], (unsigned long)expected);
            quick_check_passed = false;
            break;
        }
    }

    if (quick_check_passed) {
        PRINT("Source buffer quick check passed (6/6 key locations verified)");
    } else {
        // Quick check failed, do full verification
        PRINT("Running full verification...");
        uint32_t errors = 0;
        for (uint32_t i = 0; i < num_words; i++) {
            uint32_t expected = pattern_base | i;
            if (src[i] != expected) {
                if (errors < 10) {
                    ERROR("  SRC BUF FAIL [%lu]: got=0x%08lx expected=0x%08lx", (unsigned long)i, (unsigned long)src[i],
                          (unsigned long)expected);
                }
                errors++;
            }
        }
        if (errors > 0) {
            ERROR("Source buffer verification FAILED: %lu errors out of %lu words", (unsigned long)errors,
                  (unsigned long)num_words);
            return -1;
        }
    }

    PRINT("Source buffer initialized and verified: pattern=0x%08lx .. 0x%08lx", (unsigned long)src[0],
          (unsigned long)src[num_words - 1]);
    return 0;
}

//=============================================================================
// Function 3: init_dst_buffer()
// Fill destination buffer with sentinel 0x01010101 and verify readback
// If HPS reads back 0x01010101 after transfer, DDR is readable but CCA did
// not update it — useful to distinguish a connectivity issue from a logic issue
//=============================================================================
static int init_dst_buffer(volatile uint32_t *dst, uint32_t size)
{
    PRINT("\n--- Destination Buffer Initialization ---");

    uint32_t num_words = size / sizeof(uint32_t);

    for (uint32_t i = 0; i < num_words; i++) {
        dst[i] = 0x01010101;
    }

    // Ensure all CPU writes complete before readback
    // Nios V C code
    __asm__ volatile("fence" ::: "memory");

    // Fast readback verification: check 6 key locations first
    // If all pass, we're good. If any fail, do full scan.
    uint32_t quick_check_indices[6] = { 0, 1, num_words / 2 - 1, num_words / 2, num_words - 2, num_words - 1 };
    bool quick_check_passed = true;

    for (int j = 0; j < 6; j++) {
        uint32_t i = quick_check_indices[j];
        if (dst[i] != 0x01010101) {
            PRINT("  Quick check FAILED at [%lu]: got=0x%08lx expected=0x01010101", (unsigned long)i,
                  (unsigned long)dst[i]);
            quick_check_passed = false;
            break;
        }
    }

    if (quick_check_passed) {
        PRINT("Destination buffer quick check passed (6/6 key locations verified)");
    } else {
        // Quick check failed, do full verification
        PRINT("Running full verification...");
        uint32_t errors = 0;
        for (uint32_t i = 0; i < num_words; i++) {
            if (dst[i] != 0x01010101) {
                if (errors < 10) {
                    ERROR("  DST BUF FAIL [%lu]: got=0x%08lx expected=0x01010101", (unsigned long)i,
                          (unsigned long)dst[i]);
                }
                errors++;
            }
        }
        if (errors > 0) {
            ERROR("Destination buffer verification FAILED: %lu errors out of %lu words", (unsigned long)errors,
                  (unsigned long)num_words);
            return -1;
        }
    }

    PRINT("Destination buffer initialized and verified: %lu words = 0x01010101", (unsigned long)num_words);
    return 0;
}

//=============================================================================
// Function 4: setup_read_descriptors()
// Write NUM_OF_DESCRIPTORS READ descriptors into OCM via LWH2F (MMIO)
// Each descriptor reads one block from src (HPS DRAM) to accelerator input
//
// READ  chain: OCM_BASE_MSGDMA + 0
// WRITE chain: OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF (256 bytes offset)
//=============================================================================
static int setup_read_descriptors(uint32_t src_phys, uint32_t size, bool verbose)
{
    // CPU writes descriptors to OCM_BASE (Nios V view), but next_desc_ptr uses OCM_BASE_MSGDMA (MSGDMA view)
    volatile msgdma_descriptor_t *desc = (volatile msgdma_descriptor_t *)(uintptr_t)(OCM_BASE);

    uint32_t block_size = size / NUM_OF_DESCRIPTORS;

    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        desc[i].src_addr = src_phys + (i * block_size); // Source: HPS DRAM
        desc[i].dest_addr = 0; // N/A (goes to accelerator)
        desc[i].length = block_size;
        desc[i].reserved[0] = 0; // Must be zero (MSGDMA spec)
        desc[i].reserved[1] = 0;
        desc[i].reserved[2] = 0;

        if (i < NUM_OF_DESCRIPTORS - 1) {
            // Mid-chain: owned by HW, early done to pipeline next descriptor
            desc[i].next_desc_ptr = OCM_BASE_MSGDMA + ((i + 1) * sizeof(msgdma_descriptor_t));
            desc[i].control = DESC_CTRL_MID_CHAIN;
        } else {
            // Last: MM2ST — no write response, assert IRQ immediately on chain done
            desc[i].next_desc_ptr = 0;
            desc[i].control = DESC_CTRL_LAST_CHAIN_RD;
        }
    }
    // Note: written via MMIO (LWH2F) — no cache flush needed for descriptors

    // Readback validation: walk chain and verify each descriptor field (also tests OCM via LWH2F)
    int errors = 0;
    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        uint32_t exp_src = src_phys + (i * block_size);
        uint32_t exp_next = (i < NUM_OF_DESCRIPTORS - 1) ? (OCM_BASE_MSGDMA + ((i + 1) * sizeof(msgdma_descriptor_t))) :
                                                           0;
        uint32_t exp_ctrl = (i < NUM_OF_DESCRIPTORS - 1) ? DESC_CTRL_MID_CHAIN : DESC_CTRL_LAST_CHAIN_RD;

        if (desc[i].src_addr != exp_src || desc[i].dest_addr != 0 || desc[i].length != block_size ||
            desc[i].next_desc_ptr != exp_next || desc[i].control != exp_ctrl) {
            // Nios V C code
            ERROR("  RD DESC[%d] FAIL: src=0x%08lx dst=0x%08lx len=%lu next=0x%08lx ctrl=0x%08lx", i,
                  (unsigned long)desc[i].src_addr, (unsigned long)desc[i].dest_addr, (unsigned long)desc[i].length,
                  (unsigned long)desc[i].next_desc_ptr, (unsigned long)desc[i].control);
            errors++;
        } //
    }

    if (errors > 0) {
        ERROR("READ descriptor validation FAILED (%d errors)", errors);
        return -1;
    }

    PRINT("READ descriptors written and validated (%d descriptors)", NUM_OF_DESCRIPTORS);
    return 0;
}

//=============================================================================
// Function 5: setup_write_descriptors()
// Write NUM_OF_DESCRIPTORS WRITE descriptors into OCM via LWH2F (MMIO)
// Each descriptor writes one block from accelerator output to dst (HPS DRAM)
//=============================================================================
static int setup_write_descriptors(uint32_t dst_phys, uint32_t size, bool verbose)
{
    // CPU writes descriptors to OCM_BASE (Nios V view), but next_desc_ptr uses OCM_BASE_MSGDMA (MSGDMA view)
    volatile msgdma_descriptor_t *desc = (volatile msgdma_descriptor_t *)(uintptr_t)(OCM_BASE + WRITE_DESC_BASE_OFF);

    uint32_t block_size = size / NUM_OF_DESCRIPTORS;

    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        desc[i].src_addr = 0; // N/A (comes from accelerator)
        desc[i].dest_addr = dst_phys + (i * block_size); // Destination: HPS DRAM
        desc[i].length = block_size;
        desc[i].reserved[0] = 0;
        desc[i].reserved[1] = 0;
        desc[i].reserved[2] = 0;

        if (i < NUM_OF_DESCRIPTORS - 1) {
            // Mid-chain: owned by HW, early done
            desc[i].next_desc_ptr = OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF + ((i + 1) * sizeof(msgdma_descriptor_t));
            desc[i].control = DESC_CTRL_MID_CHAIN;
        } else {
            // Last: wait for write channel, assert IRQ on completion
            desc[i].next_desc_ptr = 0;
            desc[i].control = DESC_CTRL_LAST_CHAIN;
        }
    }
    // Note: written via MMIO (LWH2F) — no cache flush needed for descriptors

    // Readback validation: walk chain and verify each descriptor field (also tests OCM via LWH2F)
    int errors = 0;
    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        uint32_t exp_dst = dst_phys + (i * block_size);
        uint32_t exp_next = (i < NUM_OF_DESCRIPTORS - 1) ?
                                (OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF + ((i + 1) * sizeof(msgdma_descriptor_t))) :
                                0;
        uint32_t exp_ctrl = (i < NUM_OF_DESCRIPTORS - 1) ? DESC_CTRL_MID_CHAIN : DESC_CTRL_LAST_CHAIN;

        if (desc[i].src_addr != 0 || desc[i].dest_addr != exp_dst || desc[i].length != block_size ||
            desc[i].next_desc_ptr != exp_next || desc[i].control != exp_ctrl) {
            // Nios V C code
            ERROR("  WR DESC[%d] FAIL: src=0x%08lx dst=0x%08lx len=%lu next=0x%08lx ctrl=0x%08lx", i,
                  (unsigned long)desc[i].src_addr, (unsigned long)desc[i].dest_addr, (unsigned long)desc[i].length,
                  (unsigned long)desc[i].next_desc_ptr, (unsigned long)desc[i].control);
            errors++;
        } //
    }

    if (errors > 0) {
        ERROR("WRITE descriptor validation FAILED (%d errors)", errors);
        return -1;
    }

    PRINT("WRITE descriptors written and validated (%d descriptors)", NUM_OF_DESCRIPTORS);
    return 0;
}

//=============================================================================
// Function 5: cca_transfer_config()
// Cache ops + descriptor setup + accelerator command; does NOT start DMA
//
// Steps:
//   1. Flush src cache    (clean dirty lines to DDR before DMA reads)
//   2. Invalidate dst cache (discard stale lines before DMA writes)
//   3. Write READ  descriptor chain to OCM (MMIO)
//   4. Write WRITE descriptor chain to OCM (MMIO)
//   5. Write accelerator command register (START | cmd)
//=============================================================================
static int cca_transfer_config(uint32_t src_phys, void *src_virt, uint32_t dst_phys, void *dst_virt, uint32_t size,
                               uint8_t accel_cmd, bool verbose)
{
    // Flush src (ensure CPU writes are visible in DDR before DMA reads)
    // for Nios V (alt_cache.h is already included at top):
    alt_dcache_flush(src_virt, size);
    __asm__ volatile("fence" ::: "memory");

    // Invalidate dst (discard stale cache lines before DMA writes)
    // for Nios V (alt_cache.h is already included at top):
    alt_dcache_flush_no_writeback(dst_virt, size);
    __asm__ volatile("fence" ::: "memory");

    // Write READ + WRITE descriptor chains to OCM (MMIO — no cache flush)
    if (setup_read_descriptors(src_phys, size, verbose) != 0)
        return -1;
    if (setup_write_descriptors(dst_phys, size, verbose) != 0)
        return -1;

    // Configure accelerator command (PASS_THROUGH or PROCESS_DATA)
    cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_START | (accel_cmd & CCA_CTRL_CMD_MASK));

    // Nios V C code
    PRINT("CCA: configured — src=0x%08lx dst=0x%08lx size=%lu cmd=0x%02x", (unsigned long)src_phys,
          (unsigned long)dst_phys, (unsigned long)size, accel_cmd);
    return 0;
}

//=============================================================================
// Function 6: cca_transfer_submit()
// Start pre-configured DMA transfer; returns immediately (non-blocking)
//
// Steps:
//   1. Reset rd_complete / wr_complete flags
//   2. Point READ  prefetcher NEXT_DESC_LO to first READ  descriptor
//   3. Point WRITE prefetcher NEXT_DESC_LO to first WRITE descriptor
//   4. Start READ  prefetcher (RUN | IRQ_EN)
//   5. Start WRITE prefetcher (RUN | IRQ_EN)
//=============================================================================
static void cca_transfer_submit(void)
{
    // Reset completion flags BEFORE starting prefetchers
    rd_complete = false;
    wr_complete = false;

    //=========================================================
    // Reset prefetchers + dispatchers before each transfer
    // Required: dispatcher stays in STOPPED_ON_ERR after
    // end-of-chain — must reset before accepting new chain
    //=========================================================

    uint32_t timeout;

    // Reset READ prefetcher
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && timeout--)
        ;

    // Reset WRITE prefetcher
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && timeout--)
        ;

    // Reset READ dispatcher — clears STOPPED_ON_ERR
    cca_reg_write(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && timeout--)
        ;

    // Reset WRITE dispatcher — clears STOPPED_ON_ERR
    cca_reg_write(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    timeout = 100000;
    while ((cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && timeout--)
        ;

    // Point READ prefetcher to first READ descriptor in OCM
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA);
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0);

    // Point WRITE prefetcher to first WRITE descriptor in OCM
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF);
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0);

    // Start READ  prefetcher (RUN | IRQ_EN)
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RUN_MSK | PREFETCHER_IRQ_EN_MSK);

    // Start WRITE prefetcher (RUN | IRQ_EN)
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RUN_MSK | PREFETCHER_IRQ_EN_MSK);

    PRINT("CCA: transfer submitted (RUNNING)");
}

//=============================================================================
// Function 7: cca_transfer_wait()
// Poll prefetcher IRQ status bits until both READ and WRITE chains complete
//
// Steps:
//   1. Poll MSGDMA_PF_STATUS_OFFSET IRQ bit on both prefetchers (no ISR needed)
//   2. On timeout: dump status registers, return -1
//   3. On success:
//      a. Check READ  dispatcher status (error bits)
//      b. Check WRITE dispatcher status (error bits)
//      c. Check accelerator status
//      d. Invalidate dst cache (CPU reads fresh data from DDR)
//      e. Return 0
//
// freeRTOS C code: replace polling loop with osal_semaphore_wait(transfer_complete)
//=============================================================================
static int cca_transfer_wait(uint32_t timeout_ms, void *dst_virt, uint32_t size)
{
    // Nios V C code: poll prefetcher IRQ status bits directly (no ISR required)
    // 100 ticks, one print per tick — total simulation time = 100 * usleep(250us) = 25ms
    uint32_t ticks = 100;
    uint32_t rd_pf_stat, wr_pf_stat;

    // Poll dispatcher IDLE/BUSY — persists after completion, unlike IRQ status which is transient
    // Done when: IDLE=1 and BUSY=0 on both channels
    PRINT("CCA: waiting for RD+WR dispatcher IDLE (polling, max %lu ticks) ...", (unsigned long)ticks);
    PRINT("       waiting for: rd_disp[IDLE=1,BUSY=0] wr_disp[IDLE=1,BUSY=0]");

    uint32_t rd_disp_stat, wr_disp_stat;

    while (ticks > 0) {
        rd_pf_stat = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET);
        wr_pf_stat = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET);
        rd_disp_stat = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);
        wr_disp_stat = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

        int rd_done = (rd_disp_stat & DISPATCHER_IDLE_MSK) && !(rd_disp_stat & DISPATCHER_BUSY_MSK);
        int wr_done = (wr_disp_stat & DISPATCHER_IDLE_MSK) && !(wr_disp_stat & DISPATCHER_BUSY_MSK);

        // Nios V C code
        PRINT("  [T-%2lu] rd_disp=0x%08lx[IDLE=%d,BUSY=%d] wr_disp=0x%08lx[IDLE=%d,BUSY=%d]  rd_pf_irq=%d wr_pf_irq=%d",
              (unsigned long)ticks, (unsigned long)rd_disp_stat, (rd_disp_stat & DISPATCHER_IDLE_MSK) ? 1 : 0,
              (rd_disp_stat & DISPATCHER_BUSY_MSK) ? 1 : 0, (unsigned long)wr_disp_stat,
              (wr_disp_stat & DISPATCHER_IDLE_MSK) ? 1 : 0, (wr_disp_stat & DISPATCHER_BUSY_MSK) ? 1 : 0,
              (rd_pf_stat & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0,
              (wr_pf_stat & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0);

        if (rd_done && wr_done) {
            break;
        }
        // faster polling, better accuracy in performance measurements
        usleep(25); // 25us simulation time per tick
        ticks--;
    }

    if (ticks == 0) {
        // Timeout — dump debug info
        // Nios V C code
        ERROR("CCA: Transfer timeout after %lu ms", (unsigned long)timeout_ms);
        ERROR("  READ  prefetcher: ctrl=0x%08lx stat=0x%08lx",
              (unsigned long)cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET),
              (unsigned long)cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET));
        ERROR("  WRITE prefetcher: ctrl=0x%08lx stat=0x%08lx",
              (unsigned long)cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET),
              (unsigned long)cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET));
        ERROR("  READ  dispatcher: stat=0x%08lx",
              (unsigned long)cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET));
        ERROR("  WRITE dispatcher: stat=0x%08lx",
              (unsigned long)cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET));
        return -1;
    }

    // Note: DISPATCHER_STOPPED_ON_ERR_MSK can be set benignly at end-of-chain (per Intel msgDMA spec)
    // Log as warning only — data integrity is verified by buffer comparison in run_pass_through_test()
    if (rd_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) {
        // Nios V C code
        PRINT("CCA: READ dispatcher STOPPED_ON_ERR (benign end-of-chain), status=0x%08lx", (unsigned long)rd_disp_stat);
    }

    if (wr_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) {
        // Nios V C code
        PRINT("CCA: WRITE dispatcher STOPPED_ON_ERR (benign end-of-chain), status=0x%08lx",
              (unsigned long)wr_disp_stat);
    }

    // Check accelerator status
    uint32_t accel_status = cca_reg_read(CCA_BASE, CCA_ACCEL_STATUS);
    if (accel_status & CCA_STATUS_ACCEL_ERROR) {
        // Nios V C code
        ERROR("CCA: Accelerator error, status=0x%08lx", (unsigned long)accel_status);
        return -1;
    }

    // Invalidate dst cache (CPU must read fresh data from DDR)
    // Defensive — ACE5-Lite/CCU provides HW coherency, kept per Intel pattern
    // NiosV C code
    alt_dcache_flush(dst_virt, size);
    __asm__ volatile("fence" ::: "memory");

    // Nios V C code
    PRINT("CCA: transfer complete (rd_disp=0x%08lx wr_disp=0x%08lx)", (unsigned long)rd_disp_stat,
          (unsigned long)wr_disp_stat);
    return 0;
}

//=============================================================================
// Function 8: run_pass_through_test()
// Reset dst buffer, run PASS_THROUGH transfer, verify dst matches src
// Returns transfer time via transfer_time_out pointer (for performance measurement)
//=============================================================================
static int run_pass_through_test(uint32_t src_phys, volatile uint32_t *src_virt, uint32_t dst_phys,
                                 volatile uint32_t *dst_virt, uint32_t size, uint32_t iter, bool verbose,
                                 uint64_t *transfer_time_out)
{
    PRINT("\n--- PASS_THROUGH Test ---");

    // Reset destination buffer
    // Commented out to preserve 0x01010101 sentinel and verify DMA actually writes
    // memset((void *)dst_virt, 0, size);

    // Config + Submit + Wait
    if (cca_transfer_config(src_phys, (void *)src_virt, dst_phys, (void *)dst_virt, size, CCA_CMD_PASS_THROUGH,
                            verbose) != 0) {
        ERROR("PASS_THROUGH config/descriptor validation failed");
        return -1;
    }

    // Start timing: measure only DMA transfer, not verification
    uint64_t start_time, end_time;
    uint32_t rd_disp_stat, wr_disp_stat;

    // PASS_THROUGH start test timing flag
    PRINT("DEBUG: writing CCA_DBG_REG6 : iteration number is: %lu", (unsigned long)iter);
    cca_reg_write(CCA_BASE, CCA_DBG_REG6, CCA_ITER_START_FLAG | (iter & CCA_ITER_MASK));
    cca_reg_write(CCA_BASE, CCA_DBG_REG2, CCA_ITER_START_FLAG | (iter & CCA_ITER_MASK));

    // Note: PASS_THROUGH performance and data integrity; timing measured inside function
    READ_TIMER(start_time);
    // perform data transfer via DMA (non-blocking)
    cca_transfer_submit();

    // Mark submit completion in DBG_REG2
    cca_reg_write(CCA_BASE, CCA_DBG_REG2, CCA_ITER_END_FLAG | (iter & CCA_ITER_MASK));

    // Fast-path check: read dispatcher status immediately after submit
    // Hardware completes fast (~few cycles), avoid 10us polling overhead
    rd_disp_stat = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);
    wr_disp_stat = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    int rd_done = (rd_disp_stat & DISPATCHER_IDLE_MSK) && !(rd_disp_stat & DISPATCHER_BUSY_MSK);
    int wr_done = (wr_disp_stat & DISPATCHER_IDLE_MSK) && !(wr_disp_stat & DISPATCHER_BUSY_MSK);

    if (rd_done && wr_done) {
        // Fast path: transfer already complete! Skip polling.
        READ_TIMER(end_time);
        PRINT("DEBUG: Fast completion detected - erasing CCA_DBG_REG6 : iteration number is: %lu", (unsigned long)iter);
        cca_reg_write(CCA_BASE, CCA_DBG_REG6, CCA_ITER_END_FLAG | (iter & CCA_ITER_MASK));

        // Check for dispatcher errors (benign end-of-chain)
        if ((rd_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) || (wr_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK)) {
            PRINT("CCA: Dispatcher STOPPED_ON_ERR (benign end-of-chain), rd=0x%08lx wr=0x%08lx",
                  (unsigned long)rd_disp_stat, (unsigned long)wr_disp_stat);
        }

        // Check accelerator status
        uint32_t accel_status = cca_reg_read(CCA_BASE, CCA_ACCEL_STATUS);
        if (accel_status & CCA_STATUS_ACCEL_ERROR) {
            ERROR("CCA: Accelerator error, status=0x%08lx", (unsigned long)accel_status);
            return -1;
        }

        // Memory barrier (Nios V has no cache, just fence)
        __asm__ volatile("fence" ::: "memory");

        PRINT("CCA: transfer complete (fast path, rd_disp=0x%08lx wr_disp=0x%08lx)", (unsigned long)rd_disp_stat,
              (unsigned long)wr_disp_stat);
    } else {
        // Slow path: transfer still running, enter polling loop
        PRINT("Slow-path: entering cca_transfer_wait (rd_done=%d, wr_done=%d)", rd_done, wr_done);
        if (cca_transfer_wait(10000, (void *)dst_virt, size) != 0) {
            ERROR("PASS_THROUGH transfer failed");
            return -1;
        }
    }

    // calculate delta and return via output pointer
    *transfer_time_out = end_time - start_time;

    // Verify dst matches expected pattern (iteration-specific)
    uint32_t num_words = size / sizeof(uint32_t);
    uint32_t errors = 0;
    uint32_t pattern_base;

    if (iter < 16) {
        pattern_base = (0xA0 + iter) << 24;
    } else if (iter < 32) {
        pattern_base = (0xB0 + (iter - 16)) << 24;
    } else {
        pattern_base = (0xC0 + (iter - 32)) << 24;
    }

    for (uint32_t i = 0; i < num_words; i++) {
        uint32_t expected = pattern_base | i;
        if (dst_virt[i] != expected) {
            ERROR("  FAIL [%lu]: dst=0x%08lx expected=0x%08lx (iter %lu)", (unsigned long)i, (unsigned long)dst_virt[i],
                  (unsigned long)expected, (unsigned long)iter);
            if (++errors >= 10) {
                ERROR("  Too many errors, stopping verification");
                return -1;
            }
        }
    }

    if (errors > 0) {
        ERROR("PASS_THROUGH verification FAILED (%lu errors)", (unsigned long)errors);
        return -1;
    }

    PRINT("PASS_THROUGH verification PASSED (%lu bytes)", (unsigned long)size);
    return 0;
}

//=============================================================================
// Function 9: run_process_data_test()
// Reset dst buffer, run PROCESS_DATA transfer, verify per accelerator function
//=============================================================================

__attribute__((unused)) static int run_process_data_test(uint32_t src_phys, volatile uint32_t *src_virt,
                                                         uint32_t dst_phys, volatile uint32_t *dst_virt, uint32_t size,
                                                         bool verbose)
{
    PRINT("\n--- PROCESS_DATA Test ---");

    // Reset destination buffer
    memset((void *)dst_virt, 0, size);

    // Config + Submit + Wait
    if (cca_transfer_config(src_phys, (void *)src_virt, dst_phys, (void *)dst_virt, size, CCA_CMD_PROCESS_DATA,
                            verbose) != 0) {
        ERROR("PROCESS_DATA config/descriptor validation failed");
        return -1;
    }

    cca_transfer_submit();

    // Fast-path check: read dispatcher status immediately
    uint32_t rd_disp_stat = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);
    uint32_t wr_disp_stat = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    int rd_done = (rd_disp_stat & DISPATCHER_IDLE_MSK) && !(rd_disp_stat & DISPATCHER_BUSY_MSK);
    int wr_done = (wr_disp_stat & DISPATCHER_IDLE_MSK) && !(wr_disp_stat & DISPATCHER_BUSY_MSK);

    if (rd_done && wr_done) {
        // Fast path: already complete
        PRINT("CCA: PROCESS_DATA complete (fast path, rd=0x%08lx wr=0x%08lx)", (unsigned long)rd_disp_stat,
              (unsigned long)wr_disp_stat);
        __asm__ volatile("fence" ::: "memory");
    } else {
        // Slow path: enter polling
        PRINT("PROCESS_DATA: slow-path wait (rd_done=%d, wr_done=%d)", rd_done, wr_done);
        if (cca_transfer_wait(10000, (void *)dst_virt, size) != 0) {
            ERROR("PROCESS_DATA transfer failed");
            return -1;
        }
    }

    PRINT("PROCESS_DATA transfer complete");
    return 0;
}

//=============================================================================
// Function 10: print_transfer_stats()
// Print bandwidth statistics and accelerator byte counters
//=============================================================================

__attribute__((unused)) static void print_transfer_stats(uint64_t start, uint64_t end, uint64_t freq,
                                                         uint64_t num_iterations, uint32_t buf_size)
{
    double elapsed = (double)(end - start) / (double)freq;
    uint64_t total_bytes = (uint64_t)buf_size * 2 * num_iterations; // src+dst per iter

    PRINT("\n========================================");
    PRINT(" F2H Task Statistics");
    PRINT("   Iterations:      %llu", num_iterations);
    PRINT("   Elapsed:         %.3f s", elapsed);
    PRINT("   Total data:      %llu KB", total_bytes / 1024);
    PRINT("   Bandwidth:       %.2f MB/s", (double)total_bytes / (1024.0 * 1024.0 * elapsed));
    // Nios V C code
    PRINT("   Accel src cntr:  %lu bytes", (unsigned long)cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SRC));
    PRINT("   Accel snk cntr:  %lu bytes", (unsigned long)cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SNK));
    PRINT("========================================\n");
}

//=============================================================================
// Public: f2h_task()
// Calls each function in sequence
//=============================================================================

void f2h_task(uint32_t transaction_seconds)
{
    PRINT("========================================");
    PRINT(" F2H Task: CCA DMA Transfers");
    PRINT("   Data path: DRAM --> ACE5-Lite --> CCU --> msgDMA --> CCA --> DDR");
    PRINT("========================================");

    volatile uint32_t *src_buf = NULL;
    volatile uint32_t *dst_buf = NULL;

    // -------------------------------------------------------------------------
    // Test mode (compile-time switches):
    //
    //   Default — fixed buffer size, N identical iterations:
    //     CCA_TEST_SIZE  : transfer size in bytes (default: DMA_BUF_SIZE = 128KB)
    //     CCA_TEST_ITERS : number of iterations   (default: 4)
    //
    //     make prep   (uses defaults)
    //     make prep CFLAGS="-DCCA_TEST_SIZE=$((64*1024)) -DCCA_TEST_ITERS=8"
    //
    //   Sweep mode — ramp up then ramp down through all sizes:
    //     make prep CFLAGS="-DCCA_SWEEP_MODE"
    // -------------------------------------------------------------------------
#if defined(CCA_SWEEP_MODE)
    // Sweep mode: 2→4→8→16→32→64→128→64→32→16→8→4→2 KB
    static const uint32_t iter_sizes[] = {
        2 * 1024,  4 * 1024,  8 * 1024,  16 * 1024, 32 * 1024, 64 * 1024, 128 * 1024, // ramp up
        64 * 1024, 32 * 1024, 16 * 1024, 8 * 1024,  4 * 1024,  2 * 1024 // ramp down
    };
#define NUM_ITERS ((int)(sizeof(iter_sizes) / sizeof(iter_sizes[0])))
#else
    // Fixed-size mode (default): CCA_TEST_ITERS iterations all at CCA_TEST_SIZE bytes.
    // CCA_TEST_SIZE must be a multiple of 32 and <= DMA_BUF_SIZE (128KB).
#ifndef CCA_TEST_SIZE
#define CCA_TEST_SIZE (8 * 1024) // default: 8KB
#endif
#ifndef CCA_TEST_ITERS
#define CCA_TEST_ITERS 4 // default: 4 iterations
#endif
    static const uint32_t iter_sizes[] = { [0 ...(CCA_TEST_ITERS - 1)] = (uint32_t)(CCA_TEST_SIZE) };
#define NUM_ITERS (CCA_TEST_ITERS)
#endif

    // 1. Allocate maximum-size buffers once (128KB each)
    if (alloc_dma_buffers(&src_buf, &dst_buf, DMA_BUF_SIZE) != 0) {
        return;
    }

    // 2. Initialize full src buffer once with pattern (iter=0 → 0xa0000000 | i)
    //    The full 128KB is pre-loaded; each iteration DMA transfers a leading slice.
    if (init_src_buffer(src_buf, DMA_BUF_SIZE, 0) != 0) {
        return;
    }

    // 3. Initialize full destination buffer with sentinel once
    if (init_dst_buffer(dst_buf, DMA_BUF_SIZE) != 0) {
        return;
    }

    // MSGDMA master view addresses (different from CPU view in Nios V sim)
    uint32_t src_phys = DMA_READ_BUF_MSGDMA;
    uint32_t dst_phys = DMA_WRITE_BUF_MSGDMA;

    uint64_t counter_freq;
    READ_TIMER_FRQ(counter_freq);

    uint64_t passthru_times[NUM_ITERS] = { 0 };

    PRINT("\nStarting CCA transfers: %d iterations"
#if defined(CCA_SWEEP_MODE)
          " (sweep: 2→128→2 KB)...",
          NUM_ITERS
#else
          " (fixed size: %lu KB x %d)",
          NUM_ITERS, (unsigned long)((CCA_TEST_SIZE) / 1024), CCA_TEST_ITERS
#endif
    );

    for (int iter = 0; iter < NUM_ITERS; iter++) {
        uint32_t buf_size = iter_sizes[iter];
        bool verbose = false;

        // src buffer already initialized — no per-iteration re-init needed
        // Verify accelerator counters are zero before starting iteration
        // (should be zero on first iter, and after CCA_CTRL_RESET on subsequent iters)
        uint32_t pre_src_transfers = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SRC);
        uint32_t pre_snk_transfers = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SNK);
        PRINT("--- Iteration %lu Pre-Test Counter Check ---", (unsigned long)iter);
        PRINT("   Accel src cntr:  %lu transfers (expected 0)", (unsigned long)pre_src_transfers);
        PRINT("   Accel snk cntr:  %lu transfers (expected 0)", (unsigned long)pre_snk_transfers);
        if (pre_src_transfers != 0) {
            ERROR("WARNING: Accel src counter not zero at iteration start! got=%lu", (unsigned long)pre_src_transfers);
        }
        if (pre_snk_transfers != 0) {
            ERROR("WARNING: Accel snk counter not zero at iteration start! got=%lu", (unsigned long)pre_snk_transfers);
        }

        // PASS_THROUGH start test : do not start perf timing here
        PRINT("DEBUG: writing CCA_DBG_REG7 : iteration number is: %lu", (unsigned long)iter);
        cca_reg_write(CCA_BASE, CCA_DBG_REG7, CCA_ITER_START_FLAG | (iter & CCA_ITER_MASK));

        // Note: PASS_THROUGH performance and data integrity
        //       performance timing measured inside function, before data verification performed by Nios
        if (run_pass_through_test(src_phys, src_buf, dst_phys, dst_buf, buf_size, 0u, verbose, &passthru_times[iter]) !=
            0) {
            goto cleanup;
        }
        // PASS_THROUGH end test : do not end perf timing here
        PRINT("DEBUG: erasing CCA_DBG_REG7 : iteration number is: %lu", (unsigned long)iter);
        cca_reg_write(CCA_BASE, CCA_DBG_REG7, CCA_ITER_END_FLAG | ((uint32_t)iter & CCA_ITER_MASK));

        // Read accelerator transfer counters and verify they match expected transfer count
        // (counters track 256-bit transfers; multiply by 32 to get bytes)
        // (counters are reset by CCA_CTRL_RESET between iterations, not cumulative)
        uint32_t accel_src_transfers = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SRC);
        uint32_t accel_snk_transfers = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SNK);
        uint32_t accel_src_bytes = accel_src_transfers * 32;
        uint32_t accel_snk_bytes = accel_snk_transfers * 32;
        uint32_t expected_bytes = buf_size;
        uint32_t expected_transfers = buf_size / 32;

        PRINT("--- Iteration %lu Accelerator Counters ---", (unsigned long)iter);
        PRINT("   Accel src cntr:  %lu transfers = %lu bytes (expected %lu transfers = %lu bytes)",
              (unsigned long)accel_src_transfers, (unsigned long)accel_src_bytes, (unsigned long)expected_transfers,
              (unsigned long)expected_bytes);
        PRINT("   Accel snk cntr:  %lu transfers = %lu bytes (expected %lu transfers = %lu bytes)",
              (unsigned long)accel_snk_transfers, (unsigned long)accel_snk_bytes, (unsigned long)expected_transfers,
              (unsigned long)expected_bytes);

        if (accel_src_bytes != expected_bytes) {
            ERROR(
                "WARNING: Accel src counter mismatch! got=%lu transfers (%lu bytes) expected=%lu transfers (%lu bytes)",
                (unsigned long)accel_src_transfers, (unsigned long)accel_src_bytes, (unsigned long)expected_transfers,
                (unsigned long)expected_bytes);
        }
        if (accel_snk_bytes != expected_bytes) {
            ERROR(
                "WARNING: Accel snk counter mismatch! got=%lu transfers (%lu bytes) expected=%lu transfers (%lu bytes)",
                (unsigned long)accel_snk_transfers, (unsigned long)accel_snk_bytes, (unsigned long)expected_transfers,
                (unsigned long)expected_bytes);
        }

        // Reset CCA accelerator pipeline to flush stale data between iterations
        // (CCA has internal FIFOs that retain data; reset clears them)
        PRINT("DEBUG: Resetting CCA accelerator pipeline");
        cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_RESET);

        // Delay between tests
        usleep(10); // 10 usec delay for simulation

        // PROCESS_DATA test timing
        // READ_TIMER(temp_start);
        // if (run_process_data_test(src_phys, src_buf, dst_phys, dst_buf, buf_size, verbose) != 0) {
        //     goto cleanup;
        // }
        // READ_TIMER(temp_end);
        // procdata_times[iter] = temp_end - temp_start;
    }

    // Print per-iteration performance table
    PRINT("\n========================================");
    PRINT(" F2H Task Performance Summary");
    PRINT("   Max buffer size: 128 KB");
#if defined(CCA_SWEEP_MODE)
    PRINT("   Mode: sweep 2->128->2 KB (%d iters)", NUM_ITERS);
#else
    PRINT("   Mode: fixed %lu KB x %d iters", (unsigned long)((CCA_TEST_SIZE) / 1024), CCA_TEST_ITERS);
#endif
    PRINT("========================================");
    PRINT("\n--- PASS_THROUGH Performance (per iteration) ---");
    PRINT("  iter  size       time(ms)   MB/s");
    for (int i = 0; i < NUM_ITERS; i++) {
        uint64_t sz = iter_sizes[i];
        double ms = (double)passthru_times[i] / counter_freq * 1000.0;
        double mbs = (double)(sz * 2) / (1024.0 * 1024.0) / ((double)passthru_times[i] / counter_freq);
        PRINT("  %4d  %5lu KB  %8.3f   %7.2f", i, (unsigned long)(sz / 1024), ms, mbs);
    }
    PRINT("========================================\n");

cleanup:
    return;
}
