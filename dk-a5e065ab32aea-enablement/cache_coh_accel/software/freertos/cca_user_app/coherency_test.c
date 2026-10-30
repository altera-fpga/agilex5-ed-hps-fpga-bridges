/*
 * SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * CCA Cache Coherency Verification Task Implementation
 *
 * See coherency_test.h for the public API.
 *
 * Test Methodology:
 *
 *   Both buffers are setup so that DRAM and the CPU cache each carry 
 *   different data patterns.  Then submit data to the accelerator, causing a
 *   transfer that runs BUFFER-0 (src) -> BUFFER-1 (dst).
 *
 *   Pre-transfer state:
 *
 *          BUFFER-0 (src) BUFFER-1 (dst)
 *         +-------------+-------------+
 *    DRAM |  PATTERN_3  |  PATTERN_5  |
 *         +-------------+-------------+
 *
 *         +-------------+-------------+
 *   CACHE |  PATTERN_A  |  PATTERN_C  |
 *         +-------------+-------------+
 *
 *   Expected post-transfer state:
 *
 *               +-----DMA-----+
 *               |             |
 *               ^             V
 *          BUFFER-0 (src) BUFFER-1 (dst)
 *         +-------------+-------------+
 *    DRAM |  PATTERN_3  |  PATTERN_A  |
 *         +-------------+-------------+
 *
 *         +-------------+-------------+
 *   CACHE |  PATTERN_A  |  ---------  |
 *         +-------------+-------------+
 *
 *   If the coherency works correctly:
 *     Input DMA: DMA read snoops the CPU's cache line for src and
 *                fetches PATTERN_A
 *     Output DMA: DMA write commits PATTERN_A to dst buffer in DRAM and invalidates
 *                 the CPU's cache line for dst.
 *
 *   Verification:
 *     dst == PATTERN_A, then coherency is working
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "osal_log.h"
#include "socfpga_cache.h"
#include "cca_driver.h"
#include "coherency_test.h"

/* Transfer timeout */
#define COHERENCY_TIMEOUT_MS 5000U

/* Buffer size — actual data per buffer */
#define COHERENCY_BUF_SIZE 4096U

/*
 * config_page_caching() operates at a 2 MB granule.  Allocating each buffer
 * at 2 MB alignment with a 2 MB size guarantees the entire granule belongs
 * to the test and contains no FreeRTOS kernel data (TCBs, stacks, etc.).
 *
 * mode 0 = non-cacheable (NC): accesses bypass cache and go directly to DRAM.
 * mode 1 = cacheable:          accesses go through the L1/L2 cache as normal.
 */
#define NC_GRANULE (2U * 1024U * 1024U)

extern void config_page_caching(void *addr, int mode);

// The upper 2 bytes identify the PATTERN
// The lower 2 bytes identify the index
#define PATTERN_3(i) (0x33330000U | (uint32_t)(i))
#define PATTERN_5(i) (0x55550000U | (uint32_t)(i))
#define PATTERN_A(i) (0xAAAA0000U | (uint32_t)(i))
#define PATTERN_C(i) (0xCCCC0000U | (uint32_t)(i))

/* Number of words to print for before/after snapshots */
#define COHERENCY_PRINT_WORDS 8U

/*
 * print_words() - print the first nwords of a buffer, 4 per line
 */
static void print_words(const uint32_t *buf, uint32_t nwords)
{
    for (uint32_t i = 0; i < nwords; i++) {
        if ((i % 4) == 0)
            printf("    [%02u-%02u]  ", i, i + 3U < nwords ? i + 3U : nwords - 1U);
        printf("0x%08x  ", (unsigned)buf[i]);
        if ((i % 4) == 3)
            printf("\r\n");
    }
    if (nwords % 4)
        printf("\r\n");
}

/*
 * write_to_cache() - Write a pattern into a buffer word-by-word with a DSB
 * before each store.
 *
 * A DSB before each store is a defence against write-streaming, which may
 * result in the cache being bypassed
 */
static void write_to_cache(uint32_t *buf, uint32_t nwords, uint32_t (*pattern)(uint32_t))
{
    for (uint32_t i = 0; i < nwords; i++) {
        __asm__ volatile("dsb sy" ::: "memory");
        buf[i] = pattern(i);
    }
}

static uint32_t pat_a(uint32_t i)
{
    return PATTERN_A(i);
}
static uint32_t pat_c(uint32_t i)
{
    return PATTERN_C(i);
}

/*
 * verify_setup() - Stage both buffers and verify the pre-transfer
 * state using non-cacheable DRAM reads.
 *
 * Each buffer is staged and verified in four steps:
 *
 *   1. NC write  - switch the 2 MB granule to NC, write the DRAM pattern
 *                  directly to DRAM, verify via NC
 *                  read.
 * 
 *   2. C write   - switch back to cacheable, call write_to_cache() which uses
 *                  a per-store DSB to prevent write-streaming.
 * 
 *   3. NC verify - switch to NC again and read DRAM.  Because NC reads bypass
 *                  the cache entirely, this is a direct DRAM read.  DRAM must
 *                  still hold the DRAM pattern written in step 1.
 * 
 *   4. C verify  - switch back to cacheable and read cache data. The
 *                  dirty cache line is still present, so this is
 *                  a cache hit returning the cache pattern.
 *
 * Both buffers are left in cacheable state with their dirty cache lines intact
 * so that the coherency interconnect can snoop them during the DMA.
 *
 * Returns true if all four patterns are staged and verified correctly.
 */
