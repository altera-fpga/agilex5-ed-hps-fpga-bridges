/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator Test Tasks Header
 * Function prototypes for CCA subsystem verification tests
 */

#ifndef __CCA_TEST_TASKS_H__
#define __CCA_TEST_TASKS_H__

/////////////////////////////////////////////////////////////////////////////
// Test Function Prototypes
//
// All test functions return:
//   1 (test_pass) if test passed
//   0 (test_fail) if test failed
/////////////////////////////////////////////////////////////////////////////

/**
 * test_cca_csr_regs - Test CCA CSR identification and reserved registers
 *
 * Verifies that FPGA_ID, FPGA_REV, and reserved registers return expected
 * hardware-defined values.
 *
 * Returns: 1 if all registers match expected values, 0 otherwise
 */
int test_cca_csr_regs(void);

/**
 * test_cca_debug_regs - Test CCA debug register write/read functionality
 *
 * Writes test patterns to debug registers and verifies readback values
 * to ensure register read/write integrity.
 *
 * Returns: 1 if all registers match written values, 0 otherwise
 */
int test_cca_debug_regs(void);

/**
 * test_cca_debug_loopback - Test CCA debug register loopback/swap
 *
 * Reads debug registers, rotates values, writes back, and verifies.
 * Tests register read/write integrity and data path.
 *
 * Returns: 1 if loopback test passes, 0 otherwise
 */
int test_cca_debug_loopback(void);

/**
 * test_cca_ocm - Test CCA OCM descriptor memory
 *
 * Writes test patterns to OCM descriptor memory and verifies readback
 * to ensure OCM integrity for descriptor storage.
 *
 * Returns: 1 if all OCM locations match written values, 0 otherwise
 */
int test_cca_ocm(void);

/**
 * test_cca_ram - Test CCA RAM memory
 *
 * Writes test patterns to RAM memory and verifies readback
 * to ensure RAM integrity for descriptor storage.
 *
 * Returns: 1 if all RAM locations match written values, 0 otherwise
 */
int test_cca_ram(void);

/**
 * test_msgdma_wr - Test MSGDMA write dispatcher registers
 *
 * Writes test patterns to MSGDMA write dispatcher CSR registers and
 * verifies readback to ensure register integrity.
 *
 * Returns: 1 if all registers match written values, 0 otherwise
 */
int test_msgdma_wr(void);

/**
 * test_msgdma_rd - Test MSGDMA read dispatcher registers
 *
 * Writes test patterns to MSGDMA read dispatcher CSR registers and
 * verifies readback to ensure register integrity.
 *
 * Returns: 1 if all registers match written values, 0 otherwise
 */
int test_msgdma_rd(void);

/**
 * verify_dma_write_buf - Verify DMA_WRITE_BUF contents after accelerator
 *
 * Reads and prints the contents of DMA_WRITE_BUF after the DMA/accelerator test.
 * Optionally, compares against an expected pattern if provided.
 *
 * @expected: pointer to expected data (512 words) or NULL to skip compare
 * @print_all: if nonzero, print all entries; else print first/last 8
 * Returns: 1 if all values match expected (if provided), 0 otherwise
 */
int verify_dma_write_buf(const alt_u32 *expected, int print_all);

/**
 * validate_descriptors - Validate descriptor format in OCM memory
 *
 * This is a test function. 
 * Reads both u_msgdma_read (MM2ST) and u_msgdma_write (ST2MM) descriptors 
 * from OCM and validates their format. 
 * Checks that MM2ST has dest_addr=0 (stream), ST2MM has src_addr=0 (stream), 
 * and all other fields match expected values.
 *
 * Returns: 1 if both descriptors are valid, 0 if any issues detected
 */
int validate_descriptors(void);

/**
 * detect_msgdma_read_start - Probe whether the MSGDMA read channel started
 *
 * Performs a non-invasive, read-only probe of MSGDMA read CSRs and the
 * OCM descriptor control word to determine whether the read prefetcher
 * accepted the descriptor pointer or the hardware claimed ownership of
 * the descriptor. Useful for fail-fast checks immediately after
 * writing the NEXT_DESC CSR and for polling during debug.
 *
 * Returns: 1 if the read channel appears to have started (pointer
 * accepted or descriptor owned by hardware), 0 otherwise.
 */
int detect_msgdma_read_start(void);

/**
 * diagnose_write_prefetcher_stuck - Diagnose stuck WRITE prefetcher
 *
 * Diagnoses why the WRITE prefetcher is stuck waiting for data.
 * Checks accelerator state, byte counters, and data flow.
 *
 * Returns: 1 if issue identified, 0 if normal
 */
int diagnose_write_prefetcher_stuck(void);

#endif /* __CCA_TEST_TASKS_H__ */
