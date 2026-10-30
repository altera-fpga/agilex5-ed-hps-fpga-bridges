/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for CC_ACCEL tasks
 */

#ifndef __CCA_TASKS_H__
#define __CCA_TASKS_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
// Nios V C code
#include "intel_niosv.h" // alt_niosv_mtime_get()

#define PRINT(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) printf("ERROR: " fmt "\n", ##__VA_ARGS__)

#define NUM_BLOCKS 40
#define OCM_SIZE 4096 // 4 KB -- intel_onchip_memory_0 (cache_coh_accel_subsys.qsys)

#define TEN_SECONDS 10

// Nios V C code (mtime runs at ALT_CPU_CPU_FREQ = 50 MHz)
// Note: CPU internal clock is 50 MHz; external bus to msgDMA/RAM runs at 25 MHz (clock divider)
#define READ_TIMER(val) ((val) = alt_niosv_mtime_get())
#define READ_TIMER_FRQ(val) ((val) = ALT_CPU_CPU_FREQ)

// ISR handlers (defined in cca_lwh2f_task.c, registered with GIC)
void cca_rd_prefetch_isr(void *context);
void cca_wr_prefetch_isr(void *context);

// Task entry points
void lwh2f_task(uint32_t transaction_seconds);
void f2h_task(uint32_t transaction_seconds);

#endif /* __CCA_TASKS_H__ */