static bool verify_setup(uint32_t *src, uint32_t *dst, uint32_t nwords)
{
    volatile uint32_t *src_nc = (volatile uint32_t *)src;
    volatile uint32_t *dst_nc = (volatile uint32_t *)dst;

    /* SRC buffer setup */

    // Clean + invalidate any stale cache state before switching to NC
    cache_flush(src, COHERENCY_BUF_SIZE);

    // Step 1: NC write, switch to NC, write PATTERN_3 directly to DRAM
    config_page_caching(src, 0);
    for (uint32_t i = 0; i < nwords; i++)
        src_nc[i] = PATTERN_3(i);

    // Verify DRAM via NC read
    for (uint32_t i = 0; i < nwords; i++) {
        if (src_nc[i] != PATTERN_3(i)) {
            ERROR("  setup: DRAM[src][%u] = 0x%08x, expected 0x%08x", (unsigned)i, (unsigned)src_nc[i],
                  (unsigned)PATTERN_3(i));
            config_page_caching(src, 1);
            return false;
        }
    }
    PRINT("  setup: Source DRAM has PATTERN_3 (0x3333xxxx)");

    // Step 2: C write, switch to cacheable, dirty the cache with PATTERN_A
    config_page_caching(src, 1);
    write_to_cache(src, nwords, pat_a);

    // Step 3: NC verify, switch to NC, DRAM must still be PATTERN_3
    // If write-streaming occurred during step 2, DRAM has PATTERN_A and this fails
    config_page_caching(src, 0);
    for (uint32_t i = 0; i < nwords; i++) {
        if (src_nc[i] != PATTERN_3(i)) {
            ERROR("  setup: DRAM[src][%u] = 0x%08x, expected 0x%08x", (unsigned)i, (unsigned)src_nc[i],
                  (unsigned)PATTERN_3(i));
            config_page_caching(src, 1);
            return false;
        }
    }
    PRINT("  setup: Source DRAM still has PATTERN_3 after cache write");

    // Step 4: C verify, switch back to cacheable and read
    config_page_caching(src, 1);
    for (uint32_t i = 0; i < nwords; i++) {
        if (src[i] != PATTERN_A(i)) {
            ERROR("  setup: cache[src][%u] = 0x%08x, expected 0x%08x", (unsigned)i, (unsigned)src[i],
                  (unsigned)PATTERN_A(i));
            return false;
        }
    }
    PRINT("  setup: Source cache has PATTERN_A (0xAAAAxxxx)");

    /* DST buffer setup */

    // Clean + invalidate any stale cache state before switching to NC
    cache_flush(dst, COHERENCY_BUF_SIZE);

    // Step 1: NC write, switch to NC, write PATTERN_5 directly to DRAM
    config_page_caching(dst, 0);
    for (uint32_t i = 0; i < nwords; i++)
        dst_nc[i] = PATTERN_5(i);

    // Verify DRAM via NC read
    for (uint32_t i = 0; i < nwords; i++) {
        if (dst_nc[i] != PATTERN_5(i)) {
            ERROR("  setup: DRAM[dst][%u] = 0x%08x, expected 0x%08x", (unsigned)i, (unsigned)dst_nc[i],
                  (unsigned)PATTERN_5(i));
            config_page_caching(dst, 1);
            return false;
        }
    }
    PRINT("  setup: Destination DRAM has PATTERN_5 (0x5555xxxx)");

    // Step 2: C write, switch to cacheable, dirty the cache with PATTERN_C
    config_page_caching(dst, 1);
    write_to_cache(dst, nwords, pat_c);

    // Step 3: NC verify, switch to NC, DRAM must still be PATTERN_5
    // If write-streaming occurred during step 2, DRAM has PATTERN_C and this fails
    config_page_caching(dst, 0);
    for (uint32_t i = 0; i < nwords; i++) {
        if (dst_nc[i] != PATTERN_5(i)) {
            ERROR("  setup: DRAM[dst][%u] = 0x%08x, expected 0x%08x", (unsigned)i, (unsigned)dst_nc[i],
                  (unsigned)PATTERN_5(i));
            config_page_caching(dst, 1);
            return false;
        }
    }
    PRINT("  setup: Destination DRAM still has PATTERN_5 after cache write");

    // Step 4: C verify, switch back to cacheable and read
    config_page_caching(dst, 1);
    for (uint32_t i = 0; i < nwords; i++) {
        if (dst[i] != PATTERN_C(i)) {
            ERROR("  setup: cache[dst][%u] = 0x%08x, expected 0x%08x", (unsigned)i, (unsigned)dst[i],
                  (unsigned)PATTERN_C(i));
            return false;
        }
    }
    PRINT("  setup: Destination cache has PATTERN_C (0xCCCCxxxx)");

    return true;
}

