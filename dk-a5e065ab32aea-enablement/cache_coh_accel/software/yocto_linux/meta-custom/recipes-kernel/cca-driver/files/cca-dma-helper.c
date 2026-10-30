// SPDX-License-Identifier: GPL-2.0
/*
 * DMA slab management and descriptor chain programming
 * Copyright (C) 2026 Altera Corporation
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "cca.h"

/** build_rd_descriptors() - write MM2ST (the HPS to FPGA mSGDMA) chain to OCM */
static void build_rd_descriptors(struct cca_priv *priv)
{
    struct cca_dma_chan *ch = &priv->rd;
    u32 i;

    for (i = 0; i < priv->n_jobs; i++) {
        bool is_last = (i == priv->n_jobs - 1);
        u32 next_hw = (u32)(ch->desc_hw_base + (u64)(i + 1) * DESC_SLOT_SIZE);
        u32 ctrl = is_last ? DESC_CTRL_LAST_CHAIN_RD : DESC_CTRL_MID_CHAIN;

        desc_write(ch, i, DESC_OFF_SRC_ADDR, (u32)priv->jobs[i].in_dma);
        desc_write(ch, i, DESC_OFF_DEST_ADDR, 0);
        desc_write(ch, i, DESC_OFF_LENGTH, priv->jobs[i].length);
        desc_write(ch, i, DESC_OFF_NEXT_DESC, is_last ? 0 : next_hw);
        desc_write(ch, i, DESC_RSVD_1, 0);
        desc_write(ch, i, DESC_RSVD_2, 0);
        desc_write(ch, i, DESC_RSVD_3, 0);
        desc_write(ch, i, DESC_OFF_CONTROL, ctrl);
    }
}

/** build_wr_descriptors() - write ST2MM (the FPGA to HPS mSGDMA) chain to OCM */
static void build_wr_descriptors(struct cca_priv *priv)
{
    struct cca_dma_chan *ch = &priv->wr;
    u32 i;

    for (i = 0; i < priv->n_jobs; i++) {
        bool is_last = (i == priv->n_jobs - 1);
        u32 next_hw = (u32)(ch->desc_hw_base + (u64)(i + 1) * DESC_SLOT_SIZE);
        u32 ctrl = is_last ? DESC_CTRL_LAST_CHAIN_WR : DESC_CTRL_MID_CHAIN;

        desc_write(ch, i, DESC_OFF_SRC_ADDR, 0);
        desc_write(ch, i, DESC_OFF_DEST_ADDR, (u32)priv->jobs[i].out_dma);
        desc_write(ch, i, DESC_OFF_LENGTH, priv->jobs[i].length);
        desc_write(ch, i, DESC_OFF_NEXT_DESC, is_last ? 0 : next_hw);
        desc_write(ch, i, DESC_RSVD_1, 0);
        desc_write(ch, i, DESC_RSVD_2, 0);
        desc_write(ch, i, DESC_RSVD_3, 0);
        desc_write(ch, i, DESC_OFF_CONTROL, ctrl);
    }
}

/** free_slab() - release DMA slab and per-job metadata; must hold priv->lock */
void free_slab(struct cca_priv *priv)
{
    // Free the allocated DDR blocks for the input/output data
    if (priv->slab_cpu)
        dma_free_coherent(priv->dev, priv->slab_size, priv->slab_cpu, priv->slab_dma);
    kfree(priv->jobs);

    priv->slab_cpu = NULL;
    priv->slab_dma = 0;
    priv->slab_size = 0;
    priv->jobs = NULL;
    priv->n_jobs = 0;
}

/**
 * alloc_slab() - allocate a single DMA-coherent slab covering all jobs
 *
 * With 'dma-coherent' and 'iommus' set in the DTS, the kernel allocates
 * normal WB-cached pages and programs the SMMU IOVA to PA mapping for the
 * stream ID given in the DT.  slab_dma is the IOVA written into the 32 bit mSGDMA descriptor
 * fields, DMA_BIT_MASK(32) in probe() ensures it fits.
 *
 * Slab layout: [ in[0] | out[0] | in[1] | out[1] | ... ]
 */
int alloc_slab(struct cca_priv *priv, u32 n_jobs, const u32 *lengths)
{
    size_t total = 0;
    u64 offset = 0;
    u32 i;

    for (i = 0; i < n_jobs; i++)
        total += 2 * (size_t)lengths[i];

    total = PAGE_ALIGN(total);

    // Allocate memory to hold the job pointers
    priv->jobs = kcalloc(n_jobs, sizeof(*priv->jobs), GFP_KERNEL);
    if (!priv->jobs)
        return -ENOMEM;

    // Allocate space within the DDR for the input and output buffers
    priv->slab_cpu = dma_alloc_coherent(priv->dev, total, &priv->slab_dma, GFP_KERNEL);
    if (!priv->slab_cpu) {
        dev_err(priv->dev, "failed to allocate %zu-byte DMA slab for %u jobs\n", total, n_jobs);
        kfree(priv->jobs);
        priv->jobs = NULL;
        return -ENOMEM;
    }
    priv->slab_size = total;

    // Generate the offset bindings for the jobs based on the length of each job
    for (i = 0; i < n_jobs; i++) {
        priv->jobs[i].in_offset = offset;
        priv->jobs[i].in_dma = priv->slab_dma + offset;
        offset += lengths[i];

        priv->jobs[i].out_offset = offset;
        priv->jobs[i].out_dma = priv->slab_dma + offset;
        offset += lengths[i];

        priv->jobs[i].length = lengths[i];
    }
    priv->n_jobs = n_jobs;
    return 0;
}

/** start_chain_hw() - write OCM descriptors and start both DMA channels */
void start_chain_hw(struct cca_priv *priv)
{
    WRITE_ONCE(priv->xfer_running, true);
    WRITE_ONCE(priv->xfer_err, 0);

    chan_reset(&priv->rd);
    chan_reset(&priv->wr);

    build_rd_descriptors(priv);
    build_wr_descriptors(priv);

    cca_write(priv, CCA_ACCEL_CONTROL, CCA_CTRL_START | (u32)(priv->xfer_cmd & CCA_CTRL_CMD_MASK));

    // Clear stale WR IRQ flags (W1C) before asserting RUN + IRQ_EN.
    pref_write(&priv->wr, MSGDMA_PF_STATUS_OFFSET, PREFETCHER_INTERRUPT_STATUS_MSK);
    disp_write(&priv->wr, MSGDMA_DISP_STATUS_OFFSET, DISPATCHER_IRQ_MSK);

    chan_start(&priv->rd, false);
    chan_start(&priv->wr, true);
}
