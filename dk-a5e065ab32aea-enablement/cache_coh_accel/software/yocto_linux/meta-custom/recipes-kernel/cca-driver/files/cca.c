// SPDX-License-Identifier: GPL-2.0
/*
 * Linux driver for the Altera Cache Coherent Accelerator
 * Copyright (C) 2026 Altera Corporation
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "cca.h"

/** chan_reset() - stop and flush a msgDMA prefetcher and dispatcher */
void chan_reset(struct cca_dma_chan *ch)
{
    unsigned long deadline;

    pref_write(ch, MSGDMA_PF_CONTROL_OFFSET, PREFETCHER_RESET_MSK);
    deadline = jiffies + msecs_to_jiffies(100);
    while ((pref_read(ch, MSGDMA_PF_CONTROL_OFFSET) & PREFETCHER_RESET_MSK) && time_before(jiffies, deadline))
        cpu_relax();

    disp_write(ch, MSGDMA_DISP_CONTROL_OFFSET, DISPATCHER_RESET_MSK);
    deadline = jiffies + msecs_to_jiffies(100);
    while ((disp_read(ch, MSGDMA_DISP_CONTROL_OFFSET) & DISPATCHER_RESET_MSK) && time_before(jiffies, deadline))
        cpu_relax();
}

/** chan_start() - load first descriptor address and assert RUN */
void chan_start(struct cca_dma_chan *ch, bool set_irq_en)
{
    u32 ctrl = PREFETCHER_RUN_MSK;

    /* Order all preceding desc_write() (OCM MMIO) before hardware fetches */
    wmb();

    pref_write(ch, MSGDMA_PF_NEXT_DESC_LO_OFFSET, lower_32_bits(ch->desc_hw_base));
    pref_write(ch, MSGDMA_PF_NEXT_DESC_HI_OFFSET, upper_32_bits(ch->desc_hw_base));

    if (set_irq_en)
        ctrl |= PREFETCHER_IRQ_EN_MSK;
    pref_write(ch, MSGDMA_PF_CONTROL_OFFSET, ctrl);
}

/** cca_wr_isr() - WR prefetcher completion ISR; posts xfer_done */
static irqreturn_t cca_wr_isr(int irq, void *data)
{
    struct cca_priv *priv = data;
    u32 status;

    status = pref_read(&priv->wr, MSGDMA_PF_STATUS_OFFSET);

    // The dispatcher IRQ bit fired without the prefetcher bit being set
    if (!(status & PREFETCHER_INTERRUPT_STATUS_MSK)) {
        disp_write(&priv->wr, MSGDMA_DISP_STATUS_OFFSET, DISPATCHER_IRQ_MSK);
        dev_warn_ratelimited(priv->dev,
                             "WR IRQ fired but PREFETCHER_INTERRUPT_STATUS_MSK=0 "
                             "(status=0x%08x)\n",
                             status);
        return IRQ_HANDLED;
    }

    // Clear both prefetcher (W1C) and dispatcher IRQ bits
    pref_write(&priv->wr, MSGDMA_PF_STATUS_OFFSET, PREFETCHER_INTERRUPT_STATUS_MSK);
    disp_write(&priv->wr, MSGDMA_DISP_STATUS_OFFSET, DISPATCHER_IRQ_MSK);

    if (cca_read(priv, CCA_ACCEL_STATUS) & CCA_STATUS_ACCEL_ERROR)
        WRITE_ONCE(priv->xfer_err, -EIO);
    else
        WRITE_ONCE(priv->xfer_err, 0);

    WRITE_ONCE(priv->xfer_running, false);
    complete(&priv->xfer_done);
    wake_up_interruptible(&priv->xfer_wq);

    dev_dbg(priv->dev, "WR ISR: transfer complete, err=%d\n", READ_ONCE(priv->xfer_err));
    return IRQ_HANDLED;
}