/*
 * run_coherency_test()
 *
 * Returns true when both input-side (DMA read snoops CPU dirty cache for src)
 * and output-side (DMA write invalidates CPU cache for dst) coherency are
 * confirmed.
 */
static bool run_coherency_test(cca_driver_t *drv)
{
    const uint32_t nwords = COHERENCY_BUF_SIZE / sizeof(uint32_t);
    bool pass = false;

    /* Allocate the 2MB space so that all data when swapping to/from NC and C
     * belong exclusively to the buffer (so that there are no side effects of swapping)
     */
    uint32_t *src = (uint32_t *)aligned_alloc(NC_GRANULE, NC_GRANULE);
    uint32_t *dst = (uint32_t *)aligned_alloc(NC_GRANULE, NC_GRANULE);
    if (!src || !dst) {
        ERROR("  coherency_test: buffer alloc failed");
        free(src);
        free(dst);
        return false;
    }

    PRINT("  Verifying pre-transfer setup state...");
    if (!verify_setup(src, dst, nwords)) {
        ERROR("  coherency_test: setup verification failed — aborting test");
        goto cleanup;
    }

    // Print the confirmed pre-transfer state
    PRINT("\r\n  Pre-transfer state confirmed:");
    PRINT("  src cache (expect PATTERN_A = 0xAAAAxxxx):");
    print_words(src, COHERENCY_PRINT_WORDS);
    PRINT("  dst cache (expect PATTERN_C = 0xCCCCxxxx):");
    print_words(dst, COHERENCY_PRINT_WORDS);
    PRINT("\r\n  Submitting coherent DMA transfer (src -> dst)...\r\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Step 3: Submit the data to the hardware
    cca_job_t job = { .src = src, .dst = dst, .size = COHERENCY_BUF_SIZE };

    cca_status_t rc = cca_transfer_submit(drv, &job, 1U, CCA_CMD_PASS_THROUGH);
    if (rc != CCA_OK) {
        ERROR("  coherency_test: cca_transfer_submit failed (code %d)", (int)rc);
        goto cleanup;
    }

    rc = cca_transfer_wait(drv, COHERENCY_TIMEOUT_MS);
    if (rc != CCA_OK) {
        ERROR("  coherency_test: cca_transfer_wait failed (code %d)", (int)rc);
        goto cleanup;
    }

    // Step 4: Verify results are what we expect
    // Input Cache: PATTERN_A -> PATTERN_A
    // Input DRAM: PATTERN_3 -> PATTERN_3
    // Output Cache: PATTERN_C -> INVALID
    // Output DRAM: PATTERN_5 -> PATTERN_A
    PRINT("dst after transfer (expect PATTERN_A = 0xAAAAxxxx):");
    print_words(dst, COHERENCY_PRINT_WORDS);
    vTaskDelay(pdMS_TO_TICKS(50));

    pass = true;
    for (uint32_t i = 0; i < nwords; i++) {
        const uint32_t got = dst[i];
        const uint32_t expected = PATTERN_A(i);

        if (got == expected)
            continue;

        if (got == PATTERN_3(i)) {
            ERROR("  word[%u]: 0x%08x = PATTERN_3: DMA read stale DRAM from source, read coherency broken", (unsigned)i,
                  (unsigned)got);
        } else if (got == PATTERN_C(i)) {
            ERROR("  word[%u]: 0x%08x = PATTERN_C: Destination cache not invalidated, write coherency broken",
                  (unsigned)i, (unsigned)got);
        } else if (got == PATTERN_5(i)) {
            ERROR("  word[%u]: 0x%08x = PATTERN_5: Destination DRAM not updated, write did not complete", (unsigned)i,
                  (unsigned)got);
        } else {
            ERROR("  word[%u]: got 0x%08x  expected 0x%08x (PATTERN_A)", (unsigned)i, (unsigned)got,
                  (unsigned)expected);
        }

        pass = false;
        break;
    }

cleanup:
    free(src);
    free(dst);
    return pass;
}

/*
 * coherency_test_run() - task body
 */
static void coherency_test_run(void *arg)
{
    cca_driver_t *drv = (cca_driver_t *)arg;

    /* Block until the demo task notifies us to start */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    PRINT("\r\n========================================");
    PRINT("  CCA Cache Coherency Verification");
    PRINT("========================================");
    vTaskDelay(pdMS_TO_TICKS(50));

    cca_reset(drv);

    bool result = run_coherency_test(drv);

    PRINT("========================================");
    vTaskDelay(pdMS_TO_TICKS(50));
    PRINT("  Cache coherency: %s", result ? "PASSED" : "FAILED");
    PRINT("========================================\r\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    vTaskSuspend(NULL);
}

TaskHandle_t coherency_test_task_create(cca_driver_t *drv, UBaseType_t priority)
{
    TaskHandle_t handle = NULL;

    BaseType_t rc =
        xTaskCreate(coherency_test_run, "cca_coherency", configMINIMAL_STACK_SIZE * 8, drv, priority, &handle);
    if (rc != pdPASS)
        return NULL;

    return handle;
}
