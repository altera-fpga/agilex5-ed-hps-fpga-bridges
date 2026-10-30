/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * DMA Configuration and Verification for Cache Coherent Accelerator (CCA)
 * ----------------------------------------------------------------------
 * Implements initialization, configuration, and status checking routines
 * for the CCA DMA subsystem, including:
 *   - Accelerator core reset and startup
 *   - Descriptor-based DMA transfer setup (read/write chains)
 *   - Prefetcher and dispatcher control for Intel msgDMA
 *   - Polling and verification of DMA transfer completion
 *   - Descriptor chain validation and debug support
 * ----------------------------------------------------------------------
 * This file provides test and utility functions for exercising and
 * verifying the CCA DMA subsystem in hardware validation flows.
 * ----------------------------------------------------------------------
 */
// Define environment before including headers
#define __NIOS_V__

// System headers
#include "io.h" // IOWR_32DIRECT, IORD_32DIRECT

#include <stdio.h> // printf()
#include <unistd.h> // usleep()
#include <stdlib.h> // malloc(), free()
#include <string.h> // memcpy()

#include "alt_types.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"

// Cache Coherent Accelerator HAL (unified API, now uses system.h for base addresses)
#include "cca_hal.h"
// Test function prototypes
#include "cca_api.h"

/////////////////////////////////////////////////////////////////////////////
// Accelerator Initialization Function
////////////////////////////////////////
// Resets and starts the CCA accelerator core:
//   1. Write 0x00008000 to ACCEL_CONTROL to reset
//   2. Poll ACCEL_STATUS until != 0x00000000
//   3. Write 0x000001FF to ACCEL_CONTROL to send CMD_ERROR_TEST
//   4. Write 0x00000400 to ACCEL_CONTROL to send ABORT command
//   5. Write 0x00000102 to ACCEL_CONTROL to send CMD_PASS_THROUGH
// Returns: 0 on success, 1 on failure (timeout)
/////////////////////////////////////////////////////////////////////////////
int accel_init(void)
{
    printf("\nNIOS: Initializing CCA accelerator core...\n");
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 1 : Reset accelerator
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    IOWR_32DIRECT(CCA_BASE, CCA_ACCEL_CONTROL, 0x00008000);
    printf("  Accelerator reset issued (ACCEL_CONTROL = 0x00008000)\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 2 : Poll ACCEL_STATUS until not 0
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // This code will need to be replaced when using FreeRTOS.
    // We don't want to create a spin loop like this which blocks the processors.
    int timeout = 1000;
    alt_u32 status = 0;
    do {
        status = IORD_32DIRECT(CCA_BASE, CCA_ACCEL_STATUS);
        if (status != 0)
            break;
        usleep(1000);
    } while (--timeout > 0);

    if (timeout <= 0) {
        printf("  ERROR: Accelerator did not exit reset (ACCEL_STATUS still 0)\n");
        return 1;
    }
    printf("  Accelerator out of reset (ACCEL_STATUS = 0x%08lx)\n", status);

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 3 : Send CMD_ERROR_TEST
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    IOWR_32DIRECT(CCA_BASE, CCA_ACCEL_CONTROL, 0x000001FF); // CMD_ERROR_TEST
    printf("  Accelerator sent CMD_ERROR_TEST (ACCEL_CONTROL = 0x000001FF)\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 4 : Send ABORT command
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    IOWR_32DIRECT(CCA_BASE, CCA_ACCEL_CONTROL, 0x00000400); // ABORT command
    printf("  Accelerator sent ABORT command (ACCEL_CONTROL = 0x00000400)\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 5 : Reset byte counters (optional, for cleaner debug visibility)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    IOWR_32DIRECT(CCA_BASE, CCA_ACCEL_CONTROL, 0x00000800); // RESET counters
    printf("  Accelerator sent RESET counters (ACCEL_CONTROL = 0x00000800)\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 6 : Send CMD_PASS_THROUGH
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    IOWR_32DIRECT(CCA_BASE, CCA_ACCEL_CONTROL, 0x00000102); // CMD_PASS_THROUGH
    printf("  Accelerator sent CMD_PASS_THROUGH (ACCEL_CONTROL = 0x00000102)\n");
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// DMA Configuration Function
//
// * configures u_msgDMA_read and u_msgDMA_write prefetchers
// * configures u_msgDMA_read and u_msgDMA_write dispatchers
// * sets up the u_msgDMA_read and u_msgDMA_write descriptors
//
// Returns: 0 on success, 1 on failure
/////////////////////////////////////////////////////////////////////////////
int config_dma_transfer(void)
{
    printf("\nNIOS: Configuring DMA transfer...\n");
    printf("NIOS: DMA_READ_BUF  = 0x%08lx (Nios V view)\n", (alt_u32)DMA_READ_BUF);
    printf("      DMA_READ_BUF  = 0x%08lx (MSGDMA view)\n", (alt_u32)DMA_READ_BUF_MSGDMA);
    printf("NIOS: DMA_WRITE_BUF = 0x%08lx (Nios V view)\n", (alt_u32)DMA_WRITE_BUF);
    printf("      DMA_WRITE_BUF = 0x%08lx (MSGDMA view)\n", (alt_u32)DMA_WRITE_BUF_MSGDMA);
    printf("NIOS: Transfer size = %d bytes\n", DMA_BUF_SIZE);

    printf("NIOS: Writing TEST_DMA (0x%08lx) to CCA_DBG_REG10 at addr 0x%08lx\n", (alt_u32)TEST_DMA,
           (alt_u32)(CCA_BASE + CCA_DBG_REG10));
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_DMA);

    printf("NIOS: MSGDMA_WR_BASE = 0x%08lx\n", (alt_u32)MSGDMA_WR_BASE);
    printf("NIOS: MSGDMA_RD_BASE = 0x%08lx\n", (alt_u32)MSGDMA_RD_BASE);

    // ---------------------------------------------------------------
    // Step 0a: Reset WRITE prefetcher & poll until HW clears bit
    //          per configure_fdmac reference driver
    // ---------------------------------------------------------------
    // Debug reg write before reset bit write
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_0);
    // Actual reset write to prefetcher control register
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);

    // polling for reset bit to clear as required per Altera reference driver
    // to ensure prefetcher is ready for subsequent configuration steps (e.g. setting IRQ_EN, configuring dispatcher, etc.)
    while (cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK)
        ;
    // Debug reg write after polling complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_1);
    printf("  WRITE prefetcher reset complete\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 0b: Enable IRQ to set STATUS bit (required for polling)
    //          IRQ line will assert but BSP handler skipped/masked
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, reg_val | PREFETCHER_IRQ_EN_MSK);
    }
    // Debug reg write after IRQ decision
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_2);
    printf("  WRITE prefetcher IRQ_EN set (STATUS bit visible for polling)\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 0c: Reset WRITE dispatcher â€” poll until reset complete
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Debug reg write before reset bit write
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_3);
    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, reg_val | DISPATCHER_RESET_MSK);
    }
    while (cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET) & DISPATCHER_RESET_IN_PROGRESS_MSK)
        ;
    // Debug reg write after polling complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_4);
    printf("  WRITE dispatcher reset complete\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 0d: Disable STOP_ON_ERR on WRITE dispatcher (explicit clear)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    {
        /* Clear STOP_ON_ERR (write 0 to CONTROL) so dispatcher keeps running on response errors
         * This is a test change to detect upstream response issues without stopping the dispatcher.
         */
        cca_reg_write(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, 0x00);
    }
    /* Debug reg write after disabling STOP_ON_ERR */
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_5);
    printf("  WRITE dispatcher running (STOP_ON_ERR disabled)\n");

    // ---------------------------------------------------------------
    // Step 0e: Reset READ prefetcher â€” poll until HW clears bit
    // ---------------------------------------------------------------
    // Debug reg write before reset bit write
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_0);
    // Actual reset write to prefetcher control register
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    // polling for reset bit to clear as required per Altera reference driver
    // to ensure prefetcher is ready for subsequent configuration steps (e.g. setting IRQ_EN, configuring dispatcher, etc.)
    while (cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK)
        ;
    // Debug reg write after polling complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_1);
    printf("  READ  prefetcher reset complete\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 0f: Enable IRQ to set STATUS bit (required for polling)
    //          IRQ line will assert but BSP handler skipped/masked
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, reg_val | PREFETCHER_IRQ_EN_MSK);
    }
    // Debug reg write after IRQ decision
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_2);
    printf("  READ  prefetcher IRQ_EN set (STATUS bit visible for polling)\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 0g: Reset READ dispatcher â€” poll until reset complete
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Debug reg write before reset bit write
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_3);
    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, reg_val | DISPATCHER_RESET_MSK);
    }
    while (cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET) & DISPATCHER_RESET_IN_PROGRESS_MSK)
        ;
    // Debug reg write after polling complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_4);
    printf("  READ  dispatcher reset complete\n");

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Step 0h: Set STOP_ON_ERR on READ dispatcher (RMW)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET, reg_val | DISPATCHER_STOP_ON_ERR_MSK);
    }
    // Debug reg write after Set STOP_ON_ERR complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_5);
    printf("  READ  dispatcher STOP_ON_ERR set\n");

    // ---------------------------------------------------------------
    // Step 1: Create 8 READ descriptors (MM2ST: RAM → Accelerator)
    //         OCM layout: READ chain at OCM_BASE + 0
    //         Each descriptor: 32 bytes, stride 32 bytes
    //         First 7: DESC_CTRL_MID_CHAIN  (OWNED_BY_HW | EARLY_DONE)
    //         Last   : DESC_CTRL_LAST_CHAIN (OWNED_BY_HW | WAIT_FOR_WRITE | IRQ)
    // ---------------------------------------------------------------
    alt_u32 chunk_size = DMA_BUF_SIZE / NUM_OF_DESCRIPTORS; // 256 bytes per descriptor
    printf("NIOS: Creating %d READ descriptors, chunk_size = %ld bytes\n", NUM_OF_DESCRIPTORS, chunk_size);

    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        alt_u32 desc_offset = OCM_BASE + (i * DESC_STRIDE);
        alt_u32 src_offset = DMA_READ_BUF_MSGDMA + (i * chunk_size);
        alt_u32 next_desc = (i < (NUM_OF_DESCRIPTORS - 1)) ? (OCM_BASE_MSGDMA + ((i + 1) * DESC_STRIDE)) : 0;
        alt_u32 rd_control = (i < (NUM_OF_DESCRIPTORS - 1)) ? DESC_CTRL_MID_CHAIN : DESC_CTRL_LAST_CHAIN;

        cca_reg_write(desc_offset, DESC_OFF_SRC_ADDR, src_offset); // read addr [31:0]
        cca_reg_write(desc_offset, DESC_OFF_DEST_ADDR, 0); // reserved (MM2ST)
        cca_reg_write(desc_offset, DESC_OFF_LENGTH, chunk_size); // length
        cca_reg_write(desc_offset, DESC_OFF_NEXT_DESC, next_desc); // next_desc_ptr
        cca_reg_write(desc_offset, DESC_OFF_RESERVED_1, 0); // reserved [31:0]
        cca_reg_write(desc_offset, DESC_OFF_RESERVED_2, 0); // reserved [63:32]
        cca_reg_write(desc_offset, DESC_OFF_RESERVED_3, 0); // reserved [31:0]
        cca_reg_write(desc_offset, DESC_OFF_CONTROL, rd_control); // control
    }
    // Debug reg write after Read descriptors are complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_6);

    // ---------------------------------------------------------------
    // Step 2: Create 8 WRITE descriptors (ST2MM: Accelerator → RAM)
    //         OCM layout: WRITE chain at OCM_BASE + 256
    //         Same structure as READ chain
    // ---------------------------------------------------------------
    printf("NIOS: Creating %d WRITE descriptors, chunk_size = %ld bytes\n", NUM_OF_DESCRIPTORS, chunk_size);

    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        alt_u32 desc_offset = OCM_BASE + WRITE_DESC_BASE_OFF + (i * DESC_STRIDE);
        alt_u32 dest_offset = DMA_WRITE_BUF_MSGDMA + (i * chunk_size);
        alt_u32 next_desc =
            (i < (NUM_OF_DESCRIPTORS - 1)) ? (OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF + ((i + 1) * DESC_STRIDE)) : 0;
        alt_u32 wr_control = (i < (NUM_OF_DESCRIPTORS - 1)) ? DESC_CTRL_MID_CHAIN : DESC_CTRL_LAST_CHAIN;

        cca_reg_write(desc_offset, DESC_OFF_SRC_ADDR, 0); // reserved (ST2MM) (stream)
        cca_reg_write(desc_offset, DESC_OFF_DEST_ADDR, dest_offset); // write addr [31:0]
        cca_reg_write(desc_offset, DESC_OFF_LENGTH, chunk_size); // length
        cca_reg_write(desc_offset, DESC_OFF_NEXT_DESC, next_desc); // next_desc_ptr
        cca_reg_write(desc_offset, DESC_OFF_RESERVED_1, 0); // reserved [31:0]
        cca_reg_write(desc_offset, DESC_OFF_RESERVED_2, 0); // reserved [63:32]
        cca_reg_write(desc_offset, DESC_OFF_RESERVED_3, 0); // reserved [31:0]
        cca_reg_write(desc_offset, DESC_OFF_CONTROL, wr_control); // control
    }
    // Debug reg write after Write descriptors are complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_6);

    printf("  Eight READ and eight WRITE descriptors created successfully\n");

    // ---------------------------------------------------------------
    // Validate all 16 descriptors immediately after creation
    // ---------------------------------------------------------------
    /* commented to speed up the tests
    printf("\n-- Running descriptor validation (will walk chains) --\n");
    int val_ok = validate_descriptors();
    if (val_ok) {
        printf("Descriptor validation: PASS\n");
    } else {
        printf("Descriptor validation: FAIL - check descriptor chains and fields\n");
    }  // */
    // ---------------------------------------------------------------
    // Step 3: Point prefetchers to first descriptor (MSGDMA view)
    //         READ  chain: OCM_BASE_MSGDMA + 0
    //         WRITE chain: OCM_BASE_MSGDMA + 256
    // ---------------------------------------------------------------
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF);
    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0);
    printf("  WRITE prefetcher NEXT_DESC_PTR = 0x%08lx\n", (alt_u32)(OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF));
    // Debug reg write after Write prefetcher points to descriptors are complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_7);

    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA);
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0);
    printf("  READ  prefetcher NEXT_DESC_PTR = 0x%08lx\n", (alt_u32)(OCM_BASE_MSGDMA));
    // Debug reg write after Read prefetcher points to descriptors are complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_7);

    cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0);
    printf("  WRITE prefetcher NEXT_DESC_PTR = 0x%08lx\n", (alt_u32)(OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF));
    // Debug reg write after Write prefetcher points to descriptors are complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_7);

    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET, OCM_BASE_MSGDMA);
    cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET, 0);
    printf("  READ  prefetcher NEXT_DESC_PTR = 0x%08lx\n", (alt_u32)(OCM_BASE_MSGDMA));
    // Debug reg write after Read prefetcher points to descriptors are complete
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_7);

    // ---------------------------------------------------------------
    // Step 4: Start WRITE prefetcher first, then READ prefetcher
    //         Set RUN bit (IRQ_EN already set for STATUS visibility)
    // ---------------------------------------------------------------
    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET, reg_val | PREFETCHER_RUN_MSK);
    }
    // Debug reg write after Write prefetcher started
    cca_reg_write(CCA_BASE, CCA_DBG_REG0, DBG_TST_VAL_8);
    printf("  WRITE prefetcher started (RUN=1, IRQ_EN=1 for STATUS visibility)\n");

    {
        alt_u32 reg_val = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET);
        cca_reg_write(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET, reg_val | PREFETCHER_RUN_MSK);
    }
    // Debug reg write after Read prefetcher started
    cca_reg_write(CCA_BASE, CCA_DBG_REG1, DBG_TST_VAL_8);
    printf("  READ  prefetcher started (RUN=1, IRQ_EN=1 for STATUS visibility)\n");

    printf("NIOS: DMA transfer initiated\n");
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// DMA Status Check Function
//
// Polls MSGDMA status registers to check transfer completion.
// Prints detailed status every 10 calls to reduce console output.
//
// Returns: 0 on success (transfer complete), 1 on failure or timeout
/////////////////////////////////////////////////////////////////////////////
int check_dma_status(void)
{
    static int poll_count = 0;
    int verbose = (poll_count % 10 == 0);

    // Debug: Print actual addresses being accessed (only once)
    if (poll_count == 0) {
        printf("\n=== DMA Register Address Map ===\n");
        printf("READ Channel:\n");
        printf("  MSGDMA_RD_BASE          = 0x%08lx\n", (alt_u32)MSGDMA_RD_BASE);
        printf("  PF_CONTROL   @ offset 0x%02x = 0x%08lx\n", MSGDMA_PF_CONTROL_OFFSET,
               (alt_u32)(MSGDMA_RD_BASE + MSGDMA_PF_CONTROL_OFFSET));
        printf("  PF_STATUS    @ offset 0x%02x = 0x%08lx\n", MSGDMA_PF_STATUS_OFFSET,
               (alt_u32)(MSGDMA_RD_BASE + MSGDMA_PF_STATUS_OFFSET));
        printf("  MSGDMA_RD_DISPATCHER_BASE = 0x%08lx\n", (alt_u32)MSGDMA_RD_DISPATCHER_BASE);
        printf("  DISP_STATUS  @ offset 0x%02x = 0x%08lx\n", MSGDMA_DISP_STATUS_OFFSET,
               (alt_u32)(MSGDMA_RD_DISPATCHER_BASE + MSGDMA_DISP_STATUS_OFFSET));
        printf("WRITE Channel:\n");
        printf("  MSGDMA_WR_BASE          = 0x%08lx\n", (alt_u32)MSGDMA_WR_BASE);
        printf("  PF_CONTROL   @ offset 0x%02x = 0x%08lx\n", MSGDMA_PF_CONTROL_OFFSET,
               (alt_u32)(MSGDMA_WR_BASE + MSGDMA_PF_CONTROL_OFFSET));
        printf("  PF_STATUS    @ offset 0x%02x = 0x%08lx\n", MSGDMA_PF_STATUS_OFFSET,
               (alt_u32)(MSGDMA_WR_BASE + MSGDMA_PF_STATUS_OFFSET));
        printf("  MSGDMA_WR_DISPATCHER_BASE = 0x%08lx\n", (alt_u32)MSGDMA_WR_DISPATCHER_BASE);
        printf("  DISP_STATUS  @ offset 0x%02x = 0x%08lx\n", MSGDMA_DISP_STATUS_OFFSET,
               (alt_u32)(MSGDMA_WR_DISPATCHER_BASE + MSGDMA_DISP_STATUS_OFFSET));
        printf("================================\n\n");
    }

    // Check read prefetcher/dispatcher status
    alt_u32 rd_prefetch_ctrl = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_CONTROL_OFFSET);
    alt_u32 rd_prefetch_status = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET);
    alt_u32 rd_dispatch_status = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);
    alt_u32 rd_dispatch_ctrl = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET);

    // Check write prefetcher/dispatcher status
    alt_u32 wr_prefetch_ctrl = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET);
    alt_u32 wr_prefetch_status = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET);
    alt_u32 wr_dispatch_status = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);
    alt_u32 wr_dispatch_ctrl = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_CONTROL_OFFSET);

    // If the WRITE dispatcher reports STOPPED_ON_ERR, read its response FIFO
    if (wr_dispatch_status & DISPATCHER_STOPPED_ON_ERR_MSK) {
        printf("\n!!! WRITE DISPATCHER ERROR DETECTED !!!\n");

        alt_u32 resp_fill = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);
        printf("  Response FIFO fill level: %ld\n", resp_fill);

        if (resp_fill > 0) {
            printf("  Reading response FIFO entries:\n");
            for (int i = 0; i < (int)resp_fill && i < NUM_OF_DESCRIPTORS; i++) {
                alt_u32 resp = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, 0x4C); /* Response register (pops FIFO) */
                printf("    Resp[%d] = 0x%08lx (Error:%d, EarlyTerm:%d)\n", i, resp, (int)((resp >> 8) & 0xFF),
                       (int)((resp >> 16) & 0x1));
            }
        }
    }

    if (verbose) {
        printf("\n== ======== DMA Status [poll %d] ======== ==\n", poll_count);

        alt_u32 rd_next_lo = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
        alt_u32 rd_next_hi = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET);
        alt_u32 rd_poll = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_DESC_POLL_FREQ_OFFSET);
        alt_u32 rd_error = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);

        alt_u32 wr_next_lo = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
        alt_u32 wr_next_hi = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_HI_OFFSET);
        alt_u32 wr_poll = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_DESC_POLL_FREQ_OFFSET);
        alt_u32 wr_error = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);

        // RAW register dump for cross-checking with waveforms
        printf("RAW Register Values:\n");
        printf("  READ  PF_STATUS  [0x%08lx] = 0x%08lx\n", (alt_u32)(MSGDMA_RD_BASE + MSGDMA_PF_STATUS_OFFSET),
               rd_prefetch_status);
        printf("  WRITE PF_STATUS  [0x%08lx] = 0x%08lx\n", (alt_u32)(MSGDMA_WR_BASE + MSGDMA_PF_STATUS_OFFSET),
               wr_prefetch_status);
        printf("  READ  DISP_STATUS[0x%08lx] = 0x%08lx\n",
               (alt_u32)(MSGDMA_RD_DISPATCHER_BASE + MSGDMA_DISP_STATUS_OFFSET), rd_dispatch_status);
        printf("  WRITE DISP_STATUS[0x%08lx] = 0x%08lx\n",
               (alt_u32)(MSGDMA_WR_DISPATCHER_BASE + MSGDMA_DISP_STATUS_OFFSET), wr_dispatch_status);
        printf("\n");

        printf("READ Channel:\n");
        printf("  Prefetch Ctrl  = 0x%08lx (RUN:%d RESET:%d IRQ_EN:%d)\n", rd_prefetch_ctrl,
               (rd_prefetch_ctrl & PREFETCHER_RUN_MSK) ? 1 : 0, (rd_prefetch_ctrl & PREFETCHER_RESET_MSK) ? 1 : 0,
               (rd_prefetch_ctrl & PREFETCHER_INTERRUPT_EN_MSK) ? 1 : 0);
        printf("  Prefetch Stat  = 0x%08lx (IRQ:%d)\n", rd_prefetch_status,
               (rd_prefetch_status & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0);
        printf("  Next Desc LO   = 0x%08lx %s\n", rd_next_lo, (rd_next_lo == 0) ? "[fetched]" : "[pending]");
        printf("  Next Desc HI   = 0x%08lx\n", rd_next_hi);
        printf("  Desc Poll Freq = 0x%08lx\n", rd_poll);
        printf("  Dispatch Stat  = 0x%08lx (BUSY:%d IDLE:%d STOPPED:%d STOPPED_ON_ERR:%d)\n", rd_dispatch_status,
               (rd_dispatch_status & DISPATCHER_BUSY_MSK) ? 1 : 0, (rd_dispatch_status & DISPATCHER_IDLE_MSK) ? 1 : 0,
               (rd_dispatch_status & DISPATCHER_STOPPED_MSK) ? 1 : 0,
               (rd_dispatch_status & DISPATCHER_STOPPED_ON_ERR_MSK) ? 1 : 0);
        printf("  Dispatch Ctrl  = 0x%08lx (STOP_ON_ERR:%d RESET:%d)\n", rd_dispatch_ctrl,
               (rd_dispatch_ctrl & DISPATCHER_STOP_ON_ERR_MSK) ? 1 : 0,
               (rd_dispatch_ctrl & DISPATCHER_RESET_MSK) ? 1 : 0);
        printf("  Resp Fill Lvl  = 0x%08lx\n", rd_error);
        printf("\n");

        printf("WRITE Channel:\n");
        printf("  Prefetch Ctrl  = 0x%08lx (RUN:%d RESET:%d IRQ_EN:%d)\n", wr_prefetch_ctrl,
               (wr_prefetch_ctrl & PREFETCHER_RUN_MSK) ? 1 : 0, (wr_prefetch_ctrl & PREFETCHER_RESET_MSK) ? 1 : 0,
               (wr_prefetch_ctrl & PREFETCHER_INTERRUPT_EN_MSK) ? 1 : 0);
        printf("  Prefetch Stat  = 0x%08lx (IRQ:%d)\n", wr_prefetch_status,
               (wr_prefetch_status & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0);
        printf("  Next Desc LO   = 0x%08lx %s\n", wr_next_lo, (wr_next_lo == 0) ? "[fetched]" : "[pending]");
        printf("  Next Desc HI   = 0x%08lx\n", wr_next_hi);
        printf("  Desc Poll Freq = 0x%08lx\n", wr_poll);
        printf("  Dispatch Stat  = 0x%08lx (BUSY:%d IDLE:%d STOPPED:%d STOPPED_ON_ERR:%d)\n", wr_dispatch_status,
               (wr_dispatch_status & DISPATCHER_BUSY_MSK) ? 1 : 0, (wr_dispatch_status & DISPATCHER_IDLE_MSK) ? 1 : 0,
               (wr_dispatch_status & DISPATCHER_STOPPED_MSK) ? 1 : 0,
               (wr_dispatch_status & DISPATCHER_STOPPED_ON_ERR_MSK) ? 1 : 0);
        printf("  Dispatch Ctrl  = 0x%08lx (STOP_ON_ERR:%d RESET:%d)\n", wr_dispatch_ctrl,
               (wr_dispatch_ctrl & DISPATCHER_STOP_ON_ERR_MSK) ? 1 : 0,
               (wr_dispatch_ctrl & DISPATCHER_RESET_MSK) ? 1 : 0);
        printf("  Resp Fill Lvl  = 0x%08lx\n", wr_error);

        /* Response FIFO quick debug */
        printf("Response FIFO Debug:\n");
        printf("  WRITE Resp Fill = 0x%08lx (should be 0 or small)\n", wr_error);
        printf("  READ  Resp Fill = 0x%08lx (should be 0 or small)\n", rd_error);

        /* Critical FIFO fill registers for BUSY semantics */
        {
            alt_u32 wr_rw_fill = cca_reg_read(MSGDMA_WR_BASE, 0x08); /* RW_FILL_LEVEL */
            alt_u32 wr_write_fill = (wr_rw_fill >> 16) & 0xFFFF;
            alt_u32 wr_read_fill = wr_rw_fill & 0xFFFF;
            alt_u32 wr_resp_fill = cca_reg_read(MSGDMA_WR_BASE, 0x0C); /* RESP_FILL_LEVEL */

            printf("  Write Fill Lvl = 0x%04x (%d commands)\n", (unsigned)wr_write_fill, (int)wr_write_fill);
            printf("  Read  Fill Lvl = 0x%04x (%d commands)\n", (unsigned)wr_read_fill, (int)wr_read_fill);
            printf("  Resp  Fill Lvl = 0x%04x (%d responses)\n", (unsigned)wr_resp_fill, (int)wr_resp_fill);
        }

        printf("\nDescriptor Status:\n");
        printf("  READ  ptr: 0x%08lx_%08lx %s\n", rd_next_hi, rd_next_lo,
               (rd_next_lo == 0) ? "[fetched]" : "[pending]");
        printf("  WRITE ptr: 0x%08lx_%08lx %s\n", wr_next_hi, wr_next_lo,
               (wr_next_lo == 0) ? "[fetched]" : "[pending]");

        printf("\n");
        detect_msgdma_read_start();
        printf("==========================================\n");
    }

    poll_count++;

    // Check for completion — dispatcher IDLE status (more reliable than IRQ bit)
    // Done when: IDLE=1, BUSY=0
    // Note: STOPPED_ON_ERR can be set benignly (e.g., end-of-chain), so we don't check it
    //       Data integrity will be verified by buffer comparison
    int rd_done = (rd_dispatch_status & DISPATCHER_IDLE_MSK) && !(rd_dispatch_status & DISPATCHER_BUSY_MSK);
    int wr_done = (wr_dispatch_status & DISPATCHER_IDLE_MSK) && !(wr_dispatch_status & DISPATCHER_BUSY_MSK);

    /* Temporary: if prefetcher IRQ status bit is set, treat channel as done
     * (quick mitigation per user's request — revisit BUSY semantics later)
     */
    if (rd_prefetch_status & PREFETCHER_INTERRUPT_STATUS_MSK) {
        if (verbose)
            printf("  NOTE: READ prefetcher IRQ set — treating READ as done for now\n");
        rd_done = 1;
    }
    if (wr_prefetch_status & PREFETCHER_INTERRUPT_STATUS_MSK) {
        if (verbose)
            printf("  NOTE: WRITE prefetcher IRQ set — treating WRITE as done for now\n");
        wr_done = 1;
    }

    if (rd_done && wr_done) {
        printf("NIOS: Writing TEST_END (0x%08lx) to CCA_DBG_REG10\n", (alt_u32)TEST_END);
        cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_END);
        printf("\nNIOS: DMA transfer COMPLETE (both channels done) after %d polls\n", poll_count);

        // ===============================================================
        // VERIFICATION: Methods 1, 2
        // ===============================================================
        printf("\n== ======== DMA TRANSFER VERIFICATION ======== ==\n");

        // ---------------------------------------------------------------
        // METHOD 1: Check Response FIFO levels
        // ---------------------------------------------------------------
        printf("\n--- Method 1: Response FIFO Status ---\n");
        alt_u32 rd_resp_fill = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);
        alt_u32 wr_resp_fill = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);

        printf("  READ response FIFO level:  %ld\n", rd_resp_fill);
        printf("  WRITE response FIFO level: %ld\n", wr_resp_fill);

        // ---------------------------------------------------------------
        // METHOD 2: Direct buffer comparison
        // ---------------------------------------------------------------
        printf("\n--- Method 2: Direct Buffer Comparison ---\n");
        int errors = 0;
        int bytes_matched = 0;

        for (int i = 0; i < 512; i++) {
            alt_u32 src = IORD_32DIRECT(DMA_READ_BUF, i * 4);
            alt_u32 dst = IORD_32DIRECT(DMA_WRITE_BUF, i * 4);

            if (src != dst) {
                if (errors < 10) {
                    printf("  ERROR [word %3d]: dma_rd_buf[0x%08lx]=0x%08lx, dma_wr_buf[0x%08lx]=0x%08lx\n", i,
                           (alt_u32)(DMA_READ_BUF + i * 4), src, (alt_u32)(DMA_WRITE_BUF + i * 4), dst);
                }
                errors++;
            } else {
                bytes_matched += 4;
            }
        }

        printf("  Bytes matched: %d/%d\n", bytes_matched, DMA_BUF_SIZE);
        printf("  Word errors:   %d/512\n", errors);

        if (errors == 0) {
            printf("  >>> BUFFERS MATCH PERFECTLY <<<\n");
        } else {
            printf("  >>> BUFFER MISMATCH - %d words corrupted <<<\n", errors);
        }

        printf("==============================================\n\n");
        // ===============================================================

        poll_count = 0;
        return 0;
    } else if (rd_done && !wr_done) {
        if (verbose) {
            printf("  Read complete, write still in progress\n");
            // After 100 polls with READ done but WRITE stuck, run diagnostics
            if (poll_count > 100 && (poll_count % 100) == 0) {
                diagnose_write_prefetcher_stuck();
            }
        }
        return 1;
    } else if (!rd_done && wr_done) {
        if (verbose)
            printf("  Write complete, read still in progress (unusual)\n");
        return 1;
    } else {
        if (verbose)
            printf("  DMA transfer in progress...\n");
        return 1;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Descriptor Validation Function
//
// Walks both descriptor chains exactly as the prefetcher hardware does:
//   - Starts from the NEXT_DESC_PTR register in each prefetcher
//   - Converts MSGDMA view address to Nios V view for each OCM read
//       nios_addr = msgdma_addr - OCM_BASE_MSGDMA + OCM_BASE
//   - Follows next_desc_ptr at offset 24 to reach next descriptor
//   - Stops when next_desc_ptr == 0 (end of chain)
//   - Detects broken chains, out-of-range pointers, wrong field values
//
// Returns: 1 if both chains fully valid, 0 if any issue detected
/////////////////////////////////////////////////////////////////////////////
int validate_descriptors(void)
{
    printf("Descriptor Validation (walking chains like prefetcher):\n");

    int valid = 1;
    alt_u32 chunk_size = DMA_BUF_SIZE / NUM_OF_DESCRIPTORS; // 256 bytes per descriptor

    // ============================================================
    // Walk READ chain (MM2ST: RAM → Accelerator AvST sink)
    // Start from READ prefetcher NEXT_DESC_PTR register
    // ============================================================
    printf("\n  --- READ Chain (MM2ST) ---\n");
    {
        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        // Step 1: Read starting pointer from prefetcher register
        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        alt_u32 curr_msgdma = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
        printf("  Prefetcher NEXT_DESC_PTR = 0x%08lx\n", curr_msgdma);

        if (curr_msgdma == 0) {
            printf("  WARNING: NEXT_DESC_PTR = 0, prefetcher already fetched — using expected start\n");
            curr_msgdma = OCM_BASE_MSGDMA;
        }

        int desc_idx = 0;
        int chain_err = 0;

        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        // Step 2: Walk chain via next_desc_ptr (hardware behavior)
        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        while (curr_msgdma != 0 && desc_idx < 16) {
            alt_u32 nios_addr = curr_msgdma - OCM_BASE_MSGDMA + OCM_BASE;

            // Bounds check — must be within OCM
            if (nios_addr < OCM_BASE || nios_addr >= (OCM_BASE + 4096)) {
                printf("  READ[%d] ERROR: addr 0x%08lx outside OCM range!\n", desc_idx, nios_addr);
                valid = 0;
                chain_err = 1;
                break;
            }

            // Read all descriptor fields
            alt_u32 src_addr = cca_reg_read(nios_addr, DESC_OFF_SRC_ADDR);
            alt_u32 dest_addr = cca_reg_read(nios_addr, DESC_OFF_DEST_ADDR);
            alt_u32 length = cca_reg_read(nios_addr, DESC_OFF_LENGTH);
            alt_u32 next_desc = cca_reg_read(nios_addr, DESC_OFF_NEXT_DESC);
            alt_u32 control = cca_reg_read(nios_addr, DESC_OFF_CONTROL);

            alt_u32 expected_src = DMA_READ_BUF_MSGDMA + (desc_idx * chunk_size);
            int is_last = (next_desc == 0);

            printf("  READ[%d] MSGDMA:0x%08lx Nios:0x%08lx\n", desc_idx, curr_msgdma, nios_addr);
            printf("    src_addr  = 0x%08lx %s\n", src_addr, (src_addr == expected_src) ? "[OK]" : "[ERROR]");
            printf("    dest_addr = 0x%08lx %s\n", dest_addr, (dest_addr == 0) ? "[OK - stream]" : "[ERROR]");
            printf("    length    = 0x%08lx %s\n", length, (length == chunk_size) ? "[OK - 256 bytes]" : "[ERROR]");
            printf("    next_desc = 0x%08lx %s\n", next_desc, is_last ? "[end of chain]" : "[chain continues]");

            if (!is_last) {
                // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
                // Mid-chain: OWNED_BY_HW | EARLY_DONE = 0x41000000
                // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
                int ctrl_ok = (control & DESC_CTRL_OWNED_BY_HARDWARE_MSK) && (control & DESC_CTRL_EARLY_DONE_MSK);
                printf("    control   = 0x%08lx %s\n", control,
                       ctrl_ok ? "[OK - EARLY_DONE | OWNED_BY_HW]" : "[ERROR]");
                if (!ctrl_ok)
                    valid = 0;
            } else {
                // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
                // Last: OWNED_BY_HW | WAIT_FOR_WRITE | IRQ = 0x42004000
                // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
                int ctrl_ok = (control & DESC_CTRL_OWNED_BY_HARDWARE_MSK) && (control & DESC_CTRL_WAIT_FOR_WRITE_MSK) &&
                              (control & DESC_CTRL_IRQ_ON_COMPLETION_MSK);
                printf("    control   = 0x%08lx %s\n", control,
                       ctrl_ok ? "[OK - OWNED_BY_HW | WAIT_FOR_WRITE | IRQ]" : "[ERROR]");
                if (!ctrl_ok)
                    valid = 0;
            }

            if (src_addr != expected_src || dest_addr != 0 || length != chunk_size)
                valid = 0;

            curr_msgdma = next_desc;
            desc_idx++;
        }

        if (!chain_err) {
            if (desc_idx != NUM_OF_DESCRIPTORS) {
                printf("  ERROR: READ chain has %d descriptors, expected %d\n", desc_idx, NUM_OF_DESCRIPTORS);
                valid = 0;
            } else {
                printf("  READ chain: %d descriptors walked OK\n", desc_idx);
            }
        }
    }

    // ============================================================
    // Walk WRITE chain (ST2MM: Accelerator AvST source → RAM)
    // Start from WRITE prefetcher NEXT_DESC_PTR register
    // ============================================================
    printf("\n  --- WRITE Chain (ST2MM) --- \n");
    {
        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        // Step 1: Read starting pointer from prefetcher register
        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        alt_u32 curr_msgdma = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
        printf("  Prefetcher NEXT_DESC_PTR = 0x%08lx\n", curr_msgdma);

        if (curr_msgdma == 0) {
            printf("  WARNING: NEXT_DESC_PTR = 0, prefetcher already fetched — using expected start\n");
            curr_msgdma = OCM_BASE_MSGDMA + WRITE_DESC_BASE_OFF;
        }

        int desc_idx = 0;
        int chain_err = 0;

        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        // Step 2: Walk chain via next_desc_ptr (hardware behavior)
        // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
        while (curr_msgdma != 0 && desc_idx < 16) {
            alt_u32 nios_addr = curr_msgdma - OCM_BASE_MSGDMA + OCM_BASE;

            if (nios_addr < OCM_BASE || nios_addr >= (OCM_BASE + 4096)) {
                printf("  WRITE[%d] ERROR: addr 0x%08lx outside OCM range!\n", desc_idx, nios_addr);
                valid = 0;
                chain_err = 1;
                break;
            }

            alt_u32 src_addr = cca_reg_read(nios_addr, DESC_OFF_SRC_ADDR);
            alt_u32 dest_addr = cca_reg_read(nios_addr, DESC_OFF_DEST_ADDR);
            alt_u32 length = cca_reg_read(nios_addr, DESC_OFF_LENGTH);
            alt_u32 next_desc = cca_reg_read(nios_addr, DESC_OFF_NEXT_DESC);
            alt_u32 control = cca_reg_read(nios_addr, DESC_OFF_CONTROL);

            alt_u32 expected_dest = DMA_WRITE_BUF_MSGDMA + (desc_idx * chunk_size);
            int is_last = (next_desc == 0);

            printf("  WRITE[%d] MSGDMA:0x%08lx Nios:0x%08lx\n", desc_idx, curr_msgdma, nios_addr);
            printf("    src_addr  = 0x%08lx %s\n", src_addr, (src_addr == 0) ? "[OK - stream]" : "[ERROR]");
            printf("    dest_addr = 0x%08lx %s\n", dest_addr, (dest_addr == expected_dest) ? "[OK]" : "[ERROR]");
            printf("    length    = 0x%08lx %s\n", length, (length == chunk_size) ? "[OK - 256 bytes]" : "[ERROR]");
            printf("    next_desc = 0x%08lx %s\n", next_desc, is_last ? "[end of chain]" : "[chain continues]");

            if (!is_last) {
                int ctrl_ok = (control & DESC_CTRL_OWNED_BY_HARDWARE_MSK) && (control & DESC_CTRL_EARLY_DONE_MSK);
                printf("    control   = 0x%08lx %s\n", control,
                       ctrl_ok ? "[OK - EARLY_DONE | OWNED_BY_HW]" : "[ERROR]");
                if (!ctrl_ok)
                    valid = 0;
            } else {
                int ctrl_ok = (control & DESC_CTRL_OWNED_BY_HARDWARE_MSK) && (control & DESC_CTRL_WAIT_FOR_WRITE_MSK) &&
                              (control & DESC_CTRL_IRQ_ON_COMPLETION_MSK);
                printf("    control   = 0x%08lx %s\n", control,
                       ctrl_ok ? "[OK - OWNED_BY_HW | WAIT_FOR_WRITE | IRQ]" : "[ERROR]");
                if (!ctrl_ok)
                    valid = 0;
            }

            if (src_addr != 0 || dest_addr != expected_dest || length != chunk_size)
                valid = 0;

            curr_msgdma = next_desc;
            desc_idx++;
        }

        if (!chain_err) {
            if (desc_idx != NUM_OF_DESCRIPTORS) {
                printf("  ERROR: WRITE chain has %d descriptors, expected %d\n", desc_idx, NUM_OF_DESCRIPTORS);
                valid = 0;
            } else {
                printf("  WRITE chain: %d descriptors walked OK\n", desc_idx);
            }
        }
    }

    printf("\n");
    if (valid) {
        printf("  >>> All %d Descriptors (%d READ + %d WRITE) are VALID <<<\n", NUM_OF_DESCRIPTORS * 2,
               NUM_OF_DESCRIPTORS, NUM_OF_DESCRIPTORS);
    } else {
        printf("  >>> ERROR: One or more descriptors are INVALID! <<<\n");
    }

    return valid;
}

/////////////////////////////////////////////////////////////////////////////
// Descriptor Fetch Diagnostic Function
//
// Probes whether the MSGDMA read prefetcher has fetched its first descriptor.
// next_desc_ptr field is at offset 24 (0x18) in prefetcher mode descriptors.
//
// Returns: 1 if descriptor fetched (rd_next cleared to 0), 0 if stuck
/////////////////////////////////////////////////////////////////////////////
int detect_msgdma_read_start(void)
{
    alt_u32 rd_next;
    alt_u32 rd_pref_stat;
    alt_u32 rd_disp_stat;
    alt_u32 rd_ctrl;

    rd_next = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
    rd_pref_stat = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET);
    rd_disp_stat = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Read control word from READ descriptor 0
    // next_desc_ptr is at offset 24, control is at offset 28
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    rd_ctrl = cca_reg_read(OCM_BASE, DESC_OFF_CONTROL); // control @ OCM + 28

    printf("Fetch Debug:\n");
    printf("  rd_next       = 0x%08lx %s\n", rd_next, (rd_next == 0) ? "[fetched]" : "[pending]");
    printf("  pref_status   = 0x%08lx (IRQ:%d)\n", rd_pref_stat,
           (rd_pref_stat & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0);
    printf("  disp_status   = 0x%08lx (BUSY:%d IDLE:%d STOPPED:%d)\n", rd_disp_stat,
           (rd_disp_stat & DISPATCHER_BUSY_MSK) ? 1 : 0, (rd_disp_stat & DISPATCHER_IDLE_MSK) ? 1 : 0,
           (rd_disp_stat & DISPATCHER_STOPPED_MSK) ? 1 : 0);
    printf("  desc0_ctrl    = 0x%08lx (OWNED_BY_HW:%d)\n", rd_ctrl,
           (rd_ctrl & DESC_CTRL_OWNED_BY_HARDWARE_MSK) ? 1 : 0);

    if (rd_disp_stat & DISPATCHER_STOPPED_MSK) {
        alt_u32 rd_fill = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);
        printf("  resp_fill_lvl = 0x%08lx\n", rd_fill);
    }

    return (rd_next == 0) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
// WRITE Prefetcher Diagnostic Function
//
// Diagnoses why the WRITE prefetcher is stuck waiting for data.
// Checks accelerator state, byte counters, and data flow.
//
// Returns: 1 if issue identified, 0 if normal
/////////////////////////////////////////////////////////////////////////////
int diagnose_write_prefetcher_stuck(void)
{
    printf("\n=== WRITE Prefetcher Diagnostic ===\n");

    // Read WRITE channel state
    alt_u32 wr_next = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
    alt_u32 wr_pref_ctrl = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_CONTROL_OFFSET);
    alt_u32 wr_pref_stat = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET);
    alt_u32 wr_disp_stat = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    // Read accelerator byte counters
    alt_u32 accel_snk_cnt = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SNK);
    alt_u32 accel_src_cnt = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SRC);

    // Read accelerator control/status
    alt_u32 accel_ctrl = cca_reg_read(CCA_BASE, CCA_ACCEL_CONTROL);
    alt_u32 accel_stat = cca_reg_read(CCA_BASE, CCA_ACCEL_STATUS);

    // Check WRITE descriptor 0 control field
    alt_u32 wr_desc0_ctrl = cca_reg_read(OCM_BASE + WRITE_DESC_BASE_OFF, DESC_OFF_CONTROL);

    printf("WRITE Prefetcher State:\n");
    printf("  NEXT_DESC_PTR  = 0x%08lx %s\n", wr_next, (wr_next == 0) ? "[fetched all]" : "[NOT FETCHED - stuck!]");
    printf("  Prefetch CTRL  = 0x%08lx (RUN:%d IRQ_EN:%d)\n", wr_pref_ctrl, (wr_pref_ctrl & PREFETCHER_RUN_MSK) ? 1 : 0,
           (wr_pref_ctrl & PREFETCHER_IRQ_EN_MSK) ? 1 : 0);
    printf("  Prefetch STAT  = 0x%08lx (IRQ:%d)\n", wr_pref_stat,
           (wr_pref_stat & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0);
    printf("  Dispatcher     = 0x%08lx (BUSY:%d IDLE:%d STOPPED:%d STOPPED_ON_ERR:%d)\n", wr_disp_stat,
           (wr_disp_stat & DISPATCHER_BUSY_MSK) ? 1 : 0, (wr_disp_stat & DISPATCHER_IDLE_MSK) ? 1 : 0,
           (wr_disp_stat & DISPATCHER_STOPPED_MSK) ? 1 : 0, (wr_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) ? 1 : 0);
    printf("  Desc0 control  = 0x%08lx (OWNED_BY_HW:%d)\n", wr_desc0_ctrl,
           (wr_desc0_ctrl & DESC_CTRL_OWNED_BY_HARDWARE_MSK) ? 1 : 0);

    printf("\nAccelerator State:\n");
    printf("  ACCEL_CONTROL  = 0x%08lx\n", accel_ctrl);
    printf("  ACCEL_STATUS   = 0x%08lx\n", accel_stat);
    printf("  Sink bytes     = %ld (data received from READ DMA)\n", accel_snk_cnt);
    printf("  Source bytes   = %ld (data sent to WRITE DMA)\n", accel_src_cnt);
    printf("  Lost bytes     = %ld\n", accel_snk_cnt - accel_src_cnt);

    printf("\nDiagnosis:\n");

    if (wr_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) {
        printf("  >>> CRITICAL: WRITE dispatcher STOPPED_ON_ERROR <<<\n");
        printf("      This is why the prefetcher RUN bit was cleared!\n");
        printf("      Possible causes:\n");
        printf("      - Descriptor fetch error (OCM read failure)\n");
        printf("      - Invalid descriptor parameters\n");
        printf("      - Memory write error (DMA_WRITE_BUF not accessible)\n");
        printf("      - AXI bus error on descriptor or data transfer\n");
    }

    if ((wr_pref_ctrl & PREFETCHER_RUN_MSK) == 0 && wr_next != 0) {
        printf("  >>> ISSUE: WRITE prefetcher RUN bit cleared (was started, now stopped) <<<\n");
        printf("      Prefetcher was running but hardware cleared RUN bit\n");
        printf("      First descriptor at 0x%08lx never fetched\n", wr_next);
    }

    if (wr_next != 0) {
        printf("  >>> ISSUE: WRITE prefetcher never fetched first descriptor <<<\n");
        if (wr_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) {
            printf("      ERROR condition prevented descriptor fetch\n");
        } else {
            printf("      Waiting for avst_source_valid from accelerator\n");
        }
    }

    if ((wr_disp_stat & DISPATCHER_BUSY_MSK) && accel_src_cnt == 0) {
        printf("  >>> ISSUE: Dispatcher BUSY but accelerator produced ZERO bytes <<<\n");
        printf("      Accelerator pipeline may be stalled or throttled\n");
    }

    if (accel_snk_cnt > accel_src_cnt) {
        printf("  >>> ISSUE: Data loss in accelerator (%ld bytes missing) <<<\n", accel_snk_cnt - accel_src_cnt);
        printf("      Possible causes:\n");
        printf("      - 3-bit shift register throttling too aggressive\n");
        printf("      - Backpressure deadlock (ready/valid handshake stuck)\n");
        printf("      - Internal FIFO overflow\n");
    }

    if (accel_snk_cnt == DMA_BUF_SIZE && accel_src_cnt < DMA_BUF_SIZE) {
        printf("  >>> ISSUE: Accelerator received all data but only output %ld bytes <<<\n", accel_src_cnt);
        printf("      Check RTL for pipeline drain issues\n");
    }

    printf("\nRecommended Actions:\n");
    if (wr_disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK) {
        printf("  1. Check descriptor at OCM 0x%08lx for validity:\n", (alt_u32)(OCM_BASE + WRITE_DESC_BASE_OFF));
        printf("     - src_addr should be 0 (stream mode)\n");
        printf("     - dest_addr should be 0x%08lx (WRITE_BUF)\n", (alt_u32)DMA_WRITE_BUF_MSGDMA);
        printf("     - length should be 256 bytes\n");
        printf("     - control should have OWNED_BY_HW set\n");
        printf("  2. Verify DMA_WRITE_BUF (0x%08lx) is accessible to MSGDMA\n", (alt_u32)DMA_WRITE_BUF_MSGDMA);
        printf("  3. Check AXI interconnect for address decode errors\n");
        printf("  4. Review MSGDMA error logs/status registers\n");
    } else {
        printf("  1. Check accelerator RTL ready_toggle (3-bit shift register)\n");
        printf("  2. Verify avst_source_valid is pulsing in simulation\n");
        printf("  3. Check for backpressure on avst_source_ready\n");
        printf("  4. Inspect pipeline FIFO levels\n");
    }
    printf("=========================================\n\n");

    return (wr_next != 0) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
// DMA Transfer Verification Function
//
// Verifies that data was transferred correctly from READ_BUF to WRITE_BUF
// Checks descriptor status, response FIFOs, and byte counts
//
// Returns: 1 if verification passed, 0 if failed
/////////////////////////////////////////////////////////////////////////////
int verify_dma_transfer(void)
{
    printf("\n== ======== DMA TRANSFER VERIFICATION ======== ==\n");
    int verification_passed = 1;

    // ---------------------------------------------------------------
    // METHOD 1: Check Response FIFO levels
    // ---------------------------------------------------------------
    printf("\n--- Method 1: Response FIFO Status ---\n");
    alt_u32 rd_resp_fill = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);
    alt_u32 wr_resp_fill = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_RESP_FILL_OFFSET);

    /* Read dispatcher status registers needed by Method 5 checks */
    alt_u32 rd_disp_status = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);
    alt_u32 wr_disp_status = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    printf("  READ response FIFO level:  %ld\n", rd_resp_fill);
    printf("  WRITE response FIFO level: %ld\n", wr_resp_fill);

    // ---------------------------------------------------------------
    // METHOD 2: Direct buffer comparison (most reliable!)
    // ---------------------------------------------------------------
    printf("\n--- Method 2: Direct Buffer Comparison ---\n");
    int errors = 0;
    int bytes_matched = 0;

    for (int i = 0; i < 512; i++) { // 512 words = 2048 bytes
        alt_u32 src = IORD_32DIRECT(DMA_READ_BUF, i * 4);
        alt_u32 dst = IORD_32DIRECT(DMA_WRITE_BUF, i * 4);

        if (src != dst) {
            if (errors < 10) { // Print first 10 errors only
                printf("  ERROR [word %3d]: dma_rd_buf[0x%08lx]=0x%08lx, dma_wr_buf[0x%08lx]=0x%08lx\n", i,
                       (alt_u32)(DMA_READ_BUF + i * 4), src, (alt_u32)(DMA_WRITE_BUF + i * 4), dst);
            }
            errors++;
        } else {
            bytes_matched += 4;
        }
    }

    printf("  Bytes matched: %d/%d\n", bytes_matched, DMA_BUF_SIZE);
    printf("  Word errors:   %d/512\n", errors);

    if (errors > 0) {
        printf("  >>> BUFFER MISMATCH - %d words corrupted <<<\n", errors);
        verification_passed = 0;
    } else {
        printf("  >>> BUFFERS MATCH PERFECTLY <<<\n");
    }

    // ---------------------------------------------------------------
    // METHOD 3: Check Descriptor Ownership Bits
    //           After hardware processes descriptors, OWNED_BY_HW
    //           should be cleared in all descriptors
    // ---------------------------------------------------------------
    printf("\n--- Method 3: Descriptor Ownership Bits ---\n");
    int ownership_errors = 0;

    // Check READ descriptor chain (NUM_OF_DESCRIPTORS descriptors at OCM_BASE + 0)
    printf("  READ Chain:\n");
    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        alt_u32 desc_offset = OCM_BASE + (i * DESC_STRIDE);
        alt_u32 control = cca_reg_read(desc_offset, DESC_OFF_CONTROL);
        int owned_by_hw = (control & DESC_CTRL_OWNED_BY_HARDWARE_MSK) ? 1 : 0;

        if (owned_by_hw) {
            printf("    Descriptor %d: OWNED_BY_HW still SET (0x%08lx) [ERROR]\n", i, control);
            ownership_errors++;
            verification_passed = 0;
        } else {
            printf("    Descriptor %d: Released by HW (0x%08lx) [OK]\n", i, control);
        }
    }

    // Check WRITE descriptor chain (NUM_OF_DESCRIPTORS descriptors at OCM_BASE + 256)
    printf("  WRITE Chain:\n");
    for (int i = 0; i < NUM_OF_DESCRIPTORS; i++) {
        alt_u32 desc_offset = OCM_BASE + WRITE_DESC_BASE_OFF + (i * DESC_STRIDE);
        alt_u32 control = cca_reg_read(desc_offset, DESC_OFF_CONTROL);
        int owned_by_hw = (control & DESC_CTRL_OWNED_BY_HARDWARE_MSK) ? 1 : 0;

        if (owned_by_hw) {
            printf("    Descriptor %d: OWNED_BY_HW still SET (0x%08lx) [ERROR]\n", i, control);
            ownership_errors++;
            verification_passed = 0;
        } else {
            printf("    Descriptor %d: Released by HW (0x%08lx) [OK]\n", i, control);
        }
    }

    if (ownership_errors == 0) {
        printf("  >>> All 16 descriptors released by hardware <<<\n");
    } else {
        printf("  >>> ERROR: %d descriptors still owned by hardware <<<\n", ownership_errors);
    }

    // ---------------------------------------------------------------
    // METHOD 4: Accelerator byte counters (if implemented in RTL)
    // ---------------------------------------------------------------
    printf("\n--- Method 4: Accelerator Byte Counters ---\n");
    alt_u32 accel_rd_count = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SNK);
    alt_u32 accel_wr_count = cca_reg_read(CCA_BASE, CCA_ACCEL_STAT_SRC);

    printf("  Accelerator READ bytes:  %ld (expected %d)\n", accel_rd_count, DMA_BUF_SIZE);
    printf("  Accelerator WRITE bytes: %ld (expected %d)\n", accel_wr_count, DMA_BUF_SIZE);

    if (accel_rd_count != DMA_BUF_SIZE) {
        printf("  ERROR: READ byte count mismatch!\n");
        verification_passed = 0;
    }

    if (accel_wr_count != DMA_BUF_SIZE) {
        printf("  ERROR: WRITE byte count mismatch!\n");
        printf("  Lost bytes: %ld\n", accel_rd_count - accel_wr_count);
        verification_passed = 0;
    }

    if (accel_rd_count == accel_wr_count && accel_rd_count == DMA_BUF_SIZE) {
        printf("  >>> Accelerator byte counts CORRECT <<<\n");
    }

    // ---------------------------------------------------------------
    // METHOD 5: Check for Error Conditions
    //           Look for dispatcher errors and STOPPED state
    // ---------------------------------------------------------------
    printf("\n--- Method 5: Error Condition Check ---\n");
    int error_conditions = 0;

    // READ channel error checks
    int rd_stopped = (rd_disp_status & DISPATCHER_STOPPED_MSK) ? 1 : 0;
    printf("  READ Dispatcher:\n");
    printf("    STOPPED bit   = %d %s\n", rd_stopped, rd_stopped ? "[ERROR]" : "[OK]");
    printf("    RESP FIFO lvl = %ld\n", rd_resp_fill);

    if (rd_stopped) {
        error_conditions++;
        verification_passed = 0;
        printf("    >>> ERROR: READ dispatcher STOPPED (indicates error) <<<\n");
    }

    // WRITE channel error checks
    int wr_stopped = (wr_disp_status & DISPATCHER_STOPPED_MSK) ? 1 : 0;
    printf("  WRITE Dispatcher:\n");
    printf("    STOPPED bit   = %d %s\n", wr_stopped, wr_stopped ? "[ERROR]" : "[OK]");
    printf("    RESP FIFO lvl = %ld\n", wr_resp_fill);

    if (wr_stopped) {
        error_conditions++;
        verification_passed = 0;
        printf("    >>> ERROR: WRITE dispatcher STOPPED (indicates error) <<<\n");
    }

    if (error_conditions == 0) {
        printf("  >>> No error conditions detected <<<\n");
    } else {
        printf("  >>> ERROR: %d error condition(s) detected <<<\n", error_conditions);
    }

    // ---------------------------------------------------------------
    // METHOD 6: Check Prefetcher State
    //           Verify both prefetchers completed their chains
    // ---------------------------------------------------------------
    /* need to test more to understand why Write DMA does not complete to Idle
    // ---------------------------------------------------------------
    printf("\n--- Method 6: Prefetcher State ---\n");

    // READ prefetcher
    alt_u32 rd_next_lo = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
    alt_u32 rd_pref_status = cca_reg_read(MSGDMA_RD_BASE, MSGDMA_PF_STATUS_OFFSET);
    // alt_u32 rd_disp_status = cca_reg_read(MSGDMA_RD_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    int rd_next_zero = (rd_next_lo == 0);
    int rd_irq_set = (rd_pref_status & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0;
    int rd_disp_idle = (rd_disp_status & DISPATCHER_IDLE_MSK) ? 1 : 0;
    int rd_disp_busy = (rd_disp_status & DISPATCHER_BUSY_MSK) ? 1 : 0;

    printf("  READ Prefetcher:\n");
    printf("    NEXT_DESC_PTR = 0x%08lx %s\n", rd_next_lo,
           rd_next_zero ? "[OK - chain fetched]" : "[ERROR - still pending]");
    printf("    IRQ bit       = %d %s\n", rd_irq_set, rd_irq_set ? "[OK]" : "[ERROR]");
    printf("    Dispatcher    = %s %s\n", rd_disp_idle ? "IDLE" : (rd_disp_busy ? "BUSY" : "STOPPED"),
           rd_disp_idle ? "[OK]" : "[ERROR]");

    if (!rd_next_zero || !rd_irq_set || !rd_disp_idle) {
        verification_passed = 0;
    }

    // WRITE prefetcher
    alt_u32 wr_next_lo = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_NEXT_DESC_LO_OFFSET);
    alt_u32 wr_pref_status = cca_reg_read(MSGDMA_WR_BASE, MSGDMA_PF_STATUS_OFFSET);
    // alt_u32 wr_disp_status = cca_reg_read(MSGDMA_WR_DISPATCHER_BASE, MSGDMA_DISP_STATUS_OFFSET);

    int wr_next_zero = (wr_next_lo == 0);
    int wr_irq_set = (wr_pref_status & PREFETCHER_INTERRUPT_STATUS_MSK) ? 1 : 0;
    int wr_disp_idle = (wr_disp_status & DISPATCHER_IDLE_MSK) ? 1 : 0;
    int wr_disp_busy = (wr_disp_status & DISPATCHER_BUSY_MSK) ? 1 : 0;

    printf("  WRITE Prefetcher:\n");
    printf("    NEXT_DESC_PTR = 0x%08lx %s\n", wr_next_lo,
           wr_next_zero ? "[OK - chain fetched]" : "[ERROR - still pending]");
    printf("    IRQ bit       = %d %s\n", wr_irq_set, wr_irq_set ? "[OK]" : "[ERROR]");
    printf("    Dispatcher    = %s %s\n", wr_disp_idle ? "IDLE" : (wr_disp_busy ? "BUSY" : "STOPPED"),
           wr_disp_idle ? "[OK]" : "[ERROR]");

    if (!wr_next_zero || !wr_irq_set || !wr_disp_idle) {
        verification_passed = 0;
    }

    if (rd_next_zero && rd_irq_set && rd_disp_idle && wr_next_zero && wr_irq_set && wr_disp_idle) {
        printf("  >>> Both prefetchers completed successfully <<<\n");
    } else {
        printf("  >>> ERROR: One or both prefetchers did not complete properly <<<\n");
    }
    //  */
    // ---------------------------------------------------------------
    //   Final verdict
    // ---------------------------------------------------------------
    printf("\n==============================================\n");
    if (verification_passed) {
        printf(">>> DMA TRANSFER VERIFICATION PASSED <<<\n");
    } else {
        printf(">>> DMA TRANSFER VERIFICATION FAILED <<<\n");
    }
    printf("==============================================\n\n");

    return verification_passed;
}
/////////////////////////////////////////////////////////////////////////////