static long cca_ioctl_query_info(struct cca_priv *priv, unsigned long arg)
{
    struct cca_driver_info info = {
        .max_jobs = CCA_MAX_JOBS,
        .transfer_align = CCA_TRANSFER_ALIGN,
        .driver_version = CCA_DRIVER_VERSION,
        ._pad = 0,
    };
    return copy_to_user((void __user *)arg, &info, sizeof(info)) ? -EFAULT : 0;
}

/** cca_ioctl_alloc() - validate jobs, allocate DMA slab, return offsets */
static long cca_ioctl_alloc(struct cca_priv *priv, unsigned long arg)
{
    struct cca_alloc_req req;
    u32 *lengths;
    u32 i;
    int ret;

    if (priv->jobs)
        return -EBUSY;

    // Copy the request struct from the user into the kernel, this will be used to generate descriptors
    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    // Check if the request fits within the maximum job constraint
    if (!req.n_jobs || req.n_jobs > CCA_MAX_JOBS) {
        dev_dbg(priv->dev, "alloc: n_jobs=%u out of range [1, %u]\n", req.n_jobs, CCA_MAX_JOBS);
        return -EINVAL;
    }

    // Check for valid command mode passed
    if (req.cmd != CCA_CMD_PASS_THROUGH && req.cmd != CCA_CMD_PROCESS_DATA) {
        dev_dbg(priv->dev, "alloc: unknown cmd=0x%02x\n", req.cmd);
        return -EINVAL;
    }

    // Allocate space for the array holding the length of each job and fill the array
    lengths = kmalloc_array(req.n_jobs, sizeof(*lengths), GFP_KERNEL);
    if (!lengths)
        return -ENOMEM;

    if (copy_from_user(lengths, u64_to_user_ptr(req.lengths_ptr), req.n_jobs * sizeof(*lengths))) {
        ret = -EFAULT;
        goto out;
    }

    // Check if lengths is valid, non-zero and aligned by 32
    for (i = 0; i < req.n_jobs; i++) {
        if (!lengths[i] || !IS_ALIGNED(lengths[i], CCA_TRANSFER_ALIGN)) {
            dev_dbg(priv->dev, "alloc: lengths[%u]=%u must be non-zero multiple of %u\n", i, lengths[i],
                    CCA_TRANSFER_ALIGN);
            ret = -EINVAL;
            goto out;
        }
    }

    // Call the helper function to allocate space in DDR for the input and output buffer
    ret = alloc_slab(priv, req.n_jobs, lengths);
    if (ret)
        goto out;

    priv->xfer_cmd = (u8)req.cmd;

    // Update the user's request struct to store the offset bindings for each job
    req.total_mmap_size = priv->slab_size;
    for (i = 0; i < priv->n_jobs; i++) {
        req.jobs[i].in_offset = priv->jobs[i].in_offset;
        req.jobs[i].out_offset = priv->jobs[i].out_offset;
        req.jobs[i].length = priv->jobs[i].length;
        req.jobs[i]._pad = 0;
    }

    // Copy the request struct back to userspace
    if (copy_to_user((void __user *)arg, &req, sizeof(req))) {
        free_slab(priv);
        ret = -EFAULT;
    }

out:
    kfree(lengths);
    return ret;
}

/**
 * cca_ioctl_submit() - start hardware on the allocated chain
 *
 * reinit_completion() is called before start_chain_hw() to close the race
 * where the ISR fires and posts xfer_done before cca_ioctl_wait() blocks.
 */
static long cca_ioctl_submit(struct cca_priv *priv)
{
    if (!priv->jobs)
        return -ENOENT;
    if (READ_ONCE(priv->xfer_running))
        return -EBUSY;

    reinit_completion(&priv->xfer_done);
    start_chain_hw(priv);
    priv->xfer_submitted = true;
    return 0;
}

/**
 * cca_ioctl_wait() - sleep until the WR ISR signals completion or timeout
 *
 * Guards on xfer_submitted (set by SUBMIT, cleared here) rather than
 * xfer_running (cleared by the ISR) to avoid a race where the ISR fires
 * between SUBMIT and WAIT, making xfer_running false before we check it.
 */
