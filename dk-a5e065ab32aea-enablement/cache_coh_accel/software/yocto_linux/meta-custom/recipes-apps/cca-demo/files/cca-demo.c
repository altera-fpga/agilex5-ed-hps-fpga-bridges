/*
 * Linux userspace CCA driver usage sample application
 *
 * SPDX-License-Identifier: MIT-0
 * Copyright (C) 2026 Altera Corporation
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cca-uapi.h"

// Number of runs submitted back to back in a single demo run
#define DEMO_NUM_RUNS 1u

// Within a single run, how many jobs are chained
#define DEMO_NUM_JOBS 3u

// The size of each job in a chain
#define DEMO_JOB_SIZE 128u /* bytes; must be a multiple of CCA_TRANSFER_ALIGN */
#define DEMO_WORDS_PER_JOB (DEMO_JOB_SIZE / sizeof(uint32_t))
#define DEMO_TIMEOUT_MS 5000u

// Char device node exposed by the driver
#define CCA_DEV "/dev/cca0"

static void print_buffer(const uint32_t *buf, uint32_t nwords)
{
    for (uint32_t i = 0; i < nwords; i++) {
        if ((i % 4) == 0)
            printf("    [%02u-%02u]  ", i, i + 3u < nwords ? i + 3u : nwords - 1u);
        printf("0x%08x  ", (unsigned)buf[i]);
        if ((i % 4) == 3)
            printf("\n");
    }
    if (nwords % 4)
        printf("\n");
}

/** run_one_batch() - alloc, submit, verify, then free slab without hardware reset */
static int run_one_batch(int fd, uint32_t run_idx)
{
    struct cca_alloc_req alloc;
    struct cca_hw_status hw_st;
    void *slab = MAP_FAILED;
    uint32_t timeout_ms = DEMO_TIMEOUT_MS;
    int pass = 1;

    memset(&alloc, 0, sizeof(alloc));

    printf("\n--- Run %u/%u ---\n", run_idx + 1u, DEMO_NUM_RUNS);

    // Build the length array, one index represents the size of one job in a chain
    uint32_t lens[DEMO_NUM_JOBS];
    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++)
        lens[j] = DEMO_JOB_SIZE;

    alloc.n_jobs = DEMO_NUM_JOBS;
    alloc.cmd = CCA_CMD_PROCESS_DATA;
    alloc.lengths_ptr = (uint64_t)(uintptr_t)lens;

    // Allocate a DMA slab that holds both input and output buffers, offsets to each job filled by driver
    if (ioctl(fd, CCA_IOC_ALLOC, &alloc) < 0) {
        perror("CCA_IOC_ALLOC");
        return -1;
    }

    // Map the DMA slab allocated in prev step to userspace
    slab = mmap(NULL, (size_t)alloc.total_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (slab == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    // Populate the input buffer for each job and zero the output buffer
    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
        // Locate the ptr to the input and output buffer for this job
        uint32_t *in_buf = (uint32_t *)((uint8_t *)slab + alloc.jobs[j].in_offset);
        uint32_t *out_buf = (uint32_t *)((uint8_t *)slab + alloc.jobs[j].out_offset);

        // The data pattern used as input: High byte = run/job identifier, Low byte = word index
        uint32_t base = (0xA0u + run_idx * DEMO_NUM_JOBS + j) << 24;
        for (uint32_t i = 0; i < DEMO_WORDS_PER_JOB; i++)
            in_buf[i] = base | i;

        memset(out_buf, 0x00, DEMO_JOB_SIZE); // Not technically needed, the kernel alloc zeros the buffers already

        printf("  Job %u input:\n", j);
        print_buffer(in_buf, DEMO_WORDS_PER_JOB);
    }

    // Submit the batch of jobs to the hardware
    if (ioctl(fd, CCA_IOC_SUBMIT) < 0) {
        perror("CCA_IOC_SUBMIT");
        pass = -1;
        goto out_munmap;
    }

    // Block until hardware signals completion OR timeout
    if (ioctl(fd, CCA_IOC_WAIT, &timeout_ms) < 0) {
        perror("CCA_IOC_WAIT");
        pass = -1;
        goto out_munmap;
    }

    // For each job in the chain
    for (uint32_t j = 0; j < DEMO_NUM_JOBS; j++) {
        const uint32_t *in_buf = (const uint32_t *)((uint8_t *)slab + alloc.jobs[j].in_offset);
        const uint32_t *out_buf = (const uint32_t *)((uint8_t *)slab + alloc.jobs[j].out_offset);
        int job_pass = 1;

        printf("  Job %u received:\n", j);
        print_buffer(out_buf, DEMO_WORDS_PER_JOB);

        // Validate the result of the output buffers after the transfer
        for (uint32_t i = 0; i < DEMO_WORDS_PER_JOB; i++) {
            if (out_buf[i] != ~in_buf[i]) {
                fprintf(stderr,
                        "  Job %u word[%u]: sent 0x%08x "
                        "expected 0x%08x received 0x%08x\n",
                        j, i, (unsigned)in_buf[i], (unsigned)~in_buf[i], (unsigned)out_buf[i]);
                job_pass = 0;
                break;
            }
        }
        printf("  Job %u: %s\n", j, job_pass ? "PASS" : "FAIL");
        if (!job_pass)
            pass = 0;
    }

out_munmap:
    munmap(slab, (size_t)alloc.total_mmap_size);

    if (pass < 0)
        return pass;

    // Free allocated DMA slab, only call this after munmap
    if (ioctl(fd, CCA_IOC_FREE) < 0) {
        perror("CCA_IOC_FREE");
        return -1;
    }

    return pass;
}

int main(void)
{
    int fd = -1;
    int rc = EXIT_FAILURE;
    int all_pass = 1;

    printf("========================================\n");
    printf("  CCA Linux Demo\n");
    printf("  %u jobs x %u bytes (out=~in)\n", DEMO_NUM_JOBS, DEMO_JOB_SIZE);
    printf("========================================\n");

    fd = open(CCA_DEV, O_RDWR);
    if (fd < 0) {
        perror("open");
        goto cleanup;
    }

    // Run multiple separate runs
    for (uint32_t r = 0; r < DEMO_NUM_RUNS; r++) {
        int result = run_one_batch(fd, r);

        if (result < 0)
            goto cleanup;
        if (result == 0)
            all_pass = 0;
    }

    rc = all_pass ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
    printf("\n========================================\n");
    printf("  Overall result: %s\n", (rc == EXIT_SUCCESS) ? "PASSED" : "FAILED");
    printf("========================================\n");

    if (fd >= 0) {
        if (ioctl(fd, CCA_IOC_RESET) < 0)
            perror("CCA_IOC_RESET");
        close(fd);
    }

    return rc;
}
