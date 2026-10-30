/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Cache Coherent Accelerator API Header
 * Function prototypes for CCA and DMA subsystem API
 */
#ifndef __CCA_API_H__
#define __CCA_API_H__

#include "alt_types.h"

/**
 * accel_init - Reset and start the CCA accelerator core
 *
 * Issues reset, polls for status, then starts the accelerator.
 * Returns: 0 on success, 1 on failure (timeout)
 */
int accel_init(void);

/**
 * config_dma_transfer - Configure MSGDMA for transfer from DMA_READ_BUF to DMA_WRITE_BUF
 *
 * initializes u_msgDMA_read and u_msgDMA_write prefetchers and dispatchers
 * configures u_msgDMA_read and u_msgDMA_write prefetchers 
 * configures u_msgDMA_read and u_msgDMA_write dispatchers 
 * sets up the u_msgDMA_read and u_msgDMA_write descriptors in OCM memory 
 *
 * Returns: 0 on success, 1 on failure
 */
int config_dma_transfer(void);

/**
 * check_dma_status - Check MSGDMA transfer status
 *
 * Polls MSGDMA prefetcher status registers to verify transfer completion.
 *
 * Returns: 0 if transfer complete, 1 if in progress or error
 */
int check_dma_status(void);

/**
 * verify_dma_transfer - Verify DMA transfer from READ_BUF to WRITE_BUF
 *
 * Verifies that data was transferred correctly from READ_BUF to WRITE_BUF.
 * Checks descriptor status, response FIFOs, and byte counts.
 *
 * Returns: 1 if verification passed, 0 if failed
 */
int verify_dma_transfer(void);

#endif /* __CCA_API_H__ */
