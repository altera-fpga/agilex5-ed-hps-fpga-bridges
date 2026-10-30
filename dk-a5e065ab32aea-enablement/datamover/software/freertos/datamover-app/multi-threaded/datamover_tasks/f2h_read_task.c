/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * F2H Read Task for SoC FPGA Data Mover Design
 */

#include <stdlib.h>
#include <stdint.h>
#include "osal.h"
#include "osal_log.h"
#include "event_groups.h"
#include "datamover.h"
#include "datamover_tasks.h"
#include "fabric_dmac.h"

#define ELEMENT_SIZE sizeof(uint64_t)
#define BLOCK_SIZE (OCM_SIZE / ELEMENT_SIZE)

// DMA transaction done semaphore
osal_semaphore_def_t f2h_read_done_sem_mem;
osal_semaphore_t f2h_read_done_sem;

void f2h_read_task(EventGroupHandle_t execution_barrier, uint32_t thread_bit, uint32_t all_threads,
                   uint32_t transaction_seconds)
{
    //////////////////////////
    /* Prepare data buffers */
    //////////////////////////

    volatile uint64_t *sdram_buf = aligned_alloc(4096, NUM_BLOCKS * BLOCK_SIZE * ELEMENT_SIZE);
    if (sdram_buf == NULL) {
        ERROR("Allocating SDRAM buffer failed");
        vTaskDelay(pdMS_TO_TICKS(5));
        return;
    }

    for (size_t i = 0; i < NUM_BLOCKS * BLOCK_SIZE; i++) {
        sdram_buf[i] = (uint64_t)(rand()) << 32 | (uint64_t)(rand());
    }

    // Ensure all memory operations completed before proceeding
    __asm__ volatile("dsb sy" ::: "memory");

    ////////////////////////////////////
    /* Prepare MSGDMA descriptor list */
    ////////////////////////////////////

    volatile msgdma_descriptor_t *read_descriptors =
        (msgdma_descriptor_t *)(uintptr_t)(F2H_DMAC_READ_DESCRIPTOR_OCM_ADDR);

    for (size_t i = 0; i < NUM_BLOCKS + 1; i++) {
        read_descriptors[i].src_addr = (uint32_t)(uintptr_t)(&sdram_buf[i * BLOCK_SIZE]);
        read_descriptors[i].dest_addr = (uint32_t)(uintptr_t)(OCM_C_ADDR);
        read_descriptors[i].length = BLOCK_SIZE * ELEMENT_SIZE;
        read_descriptors[i].next_desc_ptr = (uint32_t)(uintptr_t)(&read_descriptors[i + 1]);
        read_descriptors[i].control = DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_EARLY_DONE_ENABLE_MSK;
        if (i == NUM_BLOCKS - 1) {
            read_descriptors[i].control = DESC_CTRL_OWNED_BY_HARDWARE_MSK | DESC_CTRL_IRQ_ENABLE_MSK |
                                          DESC_CTRL_WAIT_FOR_WRITE_RESP_MSK;
        }
        if (i == NUM_BLOCKS) {
            read_descriptors[i].src_addr = 0;
            read_descriptors[i].dest_addr = 0;
            read_descriptors[i].length = 0;
            read_descriptors[i].control = 0;
            read_descriptors[i].next_desc_ptr = 0;
        }
    }

    ///////////////////////////
    /* Set up done semaphore */
    ///////////////////////////

    f2h_read_done_sem = osal_semaphore_create(&f2h_read_done_sem_mem);

    ////////////////////////
    /* Configure F2H DMAC */
    ////////////////////////

    uint32_t config_ret = configure_f2h_fdmac(READ_OPERATION);
    if (config_ret != 0) {
        ERROR("Failed to configure F2H DMAC for read operation");
        vTaskDelay(pdMS_TO_TICKS(5));
        goto cleanup;
    }

    ////////////////////////////////
    /* Set up F2H DMAC interrupts */
    ////////////////////////////////

    uint32_t int_ret = setup_f2h_fdmac_interrupt(READ_OPERATION, f2h_read_done_sem);
    if (int_ret != 0) {
        ERROR("Failed to setup F2H DMAC read interrupt");
        vTaskDelay(pdMS_TO_TICKS(5));
        goto cleanup;
    }

    ///////////////////////////////
    /* Start F2H read transfers */
    ///////////////////////////////

    // Timer-related variables
    uint64_t counter_freq;
    READ_TIMER_FRQ(counter_freq);
    uint64_t transaction_cycles = transaction_seconds * counter_freq;

    uint64_t transfer_start;
    uint64_t transfer_end;

    uint64_t num_iterations = 0;

    // Function return value variables
    uint32_t start_ret;
    uint32_t sem_ret;

    // Bandwidth calculation variables
    double elapsed_time;
    uint64_t total_data_transferred;

    PRINT("Starting F2H read transfers for %u seconds ...", transaction_seconds);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Wait for all threads to be ready before starting transactions
    xEventGroupSync(execution_barrier, thread_bit, all_threads, portMAX_DELAY);

    // Record initial timer value
    READ_TIMER(transfer_start);
    READ_TIMER(transfer_end);

    // Perform back-to-back data transfers in a loop until the specified transaction duration has elapsed
    while ((transfer_end - transfer_start) < transaction_cycles) {
        start_ret = start_f2h_fdmac(READ_OPERATION);
        if (start_ret != 0) {
            ERROR("Failed to start F2H DMAC read transfer");
            vTaskDelay(pdMS_TO_TICKS(5));
            goto cleanup;
        }

        sem_ret = osal_semaphore_wait(f2h_read_done_sem, 10000);
        if (sem_ret != pdTRUE) {
            ERROR("Failed to wait for F2H DMAC read transfer");
            vTaskDelay(pdMS_TO_TICKS(5));
            goto cleanup;
        }

        num_iterations++;
        READ_TIMER(transfer_end);
    }

    // Calculate and print transfer statistics for read operation
    elapsed_time = (double)(transfer_end - transfer_start) / (double)counter_freq;
    total_data_transferred = (NUM_BLOCKS * BLOCK_SIZE * ELEMENT_SIZE) * num_iterations / ONE_MIL_BYTES;
    PRINT("Total data transferred in a %u-second F2H read operation: %lu MB -- Average transfer bandwidth is: %f MB/s",
          transaction_seconds, total_data_transferred, (total_data_transferred / elapsed_time));
    vTaskDelay(pdMS_TO_TICKS(5));

    ///////////////////////
    /* Clean up and exit */
    ///////////////////////

cleanup:
    free((void *)sdram_buf);
    osal_semaphore_delete(f2h_read_done_sem);
    return;
}