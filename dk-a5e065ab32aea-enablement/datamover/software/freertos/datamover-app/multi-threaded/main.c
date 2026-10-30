/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * main.c - Main program for SoC FPGA Data Mover Design (multi-threaded)
 */

#include <stdlib.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "socfpga_interrupt.h"
#include "socfpga_console.h"
#include "socfpga_smmu.h"
#include "socfpga_flash.h"
#include "socfpga_fpga_manager.h"
#include "event_groups.h"
#include "socfpga_bridge.h"
#include "datamover_tasks.h"
#include "qos_generator.h"
#include "socfpga_dma.h"
#include "socfpga_cache.h"
#include "osal.h"
#include "osal_log.h"

#define TASK_PRIORITY (configMAX_PRIORITIES - 2)

// FPGA bitstream offset in QSPI memory
#define BITSTREAM_OFFSET 0x08000000

#define SETUP_FPGA_THREAD_BIT (1 << 0)
#define F2H_READ_THREAD_BIT (1 << 1)
#define F2H_WRITE_THREAD_BIT (1 << 2)
#define F2SDRAM_READ_THREAD_BIT (1 << 3)
#define F2SDRAM_WRITE_THREAD_BIT (1 << 4)
#define H2F_READ_THREAD_BIT (1 << 5)
#define H2F_WRITE_THREAD_BIT (1 << 6)
#define LWH2F_READ_THREAD_BIT (1 << 7)
#define LWH2F_WRITE_THREAD_BIT (1 << 8)
#define S2S_DMA_THREAD_BIT (1 << 9)
#define S2S_THREAD_BIT (1 << 10)

// Add thread bit if the thread is enabled
#define ALL_THREADS                                                                                    \
    (F2H_READ_THREAD_BIT | F2H_WRITE_THREAD_BIT | F2SDRAM_READ_THREAD_BIT | F2SDRAM_WRITE_THREAD_BIT | \
     S2S_DMA_THREAD_BIT | S2S_THREAD_BIT)

void vApplicationTickHook(void)
{
    /*
     * This is called from RTOS tick handler
     * Not used in this demo, but defined to keep the configuration sharing
     * simple
     * */
}

void vApplicationIdleHook(void)
{
    ;
}

void vApplicationMallocFailedHook(void)
{
    /* vApplicationMallocFailedHook() will only be called if
       configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
       function that will get called if a call to pvPortMalloc() fails.
       pvPortMalloc() is called internally by the kernel whenever a task, queue,
       timer or semaphore is created. */
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}

static void prvSetupHardware(void)
{
    /* Initialize the GIC. */
    interrupt_init_gic();
    /* Enable SMMU */
    (void)smmu_enable();
    /* Initialize the console uart*/
#if configENABLE_CONSOLE_UART
    console_init(configCONSOLE_UART_ID, "115200-8N1");
#endif
}

