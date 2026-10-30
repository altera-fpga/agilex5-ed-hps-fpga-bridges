/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator Hardware Abstraction Layer
 * Provides unified API for both Nios V and HPS FreeRTOS environments
 */
#ifndef __CCA_HAL_H__
#define __CCA_HAL_H__

#include <stdint.h>
#include "system.h" // Use HAL-generated base addresses
/////////////////////////////////////////////////////////////////////////////
// CCA Subsystem Base Addresses (from system.h)
// These macros alias the HAL-generated addresses for portability.
//
// IMPORTANT: MSGDMA has separate prefetcher and dispatcher CSR blocks:
//   - Prefetcher CSRs: next-desc pointer, control, status (used by firmware)
//   - Dispatcher CSRs: separate control/status (advanced use only)
//
// The primary MSGDMA_WR_BASE/MSGDMA_RD_BASE macros point to prefetcher CSRs
// since firmware configures descriptors via prefetcher registers.
/////////////////////////////////////////////////////////////////////////////
// This file doesn't currently seem like it supports both a Nios testing the SS and
// the HPS connected to the SS. Consider adding the base address of the SS at the top
// of the file and then all addresses within the SS being relative to that base and
// doing the math.
// That way a preprocessor define can change the base and the file would remain the same.
/////////////////////////////////////////////////////////////////////////////

// Primary MSGDMA base addresses (prefetcher CSRs)
#define MSGDMA_WR_BASE 0x30204000 // Prefetcher Write CSR Base
#define MSGDMA_RD_BASE 0x30204020 // Prefetcher Read CSR Base

// Explicit prefetcher base addresses (same as above, for clarity)
#define MSGDMA_WR_PREFETCHER_BASE 0x30204000 // Explicit Prefetcher Write CSR Base
#define MSGDMA_RD_PREFETCHER_BASE 0x30204020 // Explicit Prefetcher Read CSR Base

// Dispatcher CSR base addresses (status monitoring only - auto-managed by prefetcher)
#define MSGDMA_WR_DISPATCHER_BASE 0x30204040 // Dispatcher Write CSR Base
#define MSGDMA_RD_DISPATCHER_BASE 0x30204060 // Dispatcher Read CSR Base

// CCA accelerator CSR base
#define CCA_BASE 0x30400000 // Cache Coherent Accelerator CSR Base

// Descriptor OCM (used for CCA descriptors)
#define OCM_BASE 0x30200000 // Descriptor OCM Base (Nios V view)

// OCM base as seen by MSGDMA descriptor masters (different address space!)
// Nios V accesses OCM at 0x30200000, but MSGDMA descriptor masters see it at 0x20200000
#define OCM_BASE_MSGDMA 0x20200000

// Accelerator RAM (AXI4-attached, see tb.qsys and system.h)
#define RAM_BASE 0x20000000 // Accelerator RAM Base (Nios V view)

// RAM base as seen by MSGDMA data masters (different address space!)
// Nios V accesses RAM at 0x20000000, MSGDMA data masters see it at 0x60000000 (per tb.qsys line 2000)
#define RAM_BASE_MSGDMA 0x60000000

// DMA Buffer Regions in RAM (512KB total: lower 256KB for read, upper 256KB for write)
#define DMA_READ_BUF 0x20000000 // DMA Read  Buffer Base (lower 256KB of RAM)
#define DMA_WRITE_BUF 0x20040000 // DMA Write Buffer Base (upper 256KB of RAM, fixed at +256KB)
#define DMA_BUF_SIZE 0x20000 // 128KB max per buffer (32768 x 32-bit words)

// IRQ numbers (from BSP system.h — prefetcher CSRs raise these IRQs)
#define CCA_IRQ_MSGDMA_WRITE CACHE_COH_ACCEL_SUBSYS_0_U_MSGDMA_WRITE_PREFETCHER_CSR_IRQ
#define CCA_IRQ_MSGDMA_READ CACHE_COH_ACCEL_SUBSYS_0_U_MSGDMA_READ_PREFETCHER_CSR_IRQ