static long cca_ioctl_wait(struct cca_priv *priv, u32 timeout_ms)
{
    long remaining;
    int err;

    if (!priv->jobs)
        return -ENOENT;
    if (!priv->xfer_submitted)
        return -EINVAL;

    // Wait for `remaining` number of milliseconds before timeout
    mutex_unlock(&priv->lock);
    remaining = wait_for_completion_interruptible_timeout(&priv->xfer_done, msecs_to_jiffies(timeout_ms));
    mutex_lock(&priv->lock);

    // Interrupted by signal
    if (remaining < 0)
        return -ERESTARTSYS;

    // If timeout, reset channels and throw error
    if (remaining == 0) {
        u32 accel = cca_read(priv, CCA_ACCEL_STATUS);
        u32 wpstat = pref_read(&priv->wr, MSGDMA_PF_STATUS_OFFSET);
        u32 wpctrl = pref_read(&priv->wr, MSGDMA_PF_CONTROL_OFFSET);
        u32 rpstat = pref_read(&priv->rd, MSGDMA_PF_STATUS_OFFSET);
        u32 rpctrl = pref_read(&priv->rd, MSGDMA_PF_CONTROL_OFFSET);

        dev_err(priv->dev,
                "transfer timed out after %u ms (%u jobs)\n"
                "  ACCEL_STATUS=0x%08x (alive=%u busy=%u done=%u error=%u)\n"
                "  WR pref: CTRL=0x%08x STATUS=0x%08x\n"
                "  RD pref: CTRL=0x%08x STATUS=0x%08x\n",
                timeout_ms, priv->n_jobs, accel, !!(accel & CCA_STATUS_ALIVE_FLAG), !!(accel & CCA_STATUS_ACCEL_BUSY),
                !!(accel & CCA_STATUS_ACCEL_DONE), !!(accel & CCA_STATUS_ACCEL_ERROR), wpctrl, wpstat, rpctrl, rpstat);

        chan_reset(&priv->rd);
        chan_reset(&priv->wr);
        WRITE_ONCE(priv->xfer_running, false);
        priv->xfer_submitted = false;
        return -ETIMEDOUT;
    }

    err = READ_ONCE(priv->xfer_err);
    priv->xfer_submitted = false;
    return err;
}

/** cca_ioctl_free() - release DMA slab without resetting hardware */
static long cca_ioctl_free(struct cca_priv *priv)
{
    if (READ_ONCE(priv->xfer_running))
        return -EBUSY;

    free_slab(priv);
    priv->xfer_submitted = false;
    WRITE_ONCE(priv->xfer_err, 0);

    dev_dbg(priv->dev, "slab freed (hardware not reset)\n");
    return 0;
}

/** cca_ioctl_reset() - full hardware reset and slab free; use for teardown or error recovery */
static long cca_ioctl_reset(struct cca_priv *priv)
{
    unsigned long deadline;

    if (READ_ONCE(priv->xfer_running)) {
        chan_reset(&priv->rd);
        chan_reset(&priv->wr);
        WRITE_ONCE(priv->xfer_running, false);
    }

    // Free allocated DDR buffers before resetting
    free_slab(priv);
    priv->xfer_submitted = false;
    WRITE_ONCE(priv->xfer_err, 0);

    // Send reset control to hardware
    cca_write(priv, CCA_ACCEL_CONTROL, CCA_CTRL_RESET);

    // Wait for hardware to reset and check if reset was successful (alive flag)
    deadline = jiffies + msecs_to_jiffies(100);
    while (time_before(jiffies, deadline)) {
        if (cca_read(priv, CCA_ACCEL_STATUS) & CCA_STATUS_ALIVE_FLAG)
            break;
        usleep_range(500, 1000);
    }
    if (!(cca_read(priv, CCA_ACCEL_STATUS) & CCA_STATUS_ALIVE_FLAG)) {
        dev_err(priv->dev, "reset: accelerator did not assert ALIVE_FLAG\n");
        return -ETIMEDOUT;
    }

    chan_reset(&priv->rd);
    chan_reset(&priv->wr);

    dev_dbg(priv->dev, "hardware reset complete\n");
    return 0;
}

