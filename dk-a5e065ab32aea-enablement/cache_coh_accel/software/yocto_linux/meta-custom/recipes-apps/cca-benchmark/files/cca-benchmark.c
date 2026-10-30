/*
 * Linux CCA throughput benchmark
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
#include <time.h>
#include <unistd.h>
#include "cca-uapi.h"

#define CCA_DEV "/dev/cca0"
#define TIMEOUT_MS 5000u
#define WARMUP_ITERS 5u
#define MEASURE_ITERS 50u

static uint64_t ns_elapsed(const struct timespec *a, const struct timespec *b)
{
    return (uint64_t)(b->tv_sec - a->tv_sec) * UINT64_C(1000000000) + (uint64_t)(b->tv_nsec - a->tv_nsec);
}

/*
 * run_config() - benchmark one (n_jobs x job_size) configuration.
 *
 * Returns 0 on success, -1 on error.
 */
static int run_config(int fd, uint32_t n_jobs, uint32_t job_size)
{
    struct cca_alloc_req alloc;
    uint32_t lens[CCA_MAX_JOBS];
    void *slab = MAP_FAILED;
    uint32_t timeout_ms = TIMEOUT_MS;
    uint64_t total_ns = 0;
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
    int rc = -1;

    for (uint32_t j = 0; j < n_jobs; j++)
        lens[j] = job_size;

    memset(&alloc, 0, sizeof(alloc));
    alloc.n_jobs = n_jobs;
    alloc.cmd = CCA_CMD_PROCESS_DATA;
    alloc.lengths_ptr = (uint64_t)(uintptr_t)lens;

    if (ioctl(fd, CCA_IOC_ALLOC, &alloc) < 0) {
        perror("CCA_IOC_ALLOC");
        return -1;
    }

    slab = mmap(NULL, (size_t)alloc.total_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (slab == MAP_FAILED) {
        perror("mmap");
        goto out_free;
    }

    /* Write a known pattern into every input buffer once. */
    for (uint32_t j = 0; j < n_jobs; j++)
        memset((uint8_t *)slab + alloc.jobs[j].in_offset, (int)(0xA0u + j), job_size);

    /* Warmup (not timed) */
    for (uint32_t i = 0; i < WARMUP_ITERS; i++) {
        if (ioctl(fd, CCA_IOC_SUBMIT) < 0) {
            perror("warmup: CCA_IOC_SUBMIT");
            goto out_unmap;
        }
        if (ioctl(fd, CCA_IOC_WAIT, &timeout_ms) < 0) {
            perror("warmup: CCA_IOC_WAIT");
            goto out_unmap;
        }
    }

    /* Measured iterations */
    for (uint32_t i = 0; i < MEASURE_ITERS; i++) {
        struct timespec t0, t1;

        clock_gettime(CLOCK_MONOTONIC, &t0);

        if (ioctl(fd, CCA_IOC_SUBMIT) < 0) {
            perror("CCA_IOC_SUBMIT");
            goto out_unmap;
        }
        if (ioctl(fd, CCA_IOC_WAIT, &timeout_ms) < 0) {
            perror("CCA_IOC_WAIT");
            goto out_unmap;
        }

        clock_gettime(CLOCK_MONOTONIC, &t1);
        {
            uint64_t elapsed = ns_elapsed(&t0, &t1);
            total_ns += elapsed;
            if (elapsed < min_ns)
                min_ns = elapsed;
            if (elapsed > max_ns)
                max_ns = elapsed;
        }
    }

    /* Report */
    {
        uint32_t xfer_KiB = n_jobs * job_size / 1024u;
        double avg_ms = (double)total_ns / MEASURE_ITERS / 1e6;
        double min_ms = (double)min_ns / 1e6;
        double max_ms = (double)max_ns / 1e6;
        double bytes_total = (double)n_jobs * job_size * 2.0;
        double mbps_avg = bytes_total / 1e6 / (avg_ms / 1000.0);
        double mbps_min = bytes_total / 1e6 / (max_ms / 1000.0);
        double mbps_max = bytes_total / 1e6 / (min_ms / 1000.0);

        printf("  jobs=%2u  job=%3u KiB  xfer=%4u KiB"
               "  avg=%7.3f ms (min=%.3f max=%.3f)"
               "  %7.2f MB/s (min=%.2f max=%.2f)\n",
               n_jobs, job_size / 1024u, xfer_KiB, avg_ms, min_ms, max_ms, mbps_avg, mbps_min, mbps_max);
    }

    rc = 0;

out_unmap:
    munmap(slab, (size_t)alloc.total_mmap_size);
out_free:
    if (ioctl(fd, CCA_IOC_FREE) < 0)
        perror("CCA_IOC_FREE");
    return rc;
}

int main(void)
{
    int fd;
    int rc = EXIT_FAILURE;

    printf("=====================================================\n");
    printf("  CCA Throughput Benchmark - Linux\n");
    printf("=====================================================\n\n");

    fd = open(CCA_DEV, O_RDWR);
    if (fd < 0) {
        perror("open " CCA_DEV);
        return EXIT_FAILURE;
    }

    if (ioctl(fd, CCA_IOC_RESET) < 0) {
        perror("CCA_IOC_RESET");
        goto done;
    }

    printf("Benchmark 1: job_size=32 KB, n_jobs = 1 to 32\n");
    {
        static const uint32_t counts[] = { 1, 2, 4, 8, 16, 32 };
        for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
            if (run_config(fd, counts[i], 32u * 1024u) < 0)
                goto done;
        }
    }

    printf("\nBenchmark 2: n_jobs=8, job_size = 4 to 96 KB\n");
    {
        static const uint32_t sizes[] = {
            4u * 1024u, 16u * 1024u, 32u * 1024u, 64u * 1024u, 96u * 1024u,
        };
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            if (run_config(fd, 8u, sizes[i]) < 0)
                goto done;
        }
    }

    rc = EXIT_SUCCESS;

done:
    printf("\n=====================================================\n");
    printf("  Benchmark %s\n", rc == EXIT_SUCCESS ? "complete." : "FAILED.");
    printf("=====================================================\n");

    ioctl(fd, CCA_IOC_RESET);
    close(fd);
    return rc;
}