// DMA buffer addresses as seen by MSGDMA (for descriptor creation)
// RAM is 512KB (0x80000 bytes), write buffer at 256KB offset (0x40000)
// tb.qsys maps MSGDMA master to accelerator_ram at 0x60000000-0x60080000
#define DMA_READ_BUF_MSGDMA 0x60000000 // DMA Read  Buffer for MSGDMA (RAM base + 0)
#define DMA_WRITE_BUF_MSGDMA 0x60040000 // DMA Write Buffer for MSGDMA (RAM base + 256KB)

#define TEST_REV 0x01010101 // test internal CCA CSR
#define TEST_CCA 0x02020202 // test internal CCA CSR
#define TEST_OCM 0x03030303 // test OCM descriptor storage
#define TEST_RAM 0x04040404 // test RAM read/write
#define TEST_BUF 0x05050505 // test DMA operations
#define TEST_DMA 0x06060606 // test DMA operations
#define TEST_END 0x00ABBA00 // test end

/////////////////////////////////////////////////////////////////////////////
// CCA_DBG_REG10 Step Codes — SignalTap / SignalProbe breadcrumbs
//
// CCA_STEP(code, name) writes a distinct value to CCA_DBG_REG10 at each
// major execution step so the last value visible in SignalTap shows exactly
// where firmware was when something went sideways.
//
// Encoding: 0xCC_TT_00_SS
//   0xCC  : CCA design marker (always set)
//   TT=00 : main.c top-level
//   TT=10 : lwh2f_task  (bridge enable + CSR init)
//   TT=20 : f2h_task    (DMA buffer setup + transfers)
//   SS    : step number within task (01, 02, ...)
//   0xCCCC: task completed successfully
//   0xDEAD: task aborted on error
/////////////////////////////////////////////////////////////////////////////
// main.c top-level steps
#define CCA_STEP_MAIN_START 0xCC000001 // Nios V started, CCA CSR space accessible
#define CCA_STEP_MAIN_LWH2F 0xCC000002 // about to call lwh2f_task
#define CCA_STEP_MAIN_F2H 0xCC000003 // about to call f2h_task
#define CCA_STEP_MAIN_DONE 0xCC00CCCC // all steps complete (pass)

// lwh2f_task steps (first write visible only after LWH2F bridge is enabled on HPS;
// on Nios V the path is always open so CCA_STEP_LWH2F_BRIDGE_OK is the first stamp)
#define CCA_STEP_LWH2F_BRIDGE_OK 0xCC100001 // bridge enabled (or confirmed open on Nios V)
#define CCA_STEP_LWH2F_ID_REGS 0xCC100002 // cca_verify_id_regs
#define CCA_STEP_LWH2F_DBG_TEST 0xCC100003 // cca_debug_reg_test
#define CCA_STEP_LWH2F_ACCEL_INIT 0xCC100004 // cca_accel_init (reset + ALIVE wait)
#define CCA_STEP_LWH2F_MSGDMA_RD 0xCC100005 // msgdma_rd_init
#define CCA_STEP_LWH2F_MSGDMA_WR 0xCC100006 // msgdma_wr_init
#define CCA_STEP_LWH2F_ISR_REG 0xCC100007 // cca_isr_register
#define CCA_STEP_LWH2F_DONE 0xCC10CCCC // lwh2f_task complete (pass)
#define CCA_STEP_LWH2F_FAIL 0xCC10DEAD // lwh2f_task aborted (fail)

