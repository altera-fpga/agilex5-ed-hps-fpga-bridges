/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator Register Mappings
 */

#ifndef CCA_H
#define CCA_H

#include <stdint.h>
#include "socfpga_interrupt.h"

/////////////////////////////////////////////////////////////////////////////
// HPS Bridge Base Addresses
/////////////////////////////////////////////////////////////////////////////
#define LWH2F_BASE_ADDR 0x20000000 // HPS LWHPS2FPGA bridge base
/////////////////////////////////////////////////////////////////////////////
// CCA Subsystem Base (HPS view via LWH2F)
/////////////////////////////////////////////////////////////////////////////
#define CCA_SS_BASE LWH2F_BASE_ADDR

/////////////////////////////////////////////////////////////////////////////
// mSGDMA CSR Base Addresses
/////////////////////////////////////////////////////////////////////////////
#define MSGDMA_WR_BASE (CCA_SS_BASE + 0x204000) // 0x20204000 (Prefetcher Write CSR)
#define MSGDMA_RD_BASE (CCA_SS_BASE + 0x204020) // 0x20204020 (Prefetcher Read CSR)

#define MSGDMA_WR_DISPATCHER_BASE (CCA_SS_BASE + 0x204040) // 0x20204040 (Dispatcher Write CSR)
#define MSGDMA_RD_DISPATCHER_BASE (CCA_SS_BASE + 0x204060) // 0x20204060 (Dispatcher Read CSR)

/////////////////////////////////////////////////////////////////////////////
// CCA Accelerator CSR Base (HPS view)
/////////////////////////////////////////////////////////////////////////////
#define CCA_BASE (CCA_SS_BASE + 0x400000) // 0x20400000

/////////////////////////////////////////////////////////////////////////////
// Descriptor OCM
/////////////////////////////////////////////////////////////////////////////
#define OCM_BASE (CCA_SS_BASE + 0x200000) // 0x20200000 (HPS view)
#define OCM_BASE_MSGDMA 0x20200000 // MSGDMA master view (same physical)

/////////////////////////////////////////////////////////////////////////////
// DMA Descriptor Layout
/////////////////////////////////////////////////////////////////////////////
#define WRITE_DESC_BASE_OFF 2048 // WRITE descriptor chain starts at OCM_BASE + 2048 (64 × 32B READ descs)

/////////////////////////////////////////////////////////////////////////////
// IRQ Numbers (F2H interrupts mapped at HPS GIC)
/////////////////////////////////////////////////////////////////////////////
/* F2H interrupt lines as seen by the GIC (socfpga_hpu_interrupt_t enum).
 *
 * Agilex 5 has two F2H interrupt banks in Qsys:
 *   f2h_irq0_in[31:0]  -> FPGA2HPS_INTERRUPT0  .. FPGA2HPS_INTERRUPT31
 *   f2h_irq1_in[30:0]  -> FPGA2HPS_INTERRUPT32 .. FPGA2HPS_INTERRUPT62
 *
 * In the GHRD Qsys design the MSGDMA interrupts are wired as:
 *   MSGDMA WRITE IRQ -> f2h_irq0_in[0] -> FPGA2HPS_INTERRUPT0
 *   MSGDMA READ  IRQ -> f2h_irq1_in[1] -> FPGA2HPS_INTERRUPT33
 */
#define CCA_IRQ_MSGDMA_WRITE FPGA2HPS_INTERRUPT0 // f2h_irq0_in[0]  (irqNumber=0 in Qsys)
#define CCA_IRQ_MSGDMA_READ FPGA2HPS_INTERRUPT33 // f2h_irq1_in[1]  (irqNumber=1 in Qsys)
#define GIC_INTERRUPT_PRIORITY_F2H 14 // GIC priority for F2H interrupts (lower is higher priority)

/////////////////////////////////////////////////////////////////////////////
// CCA CSR Register Offsets (relative to CCA_BASE)
/////////////////////////////////////////////////////////////////////////////
#define CCA_FPGA_ID 0x00000000
#define CCA_FPGA_REV 0x00000004

#define CCA_ACCEL_CONTROL 0x00000010
#define CCA_ACCEL_STATUS 0x00000014

/////////////////////////////////////////////////////////////////////////////
// CCA Hardware Register Expected Values
/////////////////////////////////////////////////////////////////////////////
#define CCA_FPGA_ID_VAL 0xCCA00101
#define CCA_FPGA_REV_VAL 0x01010003

