# Cache Coherency Accelerator FreeRTOS Software Program

The Cache Coherency Accelerator (CCA) FreeRTOS software program runs on the Agilex 5 HPS and demonstrates hardware cache coherency between the HPS and FPGA fabric via the ACE5-Lite interface.

## Data Flow

Source data is read from HPS DRAM by an mSGDMA READ channel, passed through the CCA accelerator in the FPGA fabric, then written back to HPS DRAM by an mSGDMA WRITE channel. Both DMA channels operate over the F2H coherent bridge. The accelerator supports two modes:

- **PROCESS_DATA** (`0x01`): an eight-stage pipeline applies a bitwise NOT to the data stream.
- **PASS_THROUGH** (`0x02`): data is forwarded unchanged.

## Program Sequence

There are three tasks in this example design: the main cache-coherent accelerator program, an optional CCA throughput benchmark, and an optional cache coherency test. The source code for these tasks is in `main.c`, `bench_task.c`, and `coherency_test.c`, respectively.

### Main program

The FreeRTOS program initiates data transfer across the F2H bridge by configuring read and write mSGDMA IPs in `cca_subsys`. The prefetcher core is enabled in each mSGDMA, using prefetcher linked lists stored in descriptor on-chip memory to automatically fetch the next data block. In this example design, a transfer is an array of jobs. Each job contains one contiguous source buffer, one destination buffer, and a linked list of descriptors.

The main program uses the following steps to showcase the **PROCESS_DATA** mode:

1. Initialize the LWH2F bridge, the accelerator core, both mSGDMA channels, and the completion interrupts.
2. Allocate three jobs of 128 bytes, with each source and destination buffer rounded up to the nearest 4 KB. Destination buffers are zeroed. The upper byte of each source buffer (byte 127) indicates the job number.
3. Build a `cca_job_t` array and submit all jobs with PROCESS_DATA mode enabled. A linked list of descriptors is created for each job, with the last descriptor enabling the completion interrupt.
4. Print each destination buffer and compare every word with the bitwise NOT of the matching source word. Each job prints PASS or FAIL, then the task prints an overall result and frees the buffers.

See `main.c`.

### Accelerator driver setup

`cca_driver.c` programs the CCA core and the two mSGDMA channels. CSR base addresses, as seen from the HPS, are in `cca.h`:

| Block | Address |
|-------|---------|
| LWH2F bridge | `0x20000000` |
| Descriptor on-chip memory | `0x20200000` |
| Write mSGDMA prefetcher | `0x20204000` |
| Read mSGDMA prefetcher | `0x20204020` |
| Write mSGDMA dispatcher | `0x20204040` |
| Read mSGDMA dispatcher | `0x20204060` |
| CCA accelerator CSRs | `0x20400000` |

Read descriptors occupy the start of on-chip memory. Write descriptors start 2048 bytes later, which leaves room for 64 descriptors of 32 bytes on each channel.

### Throughput benchmark

`bench_task.c` measures accelerator throughput with two different sweeps:

- **Benchmark 1** keeps each job at 32 KB and varies the job count: 1, 2, 4, 8, 16, and 32 jobs (32 KB through 1 MB moved per transfer).
- **Benchmark 2** keeps the job count at 8 and varies the job size: 4, 16, 32, 64, and 96 KB (32 KB through 768 KB moved per transfer).

For one configuration:

1. Allocate a 4 KB-aligned source buffer and destination buffer for every job. Fill the source with `0xA5` and zero the destination.
2. Run 5 warmup transfers with PROCESS_DATA enabled. Warmup transfers are not timed.
3. Run 50 transfers and calculate the throughput. Each transfer has a 10 second timeout.
4. Free the buffers and print job count, job size, total KiB, average milliseconds, MB/s, and Mbit/s.

Reported throughput counts both directions: bytes read from the source plus bytes written to the destination. A failed submit or wait stops that configuration early, and the average uses only the iterations that finished.

### Cache coherency test

`coherency_test.c` checks that an F2H DMA read snoops a dirty CPU cache line and that an F2H DMA write updates DRAM and invalidates the CPU cache line.