// f2h_task steps
#define CCA_STEP_F2H_START 0xCC200001 // f2h_task entered
#define CCA_STEP_F2H_BUF_ALLOC 0xCC200002 // alloc_dma_buffers
#define CCA_STEP_F2H_SRC_INIT 0xCC200003 // init_src_buffer
#define CCA_STEP_F2H_DST_INIT 0xCC200004 // init_dst_buffer
#define CCA_STEP_F2H_LOOP_START 0xCC200005 // transfer loop started
#define CCA_STEP_F2H_PASSTHRU 0xCC200006 // run_pass_through_test
#define CCA_STEP_F2H_PROCDATA 0xCC200007 // run_process_data_test
#define CCA_STEP_F2H_STATS 0xCC200008 // print_transfer_stats
#define CCA_STEP_F2H_DONE 0xCC20CCCC // f2h_task complete (pass)
#define CCA_STEP_F2H_FAIL 0xCC20DEAD // f2h_task aborted (fail)

// Writes code to CCA_DBG_REG10 (SignalTap) and logs name to console
#define CCA_STEP(code, name)                                        \
    do {                                                            \
        printf("[CCA_STEP 0x%08x] %s\n", (unsigned)(code), (name)); \
        cca_reg_write(CCA_BASE, CCA_DBG_REG10, (code));             \
    } while (0)

/////////////////////////////////////////////////////////////////////////////
// CCA Control and Status Register (CSR) Offsets
//
// Register map for the Cache Coherent Accelerator CSR block, defined in
// cca_accel_csr.sv. All offsets are relative to CCA_BASE.
//
// Register Access:
//   - ro: Read-only
//   - rw: Read-write
//   - Reserved registers return fixed values for verification
/////////////////////////////////////////////////////////////////////////////

// FPGA Identification Registers (0x00-0x0C)
#define CCA_FPGA_ID 0x00000000 // [ro] FPGA design identifier (major/minor/variant)
#define CCA_FPGA_REV 0x00000004 // [ro] FPGA revision number (major/minor/patch)
#define CCA_RESERVED_08 0x00000008 // [ro] Reserved - returns 0x08080808 for testing
#define CCA_RESERVED_0C 0x0000000C // [ro] Reserved - returns 0x0C0C0C0C for testing

// Accelerator Control and Status Registers (0x10-0x1C)
#define CCA_ACCEL_CONTROL 0x00000010 // [rw] Accelerator control register
#define CCA_ACCEL_STATUS 0x00000014 // [ro] Accelerator status register
#define CCA_ACCEL_STAT_SNK 0x00000018 // [ro] Accelerator sink byte counter status register
#define CCA_ACCEL_STAT_SRC 0x0000001C // [ro] Accelerator source byte counter status register

// Additional Reserved Registers (0x20-0x3C)
#define CCA_RESERVED_20 0x00000020 // [ro] Reserved - returns 0x20202020 for testing
#define CCA_RESERVED_24 0x00000024 // [ro] Reserved - returns 0x24242424 for testing
#define CCA_RESERVED_28 0x00000028 // [ro] Reserved - returns 0x28282828 for testing
#define CCA_RESERVED_2C 0x0000002C // [ro] Reserved - returns 0x2C2C2C2C for testing
#define CCA_RESERVED_30 0x00000030 // [ro] Reserved - returns 0x30303030 for testing
#define CCA_RESERVED_34 0x00000034 // [ro] Reserved - returns 0x34343434 for testing
#define CCA_RESERVED_38 0x00000038 // [ro] Reserved - returns 0x38383838 for testing
#define CCA_RESERVED_3C 0x0000003C // [ro] Reserved - returns 0x3C3C3C3C for testing

// Debug and Test Registers (0x40-0x6C)
#define CCA_DBG_TST_REG 0x00000040 // [rw] Debug test register
#define CCA_DBG_REG0 0x00000044 // [rw] Debug data register 0
#define CCA_DBG_REG1 0x00000048 // [rw] Debug data register 1
#define CCA_DBG_REG2 0x0000004C // [rw] Debug data register 2
#define CCA_DBG_REG3 0x00000050 // [rw] Debug data register 3
#define CCA_DBG_REG4 0x00000054 // [rw] Debug data register 4
#define CCA_DBG_REG5 0x00000058 // [rw] Debug data register 5
#define CCA_DBG_REG6 0x0000005C // [rw] Debug data register 6
#define CCA_DBG_REG7 0x00000060 // [rw] Debug data register 7 (iteration timing flags)
#define CCA_DBG_REG8 0x00000064 // [rw] Debug data register 8
#define CCA_DBG_REG9 0x00000068 // [rw] Debug data register 9
#define CCA_DBG_REG10 0x0000006C // [rw] Debug data register 10

