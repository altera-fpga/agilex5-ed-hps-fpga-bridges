/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Kernel-internal definitions for the Altera Cache Coherent Accelerator
 * Copyright (C) 2026 Altera Corporation
 */
#ifndef _CCA_H
#define _CCA_H

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "cca-uapi.h"

/* CCA Accelerator CSR offsets (relative to csr_base) */
#define CCA_FPGA_ID 0x00
#define CCA_FPGA_REV 0x04
#define CCA_ACCEL_CONTROL 0x10
#define CCA_ACCEL_STATUS 0x14
#define CCA_ACCEL_STAT_SNK 0x18
#define CCA_ACCEL_STAT_SRC 0x1C

#define CCA_FPGA_ID_VAL 0xCCA00101UL
#define CCA_FPGA_REV_VAL 0x01010003UL

/* ACCEL_CONTROL bits */
#define CCA_CTRL_RESET BIT(15)
#define CCA_CTRL_ABORT BIT(10)
#define CCA_CTRL_STOP BIT(9)
#define CCA_CTRL_START BIT(8)
#define CCA_CTRL_CMD_MASK 0xFFu

/* ACCEL_STATUS bits */
#define CCA_STATUS_ACCEL_ERROR BIT(4)
#define CCA_STATUS_ACCEL_DONE BIT(3)
#define CCA_STATUS_ACCEL_BUSY BIT(2)
#define CCA_STATUS_ACCEL_ALIVE BIT(1)
#define CCA_STATUS_ALIVE_FLAG BIT(0)

/* mSGDMA Prefetcher CSR offsets (relative to pref_base) */
#define MSGDMA_PF_CONTROL_OFFSET 0x00
#define MSGDMA_PF_NEXT_DESC_LO_OFFSET 0x04
#define MSGDMA_PF_NEXT_DESC_HI_OFFSET 0x08
#define MSGDMA_PF_STATUS_OFFSET 0x10

/* Prefetcher control/status bits */
#define PREFETCHER_RUN_MSK 0x01u
#define PREFETCHER_RESET_MSK 0x04u
#define PREFETCHER_IRQ_EN_MSK 0x08u
#define PREFETCHER_INTERRUPT_STATUS_MSK 0x01u /* W1C */

/* mSGDMA Dispatcher CSR offsets (relative to disp_base) */
#define MSGDMA_DISP_STATUS_OFFSET 0x00
#define MSGDMA_DISP_CONTROL_OFFSET 0x04

/* Dispatcher control/status bits */
#define DISPATCHER_RESET_MSK 0x02u
#define DISPATCHER_IRQ_MSK 0x200u

/* Descriptor control bits */
#define DESC_CTRL_OWNED_BY_HARDWARE_MSK 0x40000000u
#define DESC_CTRL_WAIT_FOR_WRITE_MSK 0x02000000u
#define DESC_CTRL_EARLY_DONE_MSK 0x01000000u
#define DESC_CTRL_IRQ_ON_COMPLETION_MSK 0x00004000u

/*
 * WAIT_FOR_WRITE defers the prefetcher IRQ until AXI BRESP arrives, ensuring
 * all DMA writes reach DDR before the ISR fires.  Do not set on the RD (MM2ST)
 * channel: it issues only reads, so there is no BRESP and the wait stalls.
 */
#define DESC_CTRL_MID_CHAIN (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_EARLY_DONE_MSK)
#define DESC_CTRL_LAST_CHAIN_RD \
    (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_IRQ_ON_COMPLETION_MSK | DESC_CTRL_EARLY_DONE_MSK)
#define DESC_CTRL_LAST_CHAIN_WR \
    (DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_WAIT_FOR_WRITE_MSK | DESC_CTRL_IRQ_ON_COMPLETION_MSK)

/* Descriptor field offsets within each 32-byte slot */
#define DESC_OFF_SRC_ADDR 0x00
#define DESC_OFF_DEST_ADDR 0x04
#define DESC_OFF_LENGTH 0x08
#define DESC_OFF_NEXT_DESC 0x0C
#define DESC_RSVD_1 0x10
#define DESC_RSVD_2 0x14
#define DESC_RSVD_3 0x18
#define DESC_OFF_CONTROL 0x1C

#define DESC_SLOT_SIZE 32u
#define MAX_DESCRIPTORS 64u /* OCM is 4 KB split equally; 64 descriptors x 32 bytes = 2 KB per channel */

/* OCM SW offsets from the mapped desc-ocm base (RD first, WR at +0x800) */
#define OCM_RD_SW_OFFSET 0x0000u
#define OCM_WR_SW_OFFSET 0x0800u

/** struct cca_dma_chan - per-direction DMA channel context */
struct cca_dma_chan {
    void __iomem *pref_base; /* ioremap'd prefetcher CSR              */
    void __iomem *disp_base; /* ioremap'd dispatcher CSR              */
    void __iomem *ocm_base; /* ioremap'd OCM partition (CPU view)    */
    u64 desc_hw_base; /* hardware-view base for PREF_NEXT_DESC */
};

/** struct cca_job - per-job DMA buffer metadata */
struct cca_job {
    dma_addr_t in_dma;
    dma_addr_t out_dma;
    u64 in_offset;
    u64 out_offset;
    u32 length;
};

/** struct cca_priv - per-device driver private data */
struct cca_priv {
    struct device *dev;
    struct miscdevice misc;

    void __iomem *csr_base;
    struct cca_dma_chan wr; /* ST2MM: FPGA to HPS DDR */
    struct cca_dma_chan rd; /* MM2ST: HPS DDR to FPGA */

    void *slab_cpu; /* kernel VA of DMA slab            */
    dma_addr_t slab_dma; /* IOVA of DMA slab (SMMU active)   */
    size_t slab_size;

    struct cca_job *jobs;
    u32 n_jobs;
    u8 xfer_cmd;

    bool xfer_running; /* set by start_chain_hw, cleared by ISR */
    bool xfer_submitted; /* set by SUBMIT, cleared by WAIT/FREE/RESET */
    int xfer_err;

    struct completion xfer_done;
    int wr_irq;

    struct mutex lock;
    wait_queue_head_t xfer_wq;
    atomic_t open_count;
};

/* Register access helpers */

static inline u32 cca_read(struct cca_priv *p, u32 off)
{
    return ioread32(p->csr_base + off);
}

static inline void cca_write(struct cca_priv *p, u32 off, u32 val)
{
    iowrite32(val, p->csr_base + off);
}

static inline u32 pref_read(struct cca_dma_chan *ch, u32 off)
{
    return ioread32(ch->pref_base + off);
}

static inline void pref_write(struct cca_dma_chan *ch, u32 off, u32 val)
{
    iowrite32(val, ch->pref_base + off);
}

static inline u32 disp_read(struct cca_dma_chan *ch, u32 off)
{
    return ioread32(ch->disp_base + off);
}

static inline void disp_write(struct cca_dma_chan *ch, u32 off, u32 val)
{
    iowrite32(val, ch->disp_base + off);
}

static inline void desc_write(struct cca_dma_chan *ch, u32 slot, u32 off, u32 val)
{
    iowrite32(val, ch->ocm_base + slot * DESC_SLOT_SIZE + off);
}

/* Defined in cca-dma-helper.c */
int alloc_slab(struct cca_priv *priv, u32 n_jobs, const u32 *lengths);
void free_slab(struct cca_priv *priv);
void start_chain_hw(struct cca_priv *priv);

/* Defined in cca.c */
void chan_reset(struct cca_dma_chan *ch);
void chan_start(struct cca_dma_chan *ch, bool set_irq_en);

#endif /* _CCA_H */