static long cca_ioctl_get_status(struct cca_priv *priv, unsigned long arg)
{
    struct cca_hw_status st = {
        .accel_status = cca_read(priv, CCA_ACCEL_STATUS),
        .sink_count = cca_read(priv, CCA_ACCEL_STAT_SNK),
        .src_count = cca_read(priv, CCA_ACCEL_STAT_SRC),
        .xfer_running = READ_ONCE(priv->xfer_running) ? 1u : 0u,
        .fpga_id = cca_read(priv, CCA_FPGA_ID),
        .fpga_rev = cca_read(priv, CCA_FPGA_REV),
    };
    return copy_to_user((void __user *)arg, &st, sizeof(st)) ? -EFAULT : 0;
}

static int cca_open(struct inode *inode, struct file *file)
{
    struct cca_priv *priv = container_of(file->private_data, struct cca_priv, misc);

    // Enforce exclusive open, reject second opener if the device is in use already
    if (atomic_cmpxchg(&priv->open_count, 0, 1) != 0)
        return -EBUSY;

    file->private_data = priv;
    dev_dbg(priv->dev, "opened\n");
    return 0;
}

static int cca_release(struct inode *inode, struct file *file)
{
    struct cca_priv *priv = file->private_data;

    mutex_lock(&priv->lock);
    if (READ_ONCE(priv->xfer_running)) {
        dev_warn(priv->dev, "fd closed with transfer active; resetting hardware\n");
        chan_reset(&priv->rd);
        chan_reset(&priv->wr);
        WRITE_ONCE(priv->xfer_running, false);
    }
    free_slab(priv);
    priv->xfer_submitted = false;
    WRITE_ONCE(priv->xfer_err, 0);
    mutex_unlock(&priv->lock);

    atomic_set(&priv->open_count, 0);
    dev_dbg(priv->dev, "closed; device idle\n");
    return 0;
}

/**
 * cca_mmap() - map the DMA slab into userspace (zero-copy path)
 *
 * vm_pgoff must be 0; requested size must equal total_mmap_size from CCA_IOC_ALLOC.
 * munmap() must be called before CCA_IOC_FREE or CCA_IOC_RESET.
 */
static int cca_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct cca_priv *priv = file->private_data;
    size_t req = vma->vm_end - vma->vm_start;
    int ret;

    /* Only map the entire DMA slab, no mapping offsets 
     * In this design, the entire region should be used */
    if (vma->vm_pgoff != 0)
        return -EINVAL;

    if (mutex_lock_interruptible(&priv->lock))
        return -ERESTARTSYS;

    // Check that the requested region has actually been alloc'd
    if (!priv->slab_cpu) {
        ret = -ENOENT;
        goto out;
    }

    // Does user expected size match allocated size
    if (req != priv->slab_size) {
        dev_dbg(priv->dev, "mmap: requested size %zu != slab size %zu\n", req, priv->slab_size);
        ret = -EINVAL;
        goto out;
    }

    ret = dma_mmap_coherent(priv->dev, vma, priv->slab_cpu, priv->slab_dma, priv->slab_size);
    if (ret)
        dev_err(priv->dev, "dma_mmap_coherent failed: %d\n", ret);
out:
    mutex_unlock(&priv->lock);
    return ret;
}

static __poll_t cca_poll(struct file *file, poll_table *pt)
{
    struct cca_priv *priv = file->private_data;
    __poll_t mask = 0;

    poll_wait(file, &priv->xfer_wq, pt);

    if (!READ_ONCE(priv->xfer_running) && priv->jobs)
        mask |= EPOLLIN | EPOLLRDNORM;

    if (!priv->jobs)
        mask |= EPOLLOUT | EPOLLWRNORM;

    return mask;
}