// CCA_DBG_REG7 Iteration Timing Flags (for SignalTap/waveform analysis)
#define CCA_ITER_START_FLAG 0x55AA0000 // Start of test iteration
#define CCA_ITER_END_FLAG 0xAA550000 // End of test iteration
#define CCA_ITER_MASK 0x000000FF // Iteration number mask (bits 7:0)
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CCA Hardware Register Expected Values
//
// These constants define the hardware-defined read values for identification
// and reserved registers. They are used for RTL verification and hardware
// detection during software initialization.
//
// FPGA_ID encoding:  [31:16]=MAJOR,     [15: 8]=MINOR,     [ 7:0]=VARIANT
// FPGA_REV encoding: [31:24]=MAJOR_REL, [23:16]=MINOR_REL, [15:0]=PATCH
/////////////////////////////////////////////////////////////////////////////
#define CCA_FPGA_ID_VAL 0xCCA00101 // FPGA ID: v1.1 variant 1
#define CCA_FPGA_REV_VAL 0x01010003 // FPGA Revision: v1.1.3
#define CCA_RESERVED_08_VAL 0x08080808 // Reserved 0x08 verification pattern
#define CCA_RESERVED_0C_VAL 0x0C0C0C0C // Reserved 0x0C verification pattern
#define CCA_RESERVED_20_VAL 0x20202020 // Reserved 0x20 verification pattern
#define CCA_RESERVED_24_VAL 0x24242424 // Reserved 0x24 verification pattern
#define CCA_RESERVED_28_VAL 0x28282828 // Reserved 0x28 verification pattern
#define CCA_RESERVED_2C_VAL 0x2C2C2C2C // Reserved 0x2C verification pattern
#define CCA_RESERVED_30_VAL 0x30303030 // Reserved 0x30 verification pattern
#define CCA_RESERVED_34_VAL 0x34343434 // Reserved 0x34 verification pattern
#define CCA_RESERVED_38_VAL 0x38383838 // Reserved 0x38 verification pattern
#define CCA_RESERVED_3C_VAL 0x3C3C3C3C // Reserved 0x3C verification pattern

/////////////////////////////////////////////////////////////////////////////
// CCA_ACCEL_CONTROL Register Bit Definitions (offset 0x0010)
/////////////////////////////////////////////////////////////////////////////
#define CCA_CTRL_RESET (1 << 15) // Bit 15: RESET (self-clearing, 3-cycle pulse)
#define CCA_CTRL_ABORT (1 << 10) // Bit 10: ABORT (self-clearing)
#define CCA_CTRL_STOP (1 << 9) // Bit  9: STOP  (self-clearing)
#define CCA_CTRL_START (1 << 8) // Bit  8: START (self-clearing)
#define CCA_CTRL_CMD_MASK 0x000000FF // Bits 7:0: CMD field

// CMD field values
#define CCA_CMD_NOP 0x00 // No operation <-> ST_IDLE <-> ST_DONE
#define CCA_CMD_PROCESS_DATA 0x01 // Route through 8-stage inversion pipeline
#define CCA_CMD_PASS_THROUGH 0x02 // Bypass pipeline, direct AvST sink -> source
#define CCA_CMD_ERROR_TEST 0xFF // Force ST_ERROR for error path testing