`config_page_caching()` changes cache attributes on a 2 MB granule. Each buffer is allocated at 2 MB alignment with a 2 MB size so that the granule contains only the test buffer. The transfer itself uses the first 4 KB. Non-cacheable accesses bypass the cache and read or write DRAM directly.

Patterns use the top half-word as an identifier and the bottom half-word as the word index:

| Pattern | Value | Where it is written before the transfer |
|---------|-------|-----------------------------------------|
| PATTERN_3 | `0x3333xxxx` | Source DRAM |
| PATTERN_A | `0xAAAAxxxx` | Source cache (dirty) |
| PATTERN_5 | `0x5555xxxx` | Destination DRAM |
| PATTERN_C | `0xCCCCxxxx` | Destination cache (dirty) |

Setup for each buffer:

1. Flush the buffer, switch the granule to non-cacheable, and write the DRAM pattern (PATTERN_3 for source, PATTERN_5 for destination). A non-cacheable read must return that pattern.
2. Switch back to cacheable and store the cache pattern (PATTERN_A or PATTERN_C) with a data synchronization barrier before every word, so the stores stay in the cache instead of streaming through to DRAM.
3. Switch to non-cacheable again and confirm DRAM still holds the DRAM pattern. A mismatch means the cache write was written through.
4. Switch to cacheable and confirm a normal load returns the cache pattern.

The test then submits one job of 4 KB with `CCA_CMD_PASS_THROUGH` and waits up to 5 seconds. Pass-through copies the bytes the DMA actually reads, so a coherent read returns the dirty cache contents rather than an inverted or stale copy.

After the transfer, every destination word is compared with PATTERN_A:

- **PATTERN_A**: the DMA read snooped the dirty source line, and the DMA write updated DRAM and invalidated the destination line. The test passes.
- **PATTERN_3**: the DMA read stale data from source DRAM. Read coherency failed.
- **PATTERN_C**: the CPU still sees the old destination cache line. Write coherency failed.
- **PATTERN_5**: destination DRAM was not updated. The write did not complete.

## Software Configurability

| Parameter | Default | Where to change |
|-----------|---------|-----------------|
| Benchmark task on/off (`ENABLE_BENCH_TASK`) | 1 | `main.c` |
| Coherency test on/off (`ENABLE_COHERENCY_TEST`) | 0 | `main.c` |
| Demo job count (`DEMO_NUM_JOBS`) | 3 | `main.c` |
| Demo job size (`DEMO_JOB_SIZE`, multiple of 32) | 128 bytes | `main.c` |
| Demo timeout (`DEMO_TIMEOUT_MS`) | 5000 ms | `main.c` |
| Warmup iterations (`BENCH_WARMUP`) | 5 | `bench_task.h` |
| Timed iterations (`BENCH_ITERATIONS`) | 50 | `bench_task.h` |
| Benchmark timeout (`BENCH_TIMEOUT_MS`) | 10000 ms | `bench_task.h` |
| Benchmark job counts and sizes | see sweeps above | `bench_task.c` |

# Build Steps

The expected output of the build process is a JIC file that combines the FPGA bitstream and software binaries that users can program onto the FPGA board with quartus_pgm. The JIC file will contain components laid out in the [QSPI flash PFG file](../qspi_flash_image_agilex5_boot.pfg).

1. Enter the cache_coh_accel folder.

```bash
cd TOP_DIR/dk-a5e065ab32aea-enablement/cache_coh_accel
```

2. Generate the JIC file (cca_user_app.hps.jic).

```bash
make software-freertos_app_qspi-build-sw
```

cca_user_app.hps.jic should appear under the software/freertos/cca_user_app directory.

Note: to build directly without make, run

```bash
./software/freertos/cca_user_app/build.sh
```

3. Program the board with the JIC file.

```bash
quartus_pgm -m jtag -c 1 -o "pvi;cca_user_app.hps.jic@2"
```

(Refer to the [Quartus Programmer Guide](https://docs.altera.com/r/docs/683039/25.3.1/quartus-prime-pro-edition-user-guide-programmer/scripting-support?tocId=hENJN1rgd6fRQEkgsuUfGQ) for more details on quartus_pgm options)

4. Power-cycle the FPGA board.
5. Program output can be viewed through a serial terminal.
