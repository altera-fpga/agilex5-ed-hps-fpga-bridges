/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator Main Application
 * ----------------------------------------------------------------------
 * Implements the main application logic, including initialization,
 * test execution, DMA transfer, and verification for RAM and DMA subsystems.
 * ----------------------------------------------------------------------
 */
// Define environment before including headers
#define __NIOS_V__

// System headers
#include "io.h" // IOWR_32DIRECT, IORD_32DIRECT
#include "system.h" // System definitions

#include <stdio.h> // printf()
#include <unistd.h> // usleep()
#include <stdlib.h> // malloc(), free()
#include <string.h> // memcpy()
#include <stdint.h> // uint32_t, uint64_t, etc.

#include "alt_types.h"
#include "altera_avalon_pio_regs.h"

#include "sys/alt_irq.h"

// Cache Coherent Accelerator HAL (unified API)
#include "cca_hal.h"
// Test function prototypes
#include "cca_tasks.h"

/////////////////////////////////////////////////////////////////////////////
// PORTING NOTE: To port this code to HPS FreeRTOS:
//
// 1. Remove the __NIOS_V__ define (or undefine it)
// 2. Update base addresses for HPS memory map (typically LWH2F bridge base)
// 3. Replace printf() with PRINT() or appropriate logging function
// 4. Replace PIO operations with HPS GPIO or bridge control functions
// 5. The cca_hal.h header provides unified register access for both environments
//
// The register access functions (cca_reg_write/cca_reg_read) work in both
// environments via the cca_hal.h abstraction layer.
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// Test-Specific Constants
// These values are used only for testing and verification and should not
// be moved to the HAL layer
/////////////////////////////////////////////////////////////////////////////
// Test phase debug markers (for HCI_DBG_REG0/1)
#define NIOS_START_BIT (0x1 << 31)
#define NIOS_PASS_BIT (0x1 << 30)
#define NIOS_DONE_BIT (0x1 << 29)
#define NIOS_CCA_BIT (0x1 << 28)
#define NIOS_OCM_BIT (0x1 << 27)
#define NIOS_BUF_BIT (0x1 << 25)
#define NIOS_BAD_BIT (0x1 << 23)
#define NIOS_OK_BIT (0x1 << 22)
/////////////////////////////////////////////////////////////////////////////
int main()
{
    /////////////////////////////////////////////////////////////////////////////
    // Set Nios Start
    /////////////////////////////////////////////////////////////////////////////
    //  NIOS: Example Nios Design
    printf("NIOS: Example Nios Design\n");
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) | NIOS_START_BIT);
    // Nios V is inside the FPGA — CCA CSR space accessible immediately
    CCA_STEP(CCA_STEP_MAIN_START, "Nios V started: CCA CSR space accessible");

    PRINT("==========================================");
    PRINT("== CCA FreeRTOS Design Example Starting ==");
    PRINT("==========================================\n");

    /////////////////////////////////////////////////////////////
    // CCA CSR Register Reads Test
    /////////////////////////////////////////////////////////////
    // Set CCA bit to indicate test start (also serves as a debug marker in HCI_DBG_REG0/1)
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) | NIOS_CCA_BIT);
    // Step 2: LWH2F task — CCA CSR init via LWH2F bridge
    // Validates registers, resets HW, creates semaphore, registers ISRs
    CCA_STEP(CCA_STEP_MAIN_LWH2F, "Step 2: calling lwh2f_task");
    lwh2f_task(TEN_SECONDS);
    PRINT("LWH2F task completed.\n");
    // Clear CCA bit to indicate test completion
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) & ~NIOS_CCA_BIT);

    /////////////////////////////////////////////////////////////
    // CCA OCM Descriptor Wr/Rd Test
    /////////////////////////////////////////////////////////////
    // Set OCM bit to indicate test start (also serves as a debug marker in HCI_DBG_REG0/1)
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) | NIOS_OCM_BIT);
    // Step 3: F2H task — CCA DMA transfers via ACE5-Lite/CCU path
    // Allocates buffers, sets up descriptors, runs transfers, verifies results
    CCA_STEP(CCA_STEP_MAIN_F2H, "Step 3: calling f2h_task");
    f2h_task(TEN_SECONDS);
    PRINT("F2H task completed.\n");
    // Clear OCM bit to indicate test completion
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) & ~NIOS_OCM_BIT);

    /////////////////////////////////////////////////////////////////////////////
    //       //       //       TB test pass/fail check       //       //       //
    /////////////////////////////////////////////////////////////////////////////
    // Set test pass
    CCA_STEP(CCA_STEP_MAIN_DONE, "All steps complete");
    printf("NIOS: Tests pass\n");
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) | NIOS_PASS_BIT);
    goto done;

    /////////////////////////////////////////////////////////////
    //       Set test not passed          //
    /////////////////////////////////////////////////////////////
fail:
    (void)0; // Null statement for label (required in C)
    printf("NIOS: Tests fail\n");
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) & ~NIOS_PASS_BIT);

    /////////////////////////////////////////////////////////////
    //       Set test done          //
    /////////////////////////////////////////////////////////////
done:
    IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, IORD_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE) | NIOS_DONE_BIT);

    /////////////////////////////////////////////////////////////
    // Never ending loop
    /////////////////////////////////////////////////////////////
    usleep(10000000);
    while (1) {
        asm("nop");
        asm("nop");
        asm("nop");
    }
    return 0;
    /////////////////////////////////////////////////////////////
}