// Registering all ioctl functions
static long cca_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct cca_priv *priv = file->private_data;
    long ret;

    /* GET_STATUS is lock-free, safe to call concurrently with WAIT */
    if (cmd == CCA_IOC_GET_STATUS)
        return cca_ioctl_get_status(priv, arg);

    ret = mutex_lock_interruptible(&priv->lock);
    if (ret)
        return ret;

    switch (cmd) {
    case CCA_IOC_QUERY_INFO:
        ret = cca_ioctl_query_info(priv, arg);
        break;
    case CCA_IOC_ALLOC:
        ret = cca_ioctl_alloc(priv, arg);
        break;
    case CCA_IOC_SUBMIT:
        ret = cca_ioctl_submit(priv);
        break;
    case CCA_IOC_WAIT: {
        u32 timeout_ms;

        if (get_user(timeout_ms, (__u32 __user *)arg)) {
            ret = -EFAULT;
            break;
        }
        ret = cca_ioctl_wait(priv, timeout_ms);
        break;
    }
    case CCA_IOC_RESET:
        ret = cca_ioctl_reset(priv);
        break;
    case CCA_IOC_FREE:
        ret = cca_ioctl_free(priv);
        break;
    default:
        ret = -ENOTTY;
        break;
    }

    mutex_unlock(&priv->lock);
    return ret;
}

static const struct file_operations cca_fops = {
    .owner = THIS_MODULE,
    .open = cca_open,
    .release = cca_release,
    .unlocked_ioctl = cca_ioctl,
    .mmap = cca_mmap,
    .poll = cca_poll,
    .llseek = noop_llseek,
};