/////////////////////////////////////////////////////////////////////////////
// CCA_ACCEL_STATUS Register Bit Definitions (offset 0x0014)
/////////////////////////////////////////////////////////////////////////////
#define CCA_STATUS_ACCEL_ERROR (1 << 4) // Bit 4: ACCEL_ERROR
#define CCA_STATUS_ACCEL_DONE (1 << 3) // Bit 3: ACCEL_DONE
#define CCA_STATUS_ACCEL_BUSY (1 << 2) // Bit 2: ACCEL_BUSY
#define CCA_STATUS_ACCEL_ALIVE (1 << 1) // Bit 1: ACCEL_ALIVE (toggles ~100ms @ 200MHz)
#define CCA_STATUS_ALIVE_FLAG (1 << 0) // Bit 0: ACCEL_ALIVE_FLAG (latched)

/////////////////////////////////////////////////////////////////////////////
// Common test patterns for CCA tests
/////////////////////////////////////////////////////////////////////////////
#define DBG_PATTERN_A 0x0123
#define DBG_PATTERN_B 0x4567
#define DBG_PATTERN_C 0x89AB
#define DBG_PATTERN_D 0xCDEF

#define DBG_TST_VAL_0 0x01020304 // Debug reg 0: pattern A + 0x0000
#define DBG_TST_VAL_1 0x05060708 // Debug reg 1: pattern B + 0x0001
#define DBG_TST_VAL_2 0x090A0B0C // Debug reg 2: pattern C + 0x0002
#define DBG_TST_VAL_3 0x0C0D0E0F // Debug reg 3: pattern D + 0x0003
#define DBG_TST_VAL_4 0x10111213 // Debug reg 4: pattern A + 0x0000
#define DBG_TST_VAL_5 0x14151617 // Debug reg 5: pattern B + 0x0001
#define DBG_TST_VAL_6 0x18191A1B // Debug reg 6: pattern C + 0x0002
#define DBG_TST_VAL_7 0x1C1D1E1F // Debug reg 7: pattern D + 0x0003
#define DBG_TST_VAL_8 0x20212223 // Debug reg 8: pattern A + 0x0000
#define DBG_TST_VAL_9 0x24252627 // Debug reg 9: pattern B + 0x0001
#define DBG_TST_VAL_10 0x28292A2B // Debug reg 10: pattern C + 0x0002
#define DBG_TST_VAL_TST 0xABCD1234 // Debug test register: unique test pattern
/////////////////////////////////////////////////////////////////////////////
// Common MSGDMA Descriptor Structure
/////////////////////////////////////////////////////////////////////////////
typedef struct msgdma_descriptor {
    uint32_t src_addr; // 0x00 : Source address (DDR, MSGDMA view) or 0 for AvST source path
    uint32_t dest_addr; // 0x04 : Dest address  (DDR, MSGDMA view) or 0 for AvST sink path
    uint32_t length; // 0x08 : Transfer length in bytes
    uint32_t next_desc_ptr; // 0x0C : Next descriptor (MSGDMA view) or 0 for end of chain
    uint32_t reserved[3]; // 0x10 .. 0x18 : Must be zero
    uint32_t control; // 0x1C : Control flags (see below)
} msgdma_descriptor_t;
/////////////////////////////////////////////////////////////////////////////
// Common Descriptor Control Bits
/////////////////////////////////////////////////////////////////////////////
#define DESC_CTRL_OWNED_BY_HARDWARE_MSK 0x40000000 // HW ownership (mid-chain descriptors)
#define DESC_CTRL_WAIT_FOR_WRITE_MSK 0x02000000 // Wait for write response (last descriptor)
#define DESC_CTRL_EARLY_DONE_MSK 0x01000000 // Early completion (mid-chain descriptors)
#define DESC_CTRL_IRQ_ON_COMPLETION_MSK 0x00004000 // Assert IRQ on completion (last descriptor)
#define DESC_CTRL_EARLY_TERMINATION_MSK 0x00002000 // Early termination enable

