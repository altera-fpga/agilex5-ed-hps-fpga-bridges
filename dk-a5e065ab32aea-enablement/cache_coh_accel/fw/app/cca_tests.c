/*
  * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
  *
  * SPDX-License-Identifier: MIT-0
  *
  * Cache Coherent Accelerator Test Tasks
  * ----------------------------------------------------------------------
  * Contains test functions for CCA subsystem verification
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
// Test Function: CCA CSR Register Read Test
//
// Verifies CCA identification and reserved registers return expected values.
// Tests FPGA_ID, FPGA_REV, RESERVED_08, and RESERVED_0C registers.
//
// Returns: 1 (pass) if all registers match expected values, 0 (fail) otherwise
/////////////////////////////////////////////////////////////////////////////
int test_cca_csr_regs(void)
{
    alt_u32 ret;

    // write test identifier to debug reg
    printf("NIOS: Writing TEST_REV (0x%08lx) to CCA_DBG_REG10 at addr 0x%08lx\n", (alt_u32)TEST_REV,
           (alt_u32)(CCA_BASE + CCA_DBG_REG10));
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_REV);

    printf("NIOS: Reading CCA FPGA ID register ...\n");
    ret = cca_reg_read(CCA_BASE, CCA_FPGA_ID);
    if (ret != CCA_FPGA_ID_VAL) {
        printf("NIOS: CCA FPGA ID rd failed, expected 0x%08lx, got 0x%08lx\n", (alt_u32)CCA_FPGA_ID_VAL, ret);
        return 0;
    } else {
        printf("NIOS: CCA FPGA ID rd passed, got 0x%08lx\n", ret);
    }

    printf("NIOS: Reading CCA FPGA Revision register ...\n");
    ret = cca_reg_read(CCA_BASE, CCA_FPGA_REV);
    if (ret != CCA_FPGA_REV_VAL) {
        printf("NIOS: CCA FPGA Revision rd failed, expected 0x%08lx, got 0x%08lx\n", (alt_u32)CCA_FPGA_REV_VAL, ret);
        return 0;
    } else {
        printf("NIOS: CCA FPGA Revision rd passed, got 0x%08lx\n", ret);
    }

    printf("NIOS: Reading CCA RESERVED_08 register ...\n");
    ret = cca_reg_read(CCA_BASE, CCA_RESERVED_08);
    if (ret != CCA_RESERVED_08_VAL) {
        printf("NIOS: CCA RESERVED_08 rd failed, expected 0x%08lx, got 0x%08lx\n", (alt_u32)CCA_RESERVED_08_VAL, ret);
        return 0;
    } else {
        printf("NIOS: CCA RESERVED_08 rd passed, got 0x%08lx\n", ret);
    }

    printf("NIOS: Reading CCA RESERVED_0C register ...\n");
    ret = cca_reg_read(CCA_BASE, CCA_RESERVED_0C);
    if (ret != CCA_RESERVED_0C_VAL) {
        printf("NIOS: CCA RESERVED_0C rd failed, expected 0x%08lx, got 0x%08lx\n", (alt_u32)CCA_RESERVED_0C_VAL, ret);
        return 0;
    } else {
        printf("NIOS: CCA RESERVED_0C rd passed, got 0x%08lx\n", ret);
    }

    // write test identifier to debug reg to indicate test end
    printf("NIOS: Writing TEST_END (0x%08lx) to CCA_DBG_REG10 at addr 0x%08lx\n", (alt_u32)TEST_END,
           (alt_u32)(CCA_BASE + CCA_DBG_REG10));
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_END);

    return 1;
}

/////////////////////////////////////////////////////////////////////////////
// Test Function: CCA Debug Register Write/Read Test
//
// Writes test patterns to CCA debug registers and verifies readback values.
// Tests DBG_TST_REG and DBG_REG0-6.
//
// Returns: 1 (pass) if all registers match written values, 0 (fail) otherwise
/////////////////////////////////////////////////////////////////////////////
int test_cca_debug_regs(void)
{
    alt_u32 ret;

    printf("NIOS: Starting CCA Debug Register rd/wr test...\n");
    printf("NIOS: CCA_BASE = 0x%08lx, CCA_DBG_REG10 offset = 0x%08lx\n", (alt_u32)CCA_BASE, (alt_u32)CCA_DBG_REG10);

    // write test identifier to debug reg
    printf("NIOS: Writing TEST_CCA (0x%08lx) to CCA_DBG_REG10 at addr 0x%08lx\n", (alt_u32)TEST_CCA,
           (alt_u32)(CCA_BASE + CCA_DBG_REG10));
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_CCA);

    printf("NIOS: Writing to the CCA debug registers ...\n");
    cca_reg_write(CCA_BASE, CCA_DBG_TST_REG, DBG_TST_VAL_TST);
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
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, DBG_TST_VAL_0);

    printf("NIOS: Reading back from CCA debug registers ...\n");
    const alt_u32 dbg_vals[7] = { DBG_TST_VAL_0, DBG_TST_VAL_1, DBG_TST_VAL_2, DBG_TST_VAL_3,
                                  DBG_TST_VAL_4, DBG_TST_VAL_5, DBG_TST_VAL_6 };
    const alt_u32 dbg_regs[7] = { CCA_DBG_REG0, CCA_DBG_REG1, CCA_DBG_REG2, CCA_DBG_REG3,
                                  CCA_DBG_REG4, CCA_DBG_REG5, CCA_DBG_REG6 };

    for (int i = 0; i < 7; ++i) {
        printf("NIOS: read from CCA Debug register %d ...\n", i);
        ret = cca_reg_read(CCA_BASE, dbg_regs[i]);
        if (ret != dbg_vals[i]) {
            printf("NIOS: CCA Debug Reg %d rd/wr failed, expected 0x%08lx, got 0x%08lx\n", i, dbg_vals[i], ret);
            return 0;
        } else {
            printf("NIOS: CCA Debug Reg %d rd/wr passed, got 0x%08lx\n", i, ret);
        }
    }

    // write test identifier to debug reg to indicate test end
    printf("NIOS: Writing TEST_END (0x%08lx) to CCA_DBG_REG10 at addr 0x%08lx\n", (alt_u32)TEST_END,
           (alt_u32)(CCA_BASE + CCA_DBG_REG10));
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_END);

    printf("NIOS: CCA Debug Register test complete!\n");
    return 1;
}

/////////////////////////////////////////////////////////////////////////////
// Test Function: CCA Debug Register Loopback/Swap Test
//
// Reads debug registers, rotates values left by 1, writes them back, and
// verifies the swap operation. Tests register read/write integrity.
//
// Returns: 1 (pass) if all swapped values match, 0 (fail) otherwise
/////////////////////////////////////////////////////////////////////////////
int test_cca_debug_loopback(void)
{
    printf("\nNIOS: Starting CCA Debug Register loopback/swap test...\n");
    const alt_u32 dbg_offsets[] = { CCA_DBG_REG0, CCA_DBG_REG1, CCA_DBG_REG2, CCA_DBG_REG3,
                                    CCA_DBG_REG4, CCA_DBG_REG5, CCA_DBG_REG6, CCA_DBG_REG7 };
    alt_u32 dbg_values[8];

    // write test identifier to debug reg
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_CCA);

    // 1. Read current values
    for (int i = 0; i < 8; ++i) {
        dbg_values[i] = cca_reg_read(CCA_BASE, dbg_offsets[i]);
        printf("  Read reg[%d] = 0x%08lx\n", i, dbg_values[i]);
    }

    // 2. Swap pattern: rotate left by 1
    alt_u32 swapped[8];
    for (int i = 0; i < 8; ++i) {
        swapped[i] = dbg_values[(i + 1) % 8];
    }

    // 3. Write swapped values back
    for (int i = 0; i < 8; ++i) {
        cca_reg_write(CCA_BASE, dbg_offsets[i], swapped[i]);
        printf("  Write reg[%d] = 0x%08lx\n", i, swapped[i]);
    }

    // 4. Read back and verify
    for (int i = 0; i < 8; ++i) {
        alt_u32 val = cca_reg_read(CCA_BASE, dbg_offsets[i]);
        if (val != swapped[i]) {
            printf("  ERROR: reg[%d] expected 0x%08lx, got 0x%08lx\n", i, swapped[i], val);
            printf("NIOS: Debug register loopback/swap test FAILED\n");
            return 0;
        } else {
            printf("  OK: reg[%d] = 0x%08lx\n", i, val);
        }
    }

    // write test identifier to debug reg to indicate test end
    cca_reg_write(CCA_BASE, CCA_DBG_REG10, TEST_END);

    printf("NIOS: Debug register loopback/swap test PASSED\n");
    return 1;
}
/////////////////////////////////////////////////////////////////////////////
