# CCA Linux Throughput Benchmark

Measures the maximum data transfer speed of the Cache Coherent Accelerator
(CCA) design on Linux.

## What it measures

Each benchmark run times the **SUBMIT to WAIT wall-clock** for a
fixed `(n_jobs, job_size)` configuration, then converts latency to output
throughput:

```
throughput = n_jobs × job_size x 2  /  elapsed_time
```

Both the **output bytes** (WR DMA to DDR) and **input bytes** (RD DMA from DDR) are counted to measure the bandwidth of the memory interface

Results are average over 50 iterations

## Benchmarking parameters

| Benchmark | n_jobs | job_size |
|-------|-------|--------|
| 1 | 1 to 32 | 32 KB |
| 2 | 8 | 4 to 96 KB |

Benchmark 1 shows the accelerator performance as descriptor chains grow. Benchmark 2 shows the accelerator performance as the size of each job in a chain grows.

## Benchmark methodology

- **Warmup:** 5 iterations per configuration (TLB and cache priming).
- **Measured:** 50 timed iterations per configuration.
- **Command:** `CCA_CMD_PROCESS_DATA` (output = ~input, full 8-stage pipeline).
- **Throughput basis:** output bytes only (`n_jobs × job_size` per transfer).