/** cca_probe() - map hardware regions, initialise accelerator and DMA, register misc device */
static int cca_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct cca_priv *priv;
    struct resource *res;
    void __iomem *ocm_base;
    u64 wr_hw_addr, rd_hw_addr;
    u32 id;
    unsigned long deadline;
    int ret;

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->dev = dev;
    mutex_init(&priv->lock);
    init_waitqueue_head(&priv->xfer_wq);
    init_completion(&priv->xfer_done);
    atomic_set(&priv->open_count, 0);

    // Map all the MMIO regions based on the device tree (CSR, DMA prefetchers and dispatchers, OCM region)
    priv->csr_base = devm_platform_ioremap_resource_byname(pdev, "cca-csr");
    if (IS_ERR(priv->csr_base))
        return dev_err_probe(dev, PTR_ERR(priv->csr_base), "failed to map 'cca-csr'\n");

    priv->wr.pref_base = devm_platform_ioremap_resource_byname(pdev, "msgdma-wr-pref");
    if (IS_ERR(priv->wr.pref_base))
        return dev_err_probe(dev, PTR_ERR(priv->wr.pref_base), "failed to map 'msgdma-wr-pref'\n");

    priv->rd.pref_base = devm_platform_ioremap_resource_byname(pdev, "msgdma-rd-pref");
    if (IS_ERR(priv->rd.pref_base))
        return dev_err_probe(dev, PTR_ERR(priv->rd.pref_base), "failed to map 'msgdma-rd-pref'\n");

    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "desc-ocm");
    if (!res)
        return dev_err_probe(dev, -ENODEV, "missing 'desc-ocm' in device tree\n");

    ocm_base = devm_ioremap_resource(dev, res);
    if (IS_ERR(ocm_base))
        return dev_err_probe(dev, PTR_ERR(ocm_base), "failed to map 'desc-ocm'\n");

    priv->rd.ocm_base = ocm_base + OCM_RD_SW_OFFSET;
    priv->wr.ocm_base = ocm_base + OCM_WR_SW_OFFSET;

    priv->wr.disp_base = devm_platform_ioremap_resource_byname(pdev, "msgdma-wr-disp");
    if (IS_ERR(priv->wr.disp_base))
        return dev_err_probe(dev, PTR_ERR(priv->wr.disp_base), "failed to map 'msgdma-wr-disp'\n");

    priv->rd.disp_base = devm_platform_ioremap_resource_byname(pdev, "msgdma-rd-disp");
    if (IS_ERR(priv->rd.disp_base))
        return dev_err_probe(dev, PTR_ERR(priv->rd.disp_base), "failed to map 'msgdma-rd-disp'\n");

    ret = device_property_read_u64(dev, "altr,wr-desc-hw-addr", &wr_hw_addr);
    if (ret)
        return dev_err_probe(dev, ret, "missing 'altr,wr-desc-hw-addr' in DT\n");
    priv->wr.desc_hw_base = wr_hw_addr;

    ret = device_property_read_u64(dev, "altr,rd-desc-hw-addr", &rd_hw_addr);
    if (ret)
        return dev_err_probe(dev, ret, "missing 'altr,rd-desc-hw-addr' in DT\n");
    priv->rd.desc_hw_base = rd_hw_addr;

    /* 32-bit mask: mSGDMA descriptor address fields are 32-bit */
    ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
    if (ret)
        return dev_err_probe(dev, ret, "failed to set 32-bit DMA mask\n");

    id = cca_read(priv, CCA_FPGA_ID);

    if (id != CCA_FPGA_ID_VAL)
        dev_warn(dev, "FPGA_ID=0x%08x expected 0x%08lx; continuing\n", id, CCA_FPGA_ID_VAL);
    else
        dev_dbg(dev, "FPGA_ID verified (0x%08x)\n", id);

    // As part of initialization, reset all hardware
    cca_write(priv, CCA_ACCEL_CONTROL, CCA_CTRL_RESET);
    deadline = jiffies + msecs_to_jiffies(100);
    while (time_before(jiffies, deadline)) {
        if (cca_read(priv, CCA_ACCEL_STATUS) & CCA_STATUS_ALIVE_FLAG)
            break;
        usleep_range(500, 1000);
    }
    if (!(cca_read(priv, CCA_ACCEL_STATUS) & CCA_STATUS_ALIVE_FLAG))
        return dev_err_probe(dev, -ETIMEDOUT, "accelerator did not assert ALIVE_FLAG after reset\n");

    chan_reset(&priv->rd);
    chan_reset(&priv->wr);

    priv->wr_irq = platform_get_irq(pdev, 0);
    if (priv->wr_irq < 0)
        return dev_err_probe(dev, priv->wr_irq, "failed to get WR IRQ from DTS\n");

    ret = devm_request_irq(dev, priv->wr_irq, cca_wr_isr, 0, "cca-wr", priv);
    if (ret)
        return dev_err_probe(dev, ret, "failed to request WR IRQ %d\n", priv->wr_irq);

    priv->misc.minor = MISC_DYNAMIC_MINOR;
    priv->misc.name = "cca0";
    priv->misc.fops = &cca_fops;
    priv->misc.parent = dev;

    ret = misc_register(&priv->misc);
    if (ret)
        return dev_err_probe(dev, ret, "failed to register misc device\n");

    platform_set_drvdata(pdev, priv);

    dev_info(dev,
             "CCA ready at /dev/%s  "
             "(max %u jobs, %u-byte align, "
             "RD_hw=0x%08llx WR_hw=0x%08llx, IRQ=%d)\n",
             priv->misc.name, CCA_MAX_JOBS, CCA_TRANSFER_ALIGN, priv->rd.desc_hw_base, priv->wr.desc_hw_base,
             priv->wr_irq);
    return 0;
}

static void cca_remove(struct platform_device *pdev)
{
    struct cca_priv *priv = platform_get_drvdata(pdev);

    misc_deregister(&priv->misc);

    mutex_lock(&priv->lock);
    if (READ_ONCE(priv->xfer_running)) {
        chan_reset(&priv->rd);
        chan_reset(&priv->wr);
    }
    free_slab(priv);
    mutex_unlock(&priv->lock);

    dev_info(priv->dev, "CCA removed\n");
}

static const struct of_device_id cca_of_match[] = { { .compatible = "altr,cache-coh-accel-1.0" }, { /* sentinel */ } };
MODULE_DEVICE_TABLE(of, cca_of_match);

static struct platform_driver cca_driver = {
	.probe  = cca_probe,
	.remove = cca_remove,
	.driver = {
		.name           = "altera-cca",
		.of_match_table = cca_of_match,
	},
};
module_platform_driver(cca_driver);

MODULE_AUTHOR("Altera Corporation");
MODULE_DESCRIPTION("Altera Cache Coherent Accelerator Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
MODULE_ALIAS("platform:altera-cca");
