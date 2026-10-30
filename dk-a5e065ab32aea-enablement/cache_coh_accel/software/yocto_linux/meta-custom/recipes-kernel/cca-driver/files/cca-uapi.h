/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Altera Cache Coherent Accelerator userspace API
 */
#ifndef CCA_UAPI_H
#define CCA_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CCA_IOC_MAGIC 'C'

#define CCA_MAX_JOBS 64u
#define CCA_TRANSFER_ALIGN 32u /* CCA data bus width in bytes (256-bit) */
#define CCA_DRIVER_VERSION 0x04000000u

#define CCA_CMD_PROCESS_DATA 0x01u /* output == ~input (8-stage bit inversion) */
#define CCA_CMD_PASS_THROUGH 0x02u /* output ==  input (bypass pipeline) */

/** struct cca_driver_info - returned by CCA_IOC_QUERY_INFO */
struct cca_driver_info {
    __u32 max_jobs;
    __u32 transfer_align;
    __u32 driver_version;
    __u32 _pad;
};

/** struct cca_buf_job - per-job buffer offsets within the mmap'd slab */
struct cca_buf_job {
    __u64 in_offset; /* byte offset of input  buffer from slab start */
    __u64 out_offset; /* byte offset of output buffer from slab start */
    __u32 length; /* transfer length in bytes */
    __u32 _pad;
};

/**
 * struct cca_alloc_req - argument for CCA_IOC_ALLOC
 *
 * in:  n_jobs, cmd, lengths_ptr (userspace pointer to uint32_t[n_jobs])
 * out: total_mmap_size, jobs[] (per-job in/out offsets and length)
 */
struct cca_alloc_req {
    /* in */
    __u32 n_jobs;
    __u32 cmd;
    __u64 lengths_ptr;
    /* out */
    __u64 total_mmap_size;
    struct cca_buf_job jobs[64];
};

/** struct cca_hw_status - returned by CCA_IOC_GET_STATUS */
struct cca_hw_status {
    __u32 accel_status; /* raw CCA_ACCEL_STATUS register */
    __u32 sink_count; /* 256-bit words received by accelerator sink */
    __u32 src_count; /* 256-bit words emitted by accelerator source */
    __u32 xfer_running; /* 1 if DMA transfer is in flight */
    __u32 fpga_id;
    __u32 fpga_rev;
    __u32 _pad[2];
};

/** Query driver/hardware capabilities */
#define CCA_IOC_QUERY_INFO _IOR(CCA_IOC_MAGIC, 1, struct cca_driver_info)

/** Allocate DMA slab; returns total_mmap_size and per-job offsets */
#define CCA_IOC_ALLOC _IOWR(CCA_IOC_MAGIC, 2, struct cca_alloc_req)

/** Start hardware on the allocated chain (write inputs first) */
#define CCA_IOC_SUBMIT _IO(CCA_IOC_MAGIC, 3)

/** Block until transfer completes; arg = pointer to uint32_t timeout_ms */
#define CCA_IOC_WAIT _IOW(CCA_IOC_MAGIC, 4, __u32)

/** Full hardware reset and slab free; safe from any state */
#define CCA_IOC_RESET _IO(CCA_IOC_MAGIC, 5)

/** Read raw hardware status registers (non-blocking, lock-free) */
#define CCA_IOC_GET_STATUS _IOR(CCA_IOC_MAGIC, 6, struct cca_hw_status)

/** Free the DMA slab without resetting hardware; transfer must not be running */
#define CCA_IOC_FREE _IO(CCA_IOC_MAGIC, 7)

#endif /* CCA_UAPI_H */