int load_bitstream_from_qspi(void)
{
    int status = 1; // default to failure

    void *buffer = NULL;

    flash_handle_t flash_handle = flash_open(QSPI_DEV0);
    if (flash_handle == NULL) {
        ERROR("Failed to open flash device");
        vTaskDelay(pdMS_TO_TICKS(5));
        return status;
    }

    // The first 4 bytes at the bitstream offset contain the size of the bitstream, followed by the bitstream data itself.
    // Here, we read the size and place it into the bitstream size variable.
    size_t bitstream_size;
    uint8_t bitstream_size_buffer[4];
    int ret = flash_read_sync(flash_handle, BITSTREAM_OFFSET, bitstream_size_buffer, sizeof(uint32_t));
    if (ret != 0) {
        ERROR("Failed to read bitstream length from flash: %d", ret);
        vTaskDelay(pdMS_TO_TICKS(5));
        flash_close(flash_handle);
        return status;
    }
    bitstream_size = ((uint32_t)bitstream_size_buffer[3] << 24) | ((uint32_t)bitstream_size_buffer[2] << 16) |
                     ((uint32_t)bitstream_size_buffer[1] << 8) | ((uint32_t)bitstream_size_buffer[0]);
    PRINT("Bitstream size: %u bytes", bitstream_size);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Allocate a buffer for the bitstream data.
    buffer = malloc(bitstream_size);
    if (buffer == NULL) {
        ERROR("Failed to allocate memory for bitstream");
        vTaskDelay(pdMS_TO_TICKS(5));
        flash_close(flash_handle);
        return status;
    }

    // Read the bitstream data. The bitstream data starts immediately after the 4-byte size field, so we add sizeof(uint32_t) to the offset.
    ret = flash_read_sync(flash_handle, BITSTREAM_OFFSET + sizeof(uint32_t), buffer, bitstream_size);
    if (ret != 0) {
        ERROR("Failed to read bitstream from flash: %d", ret);
        vTaskDelay(pdMS_TO_TICKS(5));
        free(buffer);
        flash_close(flash_handle);
        return status;
    }

    // Flush bitstream data from CPU cache to DDR and ensure all memory operation completed before proceeding
    PRINT("Flushing bitstream data from cache to memory...");
    vTaskDelay(pdMS_TO_TICKS(5));
    cache_flush(buffer, bitstream_size);
    __asm__ volatile("dsb sy" ::: "memory");

    // load_fpga_bitstream sends the bitstream data to the SDM which programs the FPGA.
    PRINT("Loading FPGA bitstream...");
    vTaskDelay(pdMS_TO_TICKS(5));
    ret = load_fpga_bitstream(buffer, bitstream_size);
    if (ret != 0) {
        ERROR("Failed to load FPGA bitstream: %d", ret);
        vTaskDelay(pdMS_TO_TICKS(5));
        free(buffer);
        flash_close(flash_handle);
        return status;
    }

    status = 0;

    free(buffer);
    flash_close(flash_handle);
    return status;
}

void setup_fpga(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    PRINT("=====================================");
    vTaskDelay(pdMS_TO_TICKS(5));
    PRINT("Data Mover FreeRTOS Program Starting.");
    vTaskDelay(pdMS_TO_TICKS(5));
    PRINT("=====================================\n");
    vTaskDelay(pdMS_TO_TICKS(5));

    // Load the FPGA bitstream from QSPI memory
    if (load_bitstream_from_qspi() != 0) {
        ERROR("Failed to load FPGA bitstream. Halting execution.");
        vTaskDelay(pdMS_TO_TICKS(5));
        for (;;)
            ;
    }
    PRINT("FPGA bitstream loaded successfully.\n");
    vTaskDelay(pdMS_TO_TICKS(5));

    // configure qos generator
    configure_qos_generator(TBU2NOC_QOS_MODE_REG_ADDR, 0x0);
    configure_qos_generator(TBU2NOC_QOS_PRIORITY_REG_ADDR, 0x0);

    configure_qos_generator(DMA_QOS_MODE_REG_ADDR, 0x0);
    configure_qos_generator(DMA_QOS_PRIORITY_REG_ADDR, 0x0);

    configure_qos_generator(CCU_DMI0_QOS_MODE_REG_ADDR, 0x0);
    configure_qos_generator(CCU_DMI0_QOS_PRIORITY_REG_ADDR, 0x0);

    configure_qos_generator(CCU_DMI1_QOS_MODE_REG_ADDR, 0x0);
    configure_qos_generator(CCU_DMI1_QOS_PRIORITY_REG_ADDR, 0x0);

    PRINT("FPGA setup complete.");
    vTaskDelay(pdMS_TO_TICKS(5));

    // signal that the FPGA setup is complete
    xEventGroupSetBits(execution_barrier, SETUP_FPGA_THREAD_BIT);

    vTaskSuspend(NULL);
}