// Descriptor control values per chain position:
//   Mid-chain (desc 0..N-2): OWNED_BY_HW | EARLY_DONE                          = 0x41000000
//   Last READ  (desc N-1)  : OWNED_BY_HW | IRQ_ON_COMPLETE                     = 0x40004000
//   Last WRITE (desc N-1)  : OWNED_BY_HW | WAIT_FOR_WRITE | IRQ_ON_COMPLETE    = 0x42004000
#define DESC_CTRL_MID_CHAIN (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_EARLY_DONE_MSK) // 0x41000000
// READ  last: no WAIT_FOR_WRITE — MM2ST has no write response, IRQ fires immediately on chain done
#define DESC_CTRL_LAST_CHAIN_RD (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_IRQ_ON_COMPLETION_MSK) // 0x40004000
// WRITE last: WAIT_FOR_WRITE ensures AXI write response received before IRQ fires
#define DESC_CTRL_LAST_CHAIN_WR \
    (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_WAIT_FOR_WRITE_MSK | DESC_CTRL_IRQ_ON_COMPLETION_MSK) // 0x42004000
// Legacy alias (kept for reference)
#define DESC_CTRL_LAST_CHAIN DESC_CTRL_LAST_CHAIN_WR

// DMA Configuration Constants
#define NUM_OF_DESCRIPTORS 4 // 4 descriptors per chain; block size = DMA_BUF_SIZE / NUM_OF_DESCRIPTORS = 32KB each
/////////////////////////////////////////////////////////////////////////////
// MSGDMA Prefetcher CSR Register Offsets
// Base: MSGDMA_WR_BASE (0x30204040) or MSGDMA_RD_BASE (0x30204060)
/////////////////////////////////////////////////////////////////////////////
#define MSGDMA_PF_CONTROL_OFFSET 0x00 // PF_CONTROL    (RW,   reset=0x00)
#define MSGDMA_PF_NEXT_DESC_LO_OFFSET 0x04 // NEXT_DESC_LO  (RW,   reset=0x00)
#define MSGDMA_PF_NEXT_DESC_HI_OFFSET 0x08 // NEXT_DESC_HI  (RW,   reset=0x00)
// 0x0C: RESERVED
#define MSGDMA_PF_STATUS_OFFSET 0x10 // PF_STATUS     (RW1C, reset=0x00)
#define MSGDMA_PF_DESC_POLL_FREQ_OFFSET 0x14 // DESC_POLL_FREQ (RW,  reset=0x00)
// 0x18-0x1C: RESERVED

/////////////////////////////////////////////////////////////////////////////
// NOTE: All MSGDMA register offsets are defined in cca_hal.h (locked)
// Use those definitions directly - no local redefinition needed
/////////////////////////////////////////////////////////////////////////////
// Descriptor layout (Altera msgDMA prefetcher mode, 32 bytes per descriptor):
#define DESC_OFF_SRC_ADDR 0 // 0x00 — Read Address (MM2ST) / Reserved (ST2MM)
#define DESC_OFF_DEST_ADDR 4 // 0x04 — Reserved (MM2ST)    / Write Address (ST2MM)
#define DESC_OFF_LENGTH 8 // 0x08 — Length
#define DESC_OFF_NEXT_DESC 12 // 0x0C — Next Desc Ptr (MSGDMA view)
#define DESC_OFF_RESERVED_1 16 // 0x10 — Reserved [63:0] lo
#define DESC_OFF_RESERVED_2 20 // 0x14 — Reserved [63:0] hi
#define DESC_OFF_RESERVED_3 24 // 0x18 — Reserved [31:0]
#define DESC_OFF_CONTROL 28 // 0x1C — Control
// Descriptor stride and chain offsets
#define DESC_STRIDE 32 // 32 bytes per descriptor
#define WRITE_DESC_BASE_OFF 256 // WRITE chain starts at OCM + 256
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Prefetcher Control/Status Bit Masks
/////////////////////////////////////////////////////////////////////////////
#define PREFETCHER_IRQ_EN_MSK 0x08 // Bit 3: IRQ_EN
#define PREFETCHER_RESET_MSK 0x04 // Bit 2: RESET
#define PREFETCHER_RUN_MSK 0x01 // Bit 0: RUN (self-clearing)