/////////////////////////////////////////////////////////////////////////////
// CCA_ACCEL_CONTROL Bit Definitions (offset 0x0010)
/////////////////////////////////////////////////////////////////////////////
#define CCA_CTRL_RESET (1 << 15)
#define CCA_CTRL_START (1 << 8)
#define CCA_CTRL_CMD_MASK 0x000000FF

/////////////////////////////////////////////////////////////////////////////
// CCA_ACCEL_STATUS Bit Definitions (offset 0x0014)
/////////////////////////////////////////////////////////////////////////////
#define CCA_STATUS_ACCEL_ERROR (1 << 4)
#define CCA_STATUS_ALIVE_FLAG (1 << 0)

/////////////////////////////////////////////////////////////////////////////
// mSGDMA Descriptor Structure
/////////////////////////////////////////////////////////////////////////////
typedef struct msgdma_descriptor {
    uint32_t src_addr; // Source address (mSGDMA view)
    uint32_t dest_addr; // Dest address   (mSGDMA view)
    uint32_t length; // Transfer length in bytes
    uint32_t next_desc_ptr; // Next descriptor (mSGDMA view), 0 = end of chain
    uint32_t reserved[3]; // Must be zero
    uint32_t control; // Control flags
} msgdma_descriptor_t;

/////////////////////////////////////////////////////////////////////////////
// Descriptor Control Bits
/////////////////////////////////////////////////////////////////////////////
#define DESC_CTRL_OWNED_BY_HARDWARE_MSK 0x40000000
#define DESC_CTRL_WAIT_FOR_WRITE_MSK 0x02000000
#define DESC_CTRL_EARLY_DONE_MSK 0x01000000
#define DESC_CTRL_IRQ_ON_COMPLETION_MSK 0x00004000

#define DESC_CTRL_MID_CHAIN (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_EARLY_DONE_MSK)
// READ  last: no WAIT_FOR_WRITE, IRQ fires immediately on chain done
#define DESC_CTRL_LAST_CHAIN_RD (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_IRQ_ON_COMPLETION_MSK)
// WRITE last: WAIT_FOR_WRITE ensures AXI write response received before IRQ fires
#define DESC_CTRL_LAST_CHAIN_WR \
    (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_WAIT_FOR_WRITE_MSK | DESC_CTRL_IRQ_ON_COMPLETION_MSK)

#define NUM_OF_DESCRIPTORS 64 // max descriptors per channel: WRITE_DESC_BASE_OFF / sizeof(msgdma_descriptor_t)

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Prefetcher CSR Register Offsets (relative to MSGDMA_WR/RD_BASE)
/////////////////////////////////////////////////////////////////////////////
#define MSGDMA_PF_CONTROL_OFFSET 0x00
#define MSGDMA_PF_NEXT_DESC_LO_OFFSET 0x04
#define MSGDMA_PF_NEXT_DESC_HI_OFFSET 0x08
#define MSGDMA_PF_STATUS_OFFSET 0x10

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Prefetcher Control/Status Bit Masks
/////////////////////////////////////////////////////////////////////////////
#define PREFETCHER_IRQ_EN_MSK 0x08
#define PREFETCHER_RESET_MSK 0x04
#define PREFETCHER_RUN_MSK 0x01
#define PREFETCHER_INTERRUPT_STATUS_MSK 0x01

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Dispatcher CSR Register Offsets (relative to MSGDMA_WR/RD_DISPATCHER_BASE)
/////////////////////////////////////////////////////////////////////////////
#define MSGDMA_DISP_STATUS_OFFSET 0x00
#define MSGDMA_DISP_CONTROL_OFFSET 0x04

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Dispatcher STATUS Bit Masks
/////////////////////////////////////////////////////////////////////////////
#define DISPATCHER_IRQ_MSK 0x200 // IRQ status bit (W1C)

/////////////////////////////////////////////////////////////////////////////
// MSGDMA Dispatcher CONTROL Bit Masks
/////////////////////////////////////////////////////////////////////////////
#define DISPATCHER_RESET_MSK 0x02

/////////////////////////////////////////////////////////////////////////////
// Register Access Macros
/////////////////////////////////////////////////////////////////////////////
#define cca_reg_write(base, offset, value)                                              \
    do {                                                                                \
        volatile uint32_t *__reg = (volatile uint32_t *)((uintptr_t)(base) + (offset)); \
        *__reg = (value);                                                               \
    } while (0)
#define cca_reg_read(base, offset) (*(volatile uint32_t *)((uintptr_t)(base) + (offset)))

#endif /* CCA_H */