void f2h_read(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("F2H Read Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    f2h_read_task(execution_barrier, F2H_READ_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void f2h_write(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("F2H Write Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    f2h_write_task(execution_barrier, F2H_WRITE_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void f2sdram_read(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("F2SDRAM Read Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    f2sdram_read_task(execution_barrier, F2SDRAM_READ_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void f2sdram_write(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("F2SDRAM Write Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    f2sdram_write_task(execution_barrier, F2SDRAM_WRITE_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void h2f_read(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("H2F Read Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    h2f_read_task(execution_barrier, H2F_READ_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void h2f_write(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("H2F Write Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    h2f_write_task(execution_barrier, H2F_WRITE_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void lwh2f_read(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("LWH2F Read Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    lwh2f_read_task(execution_barrier, LWH2F_READ_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void lwh2f_write(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("LWH2F Write Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    lwh2f_write_task(execution_barrier, LWH2F_WRITE_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void s2s_dma(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("S2S DMA Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    s2s_dma_task(execution_barrier, S2S_DMA_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

void s2s(void *arg)
{
    EventGroupHandle_t execution_barrier = (EventGroupHandle_t)arg;

    // wait for the FPGA setup to complete
    xEventGroupWaitBits(execution_barrier, SETUP_FPGA_THREAD_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINT("S2S Task Started.");
    vTaskDelay(pdMS_TO_TICKS(5));
    s2s_task(execution_barrier, S2S_THREAD_BIT, ALL_THREADS, TEN_SECONDS);
    vTaskSuspend(NULL);
}

int main(void)
{
    prvSetupHardware();

    // create task execution barrier
    EventGroupHandle_t execution_barrier;
    execution_barrier = xEventGroupCreate();
    if (execution_barrier == NULL) {
        ERROR("Failed to create event group");
        vTaskDelay(pdMS_TO_TICKS(5));
        for (;;)
            ;
    }

    // create tasks
    BaseType_t setup_xReturn;
    setup_xReturn = xTaskCreate(setup_fpga, "SETUP_FPGA", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier,
                                TASK_PRIORITY, NULL);

    BaseType_t f2h_read_xReturn;
    BaseType_t f2h_write_xReturn;
    f2h_read_xReturn =
        xTaskCreate(f2h_read, "F2H_READ", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);
    f2h_write_xReturn = xTaskCreate(f2h_write, "F2H_WRITE", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier,
                                    TASK_PRIORITY, NULL);

    BaseType_t f2sdram_read_xReturn;
    BaseType_t f2sdram_write_xReturn;
    f2sdram_read_xReturn = xTaskCreate(f2sdram_read, "F2SDRAM_READ", configMINIMAL_STACK_SIZE * 4,
                                       (void *)execution_barrier, TASK_PRIORITY, NULL);
    f2sdram_write_xReturn = xTaskCreate(f2sdram_write, "F2SDRAM_WRITE", configMINIMAL_STACK_SIZE * 4,
                                        (void *)execution_barrier, TASK_PRIORITY, NULL);

    /* Uncomment to enable H2F and LWH2F tasks */
    // BaseType_t h2f_read_xReturn;
    // BaseType_t h2f_write_xReturn;
    // h2f_read_xReturn = xTaskCreate(h2f_read, "H2F_READ", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);
    // h2f_write_xReturn = xTaskCreate(h2f_write, "H2F_WRITE", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);

    // BaseType_t lwh2f_read_xReturn;
    // BaseType_t lwh2f_write_xReturn;
    // lwh2f_read_xReturn = xTaskCreate(lwh2f_read, "LWH2F_READ", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);
    // lwh2f_write_xReturn = xTaskCreate(lwh2f_write, "LWH2F_WRITE", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);

    BaseType_t s2s_dma_xReturn;
    s2s_dma_xReturn =
        xTaskCreate(s2s_dma, "S2S_DMA", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);

    BaseType_t s2s_xReturn;
    s2s_xReturn = xTaskCreate(s2s, "S2S", configMINIMAL_STACK_SIZE * 4, (void *)execution_barrier, TASK_PRIORITY, NULL);

    if (setup_xReturn == pdPASS && f2h_read_xReturn == pdPASS && f2h_write_xReturn == pdPASS &&
        f2sdram_read_xReturn == pdPASS && f2sdram_write_xReturn == pdPASS && s2s_dma_xReturn == pdPASS &&
        s2s_xReturn == pdPASS) {
        vTaskStartScheduler();
    }

    /* Block here indefinitely; Should never reach here */
    for (;;)
        ;
}