#define PREFETCHER_INTERRUPT_EN_MSK 0x08 // Alias: IRQ_EN (legacy name)
#define PREFETCHER_INTERRUPT_STATUS_MSK 0x01 // Bit 0: IRQ (transfer complete)
/////////////////////////////////////////////////////////////////////////////
// MSGDMA Dispatcher CSR Register Offsets
// Base: MSGDMA_WR_DISPATCHER_BASE (0x30204000) or MSGDMA_RD_DISPATCHER_BASE (0x30204020)
// NOTE: Dispatcher is auto-managed by prefetcher â€” do not write control registers
/////////////////////////////////////////////////////////////////////////////
#define MSGDMA_DISP_STATUS_OFFSET 0x00 // STATUS      (RO,  reset=0x02)
#define MSGDMA_DISP_CONTROL_OFFSET 0x04 // CONTROL    (RW,  reset=0x00)
#define MSGDMA_DISP_FILL_LEVEL_OFFSET 0x08 // FILL_LEVEL (RO,  reset=0x00)
#define MSGDMA_DISP_RESP_FILL_OFFSET 0x0C // RESP_FILL  (RO,  reset=0x00)
#define MSGDMA_DISP_SEQ_NUM_OFFSET 0x10 // SEQ_NUMBER (RO,  reset=0x00)
/////////////////////////////////////////////////////////////////////////////
// MSGDMA Dispatcher STATUS Register Bit Masks (offset 0x00, reset=0x02)
// NOTE: reset=0x02 (IDLE bit set) is the normal idle state â€” not an error
/////////////////////////////////////////////////////////////////////////////
#define DISPATCHER_BUSY_MSK 0x01 // Bit 0: BUSY
#define DISPATCHER_IDLE_MSK 0x02 // Bit 1: IDLE/STOPPED (reset state)
#define DISPATCHER_RESP_BUF_FULL_MSK 0x04 // Bit 2: RESP_BUF_FULL
#define DISPATCHER_STOPPED_ON_ERR_MSK 0x08 // Bit 3: STOPPED_ON_ERROR
#define DISPATCHER_RESETTING_MSK 0x10 // Bit 4: RESETTING
#define DISPATCHER_STOPPED_MSK 0x20 // Bit 5: STOPPED
#define DISPATCHER_RESET_IN_PROGRESS_MSK 0x40 // Bit 6: RESET_IN_PROGRESS

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Dispatcher CONTROL Register Bit Masks (offset 0x04, reset=0x00)
/////////////////////////////////////////////////////////////////////////////
#define DISPATCHER_STOP_MSK 0x01 // Bit 0: STOP
#define DISPATCHER_RESET_MSK 0x02 // Bit 1: RESET
#define DISPATCHER_STOP_ON_ERR_MSK 0x08 // Bit 3: STOP_ON_ERROR
#define DISPATCHER_STOP_ON_EARLY_TERM 0x10 // Bit 4: STOP_ON_EARLY_TERM

/////////////////////////////////////////////////////////////////////////////
// Register Access Macros - Direct expansion, zero overhead
/////////////////////////////////////////////////////////////////////////////
#ifdef __NIOS_V__
// Nios V: Use HAL IO macros (includes proper barriers)
#include "io.h"
#define cca_reg_write(base, offset, value) IOWR_32DIRECT(base, offset, value)
#define cca_reg_read(base, offset) IORD_32DIRECT(base, offset)
/////////////////////////////////////////////////////////////////////////////
#else
// HPS: Direct volatile pointer access
#define cca_reg_write(base, offset, value)                                   \
    do {                                                                     \
        volatile uint32_t *__reg = (volatile uint32_t *)((base) + (offset)); \
        *__reg = (value);                                                    \
    } while (0)

#define cca_reg_read(base, offset) (*(volatile uint32_t *)((base) + (offset)))
/////////////////////////////////////////////////////////////////////////////
#endif
/////////////////////////////////////////////////////////////////////////////
#endif /* __CCA_HAL_H__ */
