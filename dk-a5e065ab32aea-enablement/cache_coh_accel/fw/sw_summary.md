# Cache Coherent Accelerator - Software Summary

## Overview

This document describes the software architecture for the Cache Coherent Accelerator (CCA) design, which supports both Nios V simulation testing and HPS FreeRTOS runtime environments.

## Directory Structure

```
fw/
├── app/
│   ├── main.c                      # Nios V testbench (simulation)
│   ├── cca_hal.h                   # Unified Hardware Abstraction Layer
│   ├── cache_coh_accel_tasks.h     # FreeRTOS task declarations
│   ├── f2h_task.c                  # FPGA-to-HPS DMA task
│   ├── lwh2f_task.c                # Lightweight HPS-to-FPGA task
│   ├── final_main.c                # ML-DSA example (reference only)
│   ├── free_main.c                 # Archive/backup testbench
│   ├── fabric_dmac/
│   │   ├── fabric_dmac.h           # MSGDMA HAL header
│   │   └── fabric_dmac.c           # MSGDMA HAL implementation
│   └── CMakeLists.txt
└── bsp/
    └── settings.bsp.orig
```

## Environment Support

### Nios V Simulation Environment

**Primary File:** `main.c`

**Purpose:** RTL simulation testbench for validating CCA hardware functionality

**Features:**
- CCA CSR register read/write tests
- Debug register loopback/swap tests
- OCM descriptor memory access tests
- MSGDMA register tests (optional)

**Build System:**
- CMake-based Nios V build flow
- BSP generated from `settings.bsp.orig`
- Output: `nios_mem.hex` for simulation

**Build Command:**
```bash
make nios_mem.hex
```

**Memory Layout:**
- Program size: ~117 KB (code + initialized data)
- Free memory: ~132 KB (stack + heap)

### HPS FreeRTOS Environment

**Primary Files:** `f2h_task.c`, `lwh2f_task.c`, `fabric_dmac/`

**Purpose:** Production runtime code for HPS ARM core

**Features:**
- FPGA-to-HPS DMA transfers (F2H bridge)
- Lightweight HPS-to-FPGA bridge operations (LWH2F)
- Bridge enable/disable control
- MSGDMA descriptor-based transfers
- Multi-block data movement
- Semaphore-based synchronization
- Interrupt-driven DMA completion

**MSGDMA HAL (`fabric_dmac/`):**
- Descriptor structure definitions
- Prefetcher/Dispatcher control
- F2H and F2SDRAM bridge support
- Read and Write operation modes

## Unified Hardware Abstraction Layer (cca_hal.h)

### Purpose

Provides a common API for register access across both Nios V and HPS FreeRTOS environments, enabling code portability and consistency.

### Key Features

**1. Common Data Structures**
```c
typedef struct msgdma_descriptor {
    uint32_t src_addr;
    uint32_t dest_addr;
    uint32_t length;
    uint32_t next_desc_ptr;
    uint32_t reserved[3];
    uint32_t control;
} msgdma_descriptor_t;
```

**2. Control Bit Definitions**
- `DESC_CTRL_WAIT_FOR_WRITE_MSK` (0x02000000)
- `DESC_CTRL_EARLY_DONE_MSK` (0x01000000)
- `DESC_CTRL_IRQ_ON_COMPLETION_MSK` (0x00004000)
- `DESC_CTRL_OWNED_BY_HARDWARE_MSK` (0x40000000)
- `PREFETCHER_RESET_MSK`, `PREFETCHER_RUN_MSK`, `PREFETCHER_INTERRUPT_EN_MSK`
- `DISPATCHER_RESET_MSK`, `DISPATCHER_STOP_ON_ERR_MSK`, `DISPATCHER_RESET_STATUS_MSK`

**3. Environment-Aware Register Access**

```c
// Nios V: Uses HAL IO macros
#define REG_WRITE32(base, offset, value) IOWR_32DIRECT(base, offset, value)
#define REG_READ32(base, offset) IORD_32DIRECT(base, offset)

// HPS FreeRTOS: Uses volatile pointer access
#define REG_WRITE32(addr, offset, value) \
    do { \
        volatile uint32_t *reg = (volatile uint32_t *)((addr) + (offset)); \
        *reg = (value); \
    } while (0)
```

**4. Unified API Functions**

```c
static inline void cca_reg_write(uint32_t base, uint32_t offset, uint32_t value)
static inline uint32_t cca_reg_read(uint32_t base, uint32_t offset)
```

### Usage

**For Nios V (main.c):**
```c
#define __NIOS_V__
#include "cca_hal.h"
```

**For HPS FreeRTOS:**
```c
#include "cca_hal.h"  // No __NIOS_V__ define
```

## API Compatibility Matrix

| Component | Nios V (Simulation) | HPS FreeRTOS (Hardware) |
|-----------|---------------------|-------------------------|
| **Header** | `cca_hal.h` with `__NIOS_V__` | `cca_hal.h` without `__NIOS_V__` |
| **Descriptor Structure** | `msgdma_descriptor_t` | `msgdma_descriptor_t` |
| **Register Access** | `cca_reg_write/read()` via `IOWR_32DIRECT/IORD_32DIRECT` | `cca_reg_write/read()` via volatile pointer |
| **Control Masks** | `DESC_CTRL_*`, `PREFETCHER_*`, `DISPATCHER_*` | Same |
| **Logging** | `printf()` | `PRINT()` macro |
| **Base Addresses** | Direct FPGA addresses | LWH2F bridge-mapped addresses |

## File Descriptions

### Active Files

#### main.c
- **Environment:** Nios V simulation
- **Purpose:** Primary testbench for CCA accelerator validation
- **Base Addresses:**
  - `OCM_BASE`: 0x00200000 (Descriptor OCM)
  - `MSGDMA_WR_BASE`: 0x00204040 (Write CSR)
  - `MSGDMA_RD_BASE`: 0x00204060 (Read CSR)
  - `CCA_BASE`: 0x00400000 (Accelerator CSR)
- **Tests Implemented:**
  - FPGA ID/Revision register reads
  - Debug register write/read verification
  - Debug register loopback/swap test
  - OCM descriptor memory test
- **API:** Uses `cca_hal.h` unified API

#### cca_hal.h
- **Purpose:** Unified Hardware Abstraction Layer
- **Features:**
  - Environment detection (`__NIOS_V__`)
  - Common data structures
  - Portable register access macros
  - Control bit definitions
- **Compatibility:** Works in both Nios V and HPS FreeRTOS

#### f2h_task.c
- **Environment:** HPS FreeRTOS
- **Purpose:** FPGA-to-HPS DMA transfer task
- **Bridge:** F2H bridge
- **Features:**
  - Bridge enable/disable control
  - Multi-block descriptor-based DMA
  - Cache coherency management
  - Interrupt-driven completion

#### lwh2f_task.c
- **Environment:** HPS FreeRTOS
- **Purpose:** Lightweight HPS-to-FPGA bridge operations
- **Bridge:** LWH2F bridge
- **Base Address:** 0x20000000 (LWH2F_BASE_ADDR)
- **Features:**
  - Accelerator CSR debug register tests
  - DMA operations via `socfpga_dma.h`
  - Bridge control via `socfpga_bridge.h`

#### cache_coh_accel_tasks.h
- **Purpose:** Task interface definitions for FreeRTOS
- **Defines:**
  - `H2F_BASE_ADDR`: 0x40000000
  - `LWH2F_BASE_ADDR`: 0x20000000
  - `NUM_BLOCKS`: 8
  - `TOTAL_BUF_BYTES`: 0x80000 (512KB)
- **Functions:** `h2f_task()`, `lwh2f_task()`, `f2h_task()`, `f2sdram_task()`

#### fabric_dmac/fabric_dmac.h
- **Purpose:** MSGDMA HAL header
- **Defines:**
  - MSGDMA CSR register addresses
  - Descriptor control bits
  - Bridge-specific offsets
  - Prefetcher/Dispatcher control masks
- **Functions:** `configure_fdmac()`

#### fabric_dmac/fabric_dmac.c
- **Purpose:** MSGDMA HAL implementation
- **Functions:**
  - `configure_fdmac()` - Configure read/write DMA operations
  - Supports F2H and F2SDRAM bridges
  - Handles prefetcher and dispatcher reset/configuration

### Reference/Archive Files

#### final_main.c
- **Status:** Reference only (not built)
- **Purpose:** ML-DSA (FIPS 204) signature verification example
- **Content:** Extensive test vectors for ML-DSA-44, ML-DSA-65, ML-DSA-87
- **Use Case:** Example of how to structure large data test cases

#### free_main.c
- **Status:** Archive/backup (not built)
- **Purpose:** Alternative Nios V testbench
- **Differences:** Uses different base address (0x10000000)

## Porting Guide

### From Nios V to HPS FreeRTOS

1. **Remove or undefine `__NIOS_V__`**
   ```c
   // #define __NIOS_V__  // Comment out or remove
   ```

2. **Update base addresses for HPS memory map**
   ```c
   #define CCA_BASE 0x23000000  // LWH2F bridge mapped
   ```

3. **Replace logging functions**
   ```c
   // Nios V:
   printf("Test message\n");
   
   // FreeRTOS:
   PRINT("Test message\n");
   ```

4. **Replace PIO operations**
   ```c
   // Nios V:
   IOWR_ALTERA_AVALON_PIO_DATA(TB_GPO_0_BASE, value);
   
   // FreeRTOS:
   // Use HPS GPIO or bridge control functions
   ```

5. **Add FreeRTOS-specific includes**
   ```c
   #include "osal.h"
   #include "osal_log.h"
   #include <task.h>
   #include "socfpga_bridge.h"
   ```

### From HPS FreeRTOS to Nios V

1. **Add `__NIOS_V__` define**
2. **Update base addresses for simulation memory map**
3. **Replace `PRINT()` with `printf()`**
4. **Remove FreeRTOS-specific code** (tasks, semaphores, etc.)
5. **Add Nios V HAL includes**

## Build Information

### Firmware Build

**Prerequisites:**
- Quartus Prime Pro with Nios V tools
- CMake build system
- BSP from `settings.bsp.orig`

**Build Steps:**
```bash
# From cache_coh_accel directory
make nios_mem.hex
```

**Build Process:**
1. Copy `fw/bsp/settings.bsp.orig` to `fw/bsp/settings.bsp`
2. Generate BSP with `niosv-bsp`
3. Configure CMake with `cmake -B fw/app/build fw/app`
4. Build application with `cmake --build fw/app/build`
5. Convert ELF to HEX with `elf2hex`
6. Copy to `nios_mem.hex` for simulation

**Build Output:**
- `fw/app/build/app.elf` - Executable
- `fw/app/build/app.hex` - Memory initialization file
- `nios_mem.hex` - Simulation memory image

### Memory Report

```
* 117.05 KB - Program size (code + initialized data)
* 132.51 KB - Free for stack + heap
```

## Testing

### Nios V Simulation Tests

**Test Suite in main.c:**

1. **CCA CSR Register Reads**
   - FPGA_ID (expected: 0x01010003)
   - FPGA_REV (expected: 0x01010001)
   - RESERVED_08 (expected: 0x08080808)
   - RESERVED_0C (expected: 0x0C0C0C0C)

2. **Debug Register Write/Read**
   - Write unique patterns to DBG_REG0-9
   - Read back and verify each register
   - Test values: DBG_TST_VAL_0 through DBG_TST_VAL_9

3. **Debug Register Loopback/Swap**
   - Read current values from 8 debug registers
   - Rotate values left by 1 position
   - Write swapped values back
   - Read and verify all 8 registers

4. **OCM Descriptor Memory Test**
   - Write 10 test values to OCM_BASE (0x00200000)
   - Read back and verify each location
   - Tests OCM connectivity and data integrity

### Test Status Indicators

```c
#define NIOS_START_BIT (0x1 << 31)  // Bit 31: Test started
#define NIOS_PASS_BIT  (0x1 << 30)  // Bit 30: Test passed
#define NIOS_DONE_BIT  (0x1 << 29)  // Bit 29: Test done
```

Signals written to `TB_GPO_0_BASE` PIO for testbench monitoring.

## Address Maps

### Nios V Simulation (main.c)

| Component | Base Address | Description |
|-----------|--------------|-------------|
| OCM | 0x00200000 | Descriptor on-chip memory |
| MSGDMA Write CSR | 0x00204040 | Write DMA control/status |
| MSGDMA Read CSR | 0x00204060 | Read DMA control/status |
| CCA Accelerator | 0x00400000 | Accelerator CSR registers |

### HPS FreeRTOS (lwh2f_task.c)

| Component | Base Address | Description |
|-----------|--------------|-------------|
| LWH2F Bridge | 0x20000000 | Lightweight HPS-to-FPGA bridge |
| Accelerator CSR | 0x23000000 | LWH2F-mapped accelerator registers |
| OCM_B | 0x22200000 | LWH2F + 0x02200000 offset |

### HPS FreeRTOS (fabric_dmac.h)

| Component | Base Address | Description |
|-----------|--------------|-------------|
| F2H DMA | 0x21000000 | LWH2F + 0x01000000 offset |
| F2SDRAM DMA | 0x20000000 | LWH2F + 0x00000000 offset |

## Register Definitions

### CCA Accelerator Registers

| Offset | Name | Type | Description |
|--------|------|------|-------------|
| 0x0000 | CCA_FPGA_ID | RO | FPGA design identifier |
| 0x0004 | CCA_FPGA_REV | RO | FPGA revision number |
| 0x0008 | CCA_RESERVED_08 | RO | Reserved register |
| 0x000C | CCA_RESERVED_0C | RO | Reserved register |
| 0x0010 | CCA_ACCEL_CONTROL | RW | Accelerator control |
| 0x0014 | CCA_ACCEL_STATUS | RO | Accelerator status |
| 0x0040 | CCA_DBG_TST_REG | RW | Debug test register |
| 0x0044 | CCA_DBG_REG0 | RW | Debug data register 0 |
| 0x0048 | CCA_DBG_REG1 | RW | Debug data register 1 |
| ... | ... | ... | Debug registers 2-10 |

## Nios V vs FreeRTOS Implementation Differences

### Overview

During testbench development and debugging, several critical differences and improvements were identified between the Nios V simulation code (`fw/app/`) and the FreeRTOS HPS production code (`software/freertos/cc_accel_app/`). This section documents these differences to aid future porting efforts and code synchronization.

### File Size Comparison

| Version | File | Line Count |
|---------|------|------------|
| Nios V | `fw/app/cca_f2h_task.c` | 879 lines |
| FreeRTOS | `software/freertos/.../cca_f2h_task.c` | 746 lines |

### Critical Bug Fixes in Nios V Version

#### 1. Descriptor Address Space Translation (FIXED)

**Issue**: CPU and DMA masters have separate address spaces in Nios V testbench, requiring careful address translation.

**FreeRTOS Code (Incorrect for Nios V):**
```c
// CPU writes descriptors using MSGDMA address space (WRONG for Nios V CPU)
volatile msgdma_descriptor_t *desc = 
    (volatile msgdma_descriptor_t *)(uintptr_t)(OCM_BASE_MSGDMA);

// next_desc_ptr also uses MSGDMA address space
desc[i].next_desc_ptr = OCM_BASE_MSGDMA + offset;
```

**Nios V Code (Corrected):**
```c
// CPU writes descriptors to OCM_BASE (Nios V CPU address space: 0x30200000)
volatile msgdma_descriptor_t *desc = 
    (volatile msgdma_descriptor_t *)(uintptr_t)(OCM_BASE);

// But next_desc_ptr uses OCM_BASE_MSGDMA (MSGDMA address space: 0x20200000)
desc[i].next_desc_ptr = OCM_BASE_MSGDMA + offset;
```

**Impact**: 
- FreeRTOS code works on HPS where CPU and DMA share identity-mapped address space
- Nios V testbench requires separate addressing: CPU sees OCM at 0x30200000, MSGDMA sees it at 0x20200000
- This bug would cause descriptor chains to fail in Nios V simulation

**Files Affected**:
- `setup_read_descriptors()` - Line 234, 248, 262
- `setup_write_descriptors()` - Line 304, 318, 333

#### 2. DMA Buffer Base Address Fixes (CRITICAL)

**Issue**: MSGDMA view of accelerator_ram was missing upper address bits.

**Original (Broken):**
```c
#define RAM_BASE_MSGDMA      0x00060000  // WRONG: Missing upper bits
#define DMA_READ_BUF_MSGDMA  0x00060000  // Should be 0x60000000
#define DMA_WRITE_BUF_MSGDMA 0x000A0000  // Should be 0x60040000
```

**Fixed:**
```c
#define RAM_BASE_MSGDMA      0x60000000  // Matches tb.qsys line 2000 mapping
#define DMA_READ_BUF_MSGDMA  0x60000000  // MSGDMA view of read buffer
#define DMA_WRITE_BUF_MSGDMA 0x60040000  // MSGDMA view of write buffer (+256KB)
```

**Impact**: Without this fix, DMA transfers complete but destination buffer reads all zeros because MSGDMA accesses wrong memory region.

### Performance Optimizations in Nios V Version

#### 3. Fast Buffer Verification Strategy

**Problem**: For large buffers (128 KB = 32,768 words), verifying every location during initialization is extremely slow in simulation.

**FreeRTOS Implementation (Slow):**
```c
// Always verify ALL words in buffer
uint32_t errors = 0;
for (uint32_t i = 0; i < num_words; i++) {
    if (src[i] != expected) {
        // Report error
        if (++errors >= 10) break;
    }
}
```

**Nios V Optimization (Fast):**
```c
// Quick check: verify only 6 key locations
uint32_t quick_check_indices[6] = {
    0, 1,                          // First 2 words
    num_words/2 - 1, num_words/2, // Middle 2 words
    num_words - 2, num_words - 1  // Last 2 words
};

// Only do full scan if quick check fails
for (int j = 0; j < 6; j++) {
    if (buffer[quick_check_indices[j]] != expected) {
        // Quick check failed, run full verification
        // ... detailed error reporting
    }
}
```

**Performance Impact**:
- **Before**: Check 32,768 locations every initialization
- **After**: Check 6 locations (if pass, done!)
- **Speedup**: ~5,461× faster for typical case
- Full verification still runs if errors detected, preserving diagnostic capability

**Functions Modified**:
- `init_src_buffer()` - Lines 118-154
- `init_dst_buffer()` - Lines 177-214

#### 4. Accurate Performance Measurement

**Problem**: FreeRTOS version includes CPU data verification overhead in transfer timing, inflating measured time.

**FreeRTOS Timing (Inaccurate):**
```c
// Timer wraps ENTIRE test including data verification
READ_TIMER(temp_start);
run_pass_through_test(...);        // DMA transfer happens
// Verify data integrity (CPU reads all 32,768 words)
for (uint32_t i = 0; i < num_words; i++) {
    if (dst_buf[i] != src_buf[i]) {
        // Error checking...
    }
}
READ_TIMER(temp_end);
passthru_elapsed_us = temp_end - temp_start;  // Includes verification!
```

**Nios V Timing (Accurate):**
```c
// Timing moved INSIDE run_pass_through_test()
// Measures ONLY DMA submit + wait, excludes verification

uint64_t run_pass_through_test(..., uint64_t *transfer_time_out) {
    // ... setup ...
    
    READ_TIMER(start_time);              // Start timer
    cca_transfer_submit();               // Submit DMA request
    cca_transfer_wait(10000, ...);       // Wait for completion
    READ_TIMER(end_time);                // Stop timer
    
    *transfer_time_out = end_time - start_time;  // Pure DMA time
    
    // Data verification happens AFTER timing, in caller
}
```

**Results**:
- Measures actual DMA transfer performance: **5.25 MB/s** (simulation)
- Excludes CPU verification overhead (reading 32,768 words over bus)
- Provides accurate bandwidth measurement for hardware characterization
- CPU timer @ 50 MHz accurately measures wait time for external bus operations @ 25 MHz

**Files Modified**:
- `run_pass_through_test()` - Lines 597-653
- Timer function return value added (line 612)

#### 5. Polling Optimization

**Change**: Removed `usleep(100000)` delay in completion polling loop.

**Before:**
```c
while (iterations < max_iters) {
    // Check status
    if (complete) break;
    usleep(100000);  // 100ms delay between checks
    iterations++;
}
```

**After:**
```c
while (iterations < max_iters) {
    // Check status
    if (complete) break;
    // usleep(100000);  // Commented out for fastest polling
    iterations++;
}
```

**Impact**: Faster completion detection in simulation environment where usleep overhead is high.

#### 6. Destination Buffer Sentinel Preservation

**Change**: Commented out destination buffer memset before transfer.

**Rationale**: 
- Destination initialized with 0x01010101 sentinel value in `init_dst_buffer()`
- Preserving sentinel allows detection of partial/incomplete transfers
- If DMA fails, destination still has 0x01010101 (not zeros)
- Improves debugging capability

**Code:**
```c
// init_dst_buffer(dst_virt, size);  // Sets 0x01010101 sentinel
// memset((void *)dst_virt, 0, size);  // COMMENTED OUT - preserve sentinel
```

### Memory Allocation Strategy Differences

| Aspect | Nios V Simulation | FreeRTOS HPS |
|--------|-------------------|--------------|
| **Allocation** | Fixed on-chip FPGA RAM | Dynamic DRAM allocation |
| **Method** | Static addresses from `cca_hal.h` | `aligned_alloc(4096, size)` |
| **Source Buffer** | `DMA_READ_BUF = 0x20000000` | Allocated pointer |
| **Dest Buffer** | `DMA_WRITE_BUF = 0x20040000` | Allocated pointer |
| **Address Mapping** | Separate CPU/DMA views | Identity mapping (virt == phys) |
| **Alignment** | Hardware-defined | 4KB page-aligned |
| **Free Operation** | Not applicable (static) | `free()` required |

**Nios V Code:**
```c
static int alloc_dma_buffers(volatile uint32_t **src, volatile uint32_t **dst, uint32_t size)
{
    // Use fixed FPGA RAM addresses (no dynamic allocation)
    *src = (volatile uint32_t *)(uintptr_t)DMA_READ_BUF;
    *dst = (volatile uint32_t *)(uintptr_t)DMA_WRITE_BUF;
    
    PRINT("Source buffer:      virt=0x%08lx msgdma=0x%08lx", 
          (unsigned long)DMA_READ_BUF, (unsigned long)DMA_READ_BUF_MSGDMA);
    PRINT("Destination buffer: virt=0x%08lx msgdma=0x%08lx", 
          (unsigned long)DMA_WRITE_BUF, (unsigned long)DMA_WRITE_BUF_MSGDMA);
    
    return 0;
}
```

**FreeRTOS Code:**
```c
static int alloc_dma_buffers(volatile uint32_t **src, volatile uint32_t **dst, uint32_t size)
{
    // 4KB-aligned allocation (page-aligned for cache coherency)
    *src = aligned_alloc(4096, size);
    if (*src == NULL) {
        ERROR("Failed to allocate source buffer (%u bytes)", size);
        return -1;
    }
    
    *dst = aligned_alloc(4096, size);
    if (*dst == NULL) {
        ERROR("Failed to allocate destination buffer (%u bytes)", size);
        free((void *)*src);
        return -1;
    }
    
    // Physical addresses (identity mapping on Agilex 5 HPS)
    uint32_t src_phys = (uint32_t)(uintptr_t)*src;
    uint32_t dst_phys = (uint32_t)(uintptr_t)*dst;
    
    PRINT("Source buffer:      virt=%p phys=0x%08x", *src, src_phys);
    PRINT("Destination buffer: virt=%p phys=0x%08x", *dst, dst_phys);
    
    return 0;
}
```

### Platform-Specific Code Differences

#### Header Files

| Nios V Version | FreeRTOS Version | Purpose |
|---------------|------------------|---------|
| `#include "cca_hal.h"` | `#include "cc_accel.h"` | Hardware definitions |
| `#include "sys/alt_cache.h"` | `#include "socfpga_cache.h"` | Cache operations |
| `#include "sys/alt_irq.h"` | `#include "socfpga_interrupt.h"` | Interrupt handling |
| - | `#include "osal.h"` | OS abstraction (FreeRTOS only) |
| - | `#include "socfpga_bridge.h"` | HPS bridge control |

#### Memory Barriers

| Platform | Instruction | Usage |
|----------|-------------|-------|
| Nios V (RISC-V) | `__asm__ volatile("fence" ::: "memory")` | CPU write completion |
| FreeRTOS (ARM A55) | `__asm__ volatile("dsb sy" ::: "memory")` | Data synchronization barrier |

#### Synchronization

| Aspect | Nios V | FreeRTOS |
|--------|--------|----------|
| **Method** | Simple volatile flag | OSAL semaphores |
| **Declaration** | `extern volatile bool transfer_complete;` | `extern osal_semaphore_t transfer_complete;` |
| **Wait** | Busy polling | Semaphore wait (blocking) |
| **Signal** | Set flag to true | Semaphore post (from ISR) |

### CCA Accelerator Pipeline Reset Requirement (CRITICAL)

#### Issue Discovery

During testing with 128 KB buffers and multiple iterations, a critical hardware behavior was discovered:

**Problem**: Iteration 0 passed perfectly, but iteration 1 showed stale data in the first 8 words of the destination buffer.

**Symptoms**:
```
Iteration 1 destination buffer:
  dst[0] = 0xA0006000  (expected 0xA1000000) - old data from iter 0, descriptor 3!
  dst[1] = 0xA0006001  (expected 0xA1000001)
  ...
  dst[7] = 0xA0006007  (expected 0xA1000007)
  dst[8] = 0xA1000000  (expected 0xA1000008) - new data, but offset!
  dst[9] = 0xA1000001  (expected 0xA1000009)
```

**Analysis**:
- Value 0xA0006000 = pattern 0xA0 (iter 0) + word index 0x6000 (24,576 decimal)
- Word 24,576 is the **first word of descriptor 3** (descriptors 0-2 = 24,576 words)
- This is stale data from **iteration 0's last descriptor** stuck in CCA pipeline
- READ MSGDMA correctly provided new data (0xA1xxxxxx)
- CCA accelerator retained 8 words (32 bytes = one 256-bit beat) in internal FIFOs

**Root Cause**: The CCA accelerator has internal pipeline buffers (FIFOs) that retain data between transfers. Without explicit reset, data from the previous iteration's final descriptor remains in the pipeline and gets written out at the start of the next iteration.

#### Solution: CCA Pipeline Reset Between Iterations

**Implementation**: Issue `CCA_CTRL_RESET` after each iteration completes:

```c
// After iteration completes
PRINT("DEBUG: Resetting CCA accelerator pipeline");
cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_RESET);
usleep(1); // Allow reset pulse to complete (self-clearing, 3 cycles)
```

**Reset Behavior** (from `cca_hal.h`):
```c
#define CCA_CTRL_RESET (1 << 15)  // Bit 15: RESET (self-clearing, 3-cycle pulse)
```

- Reset is **self-clearing** (write 1, hardware auto-clears after 3 clock cycles)
- Flushes all internal pipeline stages and FIFOs
- Does NOT reset CSR registers (configuration preserved)
- Required between iterations to prevent stale data corruption

**When to Reset**:
1. ✅ **Between iterations** of the same buffer transfer
2. ✅ **Before starting new test** with different data patterns
3. ✅ **After any transfer that uses chained descriptors**
4. ❌ **NOT needed** before first iteration (pipeline starts clean)
5. ❌ **NOT needed** during a single multi-descriptor transfer

**Impact**:
- **Before fix**: Iteration 1+ had 8 words of stale data, then offset pattern
- **After fix**: All iterations have correct data from word 0
- **Performance**: Negligible (~1 µs per reset, <<1% of transfer time)

#### Hardware Architecture Notes

**CCA Pipeline Depth**: 
- The 8-word (32-byte) stale data indicates at least **one full 256-bit beat** retained
- Actual pipeline may be deeper but only final beat(s) observable in this failure mode
- PASS_THROUGH mode: Data flows through minimal buffering
- PROCESS_DATA mode: 8-stage inversion pipeline (may retain more data)

**Why 32 KB Worked But 128 KB Failed**:
- With 32 KB (single descriptor equivalent), no pipeline residue observable
- With 128 KB (4 descriptors), final descriptor's last beat stuck in pipeline
- Larger buffers → more descriptors → higher chance of pipeline state carryover

### Configuration Differences

| Parameter | Nios V | FreeRTOS | Notes |
|-----------|--------|----------|-------|
| `NUM_ITERS` | 5 | 10 | Test iteration count |
| `DMA_BUF_SIZE` | 0x20000 (128 KB) | Varies | Current test size |
| Polling Delay | Disabled (usleep commented) | Active (100ms) | Completion check rate |
| Dst memset | Disabled (commented) | Active | Pre-transfer clear |

### Files Unique to Nios V Version

The following files exist only in `fw/app/` and not in FreeRTOS version:

| File | Purpose |
|------|---------|
| `cca_hal.h` | Unified hardware abstraction layer with all base addresses |
| `cca_api.h` | API definitions for accelerator operations |
| `dma_cfg.c` | DMA configuration utilities |
| `ram_tests.c` | Memory testing utilities for simulation |
| `main.c` | Nios V testbench entry point |

### Porting Recommendations

When synchronizing improvements back to FreeRTOS version:

1. **Address Space Translation**: 
   - ✅ **Keep** FreeRTOS version as-is (identity mapping correct for HPS)
   - ❌ **Do NOT** apply Nios V OCM_BASE/OCM_BASE_MSGDMA split

2. **Fast Buffer Verification**: 
   - ✅ **Port** quick check strategy to FreeRTOS for large buffers
   - Improves initialization time with no functional impact

3. **Accurate Timing Measurement**: 
   - ✅ **Port** timer placement inside transfer function
   - Provides true DMA performance metrics excluding CPU overhead

4. **Configuration**:
   - ⚠️ **Review** NUM_ITERS, polling delay, memset behavior per use case
   - Consider keeping FreeRTOS defaults for production code

5. **Memory Allocation**:
   - ✅ **Keep** FreeRTOS dynamic allocation (required for flexible buffer sizing)
   - ❌ **Do NOT** replace with static addresses

6. **CCA Pipeline Reset** (CRITICAL):
   - ✅ **MUST port** - Reset accelerator between iterations to flush internal FIFOs
   - Without reset: stale data from previous iteration corrupts next transfer
   - Add `cca_reg_write(CCA_BASE, CCA_ACCEL_CONTROL, CCA_CTRL_RESET)` after each iteration
   - Essential for multi-iteration tests and production use

### Testing Status (Nios V Version)

**Configuration**: 128 KB buffers, 4 descriptors (32 KB each), 5 iterations

**Results**:
- ✅ All tests passing (5/5 iterations successful)
- ✅ Data integrity verified (destination contains correct pattern)
- ✅ Performance: **5.25 MB/s** average (simulation)
- ✅ Accelerator counters: 655,360 bytes (128 KB × 5 iterations)
- ✅ Descriptor chains: 4 descriptors per direction, proper linking
- ✅ Transfer completion: MSGDMA reaches IDLE state correctly

**Key Metrics**:
- Min: 5.26 MB/s
- Max: 5.23 MB/s
- Avg: 5.25 MB/s
- Transfer time: ~47.5 ms per 256 KB (read + write)
- Bus width: 256 bits (32 bytes/beat)
- Clock domains: CPU @ 50 MHz, External bus @ 25 MHz
- Transfers per descriptor: 1,024 (32 KB / 32 bytes)
- Total bus transactions: 4,096 per 128 KB buffer

## Best Practices

1. **Always use `cca_hal.h` for register access** - Ensures portability
2. **Define environment with `__NIOS_V__`** - Selects correct implementation
3. **Use `msgdma_descriptor_t`** - Common structure across environments
4. **Follow naming conventions** - `cca_*` for accelerator, `fdmac_*` for DMA
5. **Document base addresses** - Keep address maps up to date
6. **Test in simulation first** - Validate with main.c before HPS testing
7. **CRITICAL: Reset CCA pipeline between iterations** - Issue `CCA_CTRL_RESET` after each transfer to flush internal FIFOs and prevent stale data corruption
8. **Understand clock domains** - CPU runs at 50 MHz, external bus (msgDMA/RAM) at 25 MHz

## Known Issues / Limitations

1. MSGDMA register tests in main.c are commented out (need validation)
2. ~~f2h_task.c appears incomplete in the analyzed portion~~ (Resolved)
3. Different base addresses between Nios V and HPS require careful porting
4. **CCA pipeline retains data** - Must explicitly reset between iterations (see documentation above)
5. **Testbench RAM bottleneck** - 32-bit @ 25 MHz limits simulation throughput to ~5.4 MB/s; production hardware will be 9-37× faster
6. **Clock domain asymmetry** - CPU internal clock (50 MHz) runs 2× faster than external bus (25 MHz)

## Nios V Simulation Performance Analysis

### Clock Architecture

**System Clock Configuration**:
- **Nios V CPU internal clock**: 50 MHz (correct per BSP `ALT_CPU_CPU_FREQ`)
- **RISC-V mtime counter**: 50 MHz (tied to CPU clock)
- **External bus clock**: 25 MHz (2:1 clock divider from CPU)
  - msgDMA read/write controllers
  - Accelerator RAM
  - CCA accelerator pipeline
  - OCM for descriptors

**Impact on Measurements**:
```
CPU timer frequency: 50 MHz ✓ (BSP correct)
External bus frequency: 25 MHz (half CPU clock)
Software timer measures CPU cycles (accurate)
Hardware operates at external bus frequency (slower)
```

### Testbench RAM Bottleneck Analysis

**RAM Configuration**:
- **Width**: 32 bits (4 bytes/cycle)
- **Clock**: 25 MHz (external bus frequency)
- **Type**: Single-port (read OR write, not simultaneous)
- **Instance**: `accelerator_ram` from tb.qsys

**Theoretical Performance**:
```
Peak bandwidth: 4 bytes/cycle × 25 MHz = 100 MB/s (external bus limit)
```

**Actual Transfer Breakdown**:
```
Total data:     256 KB (128 KB read + 128 KB write)
Transfer time:  ~47.5 ms (measured by CPU timer @ 50 MHz)
Throughput:     256 KB ÷ 47.5 ms = 5.39 MB/s
RAM efficiency: 5.39 / 100 = 5.4%
```

**Why So Low?**

The RAM operates at **100% efficiency when active** (1 transaction per cycle, confirmed in waveforms), but the overall system utilization is limited by:

1. **Descriptor Processing Overhead**: 
   - msgDMA prefetcher fetches descriptors from OCM
   - Parses descriptor fields
   - Initiates next burst
   - Gaps between 128-byte bursts while processing

2. **Read-Write Coordination**:
   - Write msgDMA must wait for read data to flow through accelerator
   - Single-port RAM serializes read and write accesses
   - Arbiter switching overhead between channels

3. **Burst Granularity**:
   - 128-byte bursts = 32 cycles of RAM activity @ 25 MHz
   - Gap after each burst for next descriptor fetch/decode

4. **Memory Contention**:
   - Read msgDMA accessing source buffer
   - Write msgDMA accessing destination buffer
   - Both prefetchers reading descriptors from OCM
   - All competing for the same memory controller

**Waveform Observations**:
- RAM transactions show continuous activity during active bursts
- Long idle periods between burst sequences
- Hardware DMA busy signal indicates active transfer window
- Descriptor fetch/decode overhead dominates total transfer time

**Conclusion**: 
The testbench RAM bandwidth (100 MB/s theoretical at 25 MHz external bus) combined with descriptor processing overhead limits throughput to ~5.4 MB/s. The 2:1 clock ratio between CPU (50 MHz) and external bus (25 MHz) is working as designed.

**Production Hardware Expectations**:
- HPS DDR controller: Multi-GB/s bandwidth
- Separate read/write buffers (no contention)
- Much higher clock frequencies (400+ MHz)
- Expected real-world throughput: **50-200 MB/s** (9-37× faster than simulation)

### Measurement Methodology Validation

**Debug Register Strategy**:

Two sets of debug registers provide comprehensive performance instrumentation:

1. **CCA_DBG_REG6** (Hardware DMA Time):
   - Written at `cca_transfer_submit()` start
   - Cleared after `cca_transfer_wait()` completes
   - Measures pure hardware DMA transfer time as seen by CPU
   - Visible in waveforms as `dbg_6_reg_data`
   - **Accurate representation** of CPU wait time

2. **CCA_DBG_REG7** (Software Overhead):
   - Written at iteration start (before buffer init)
   - Cleared at iteration end (after verification)
   - Measures total software time including:
     - Buffer initialization and verification
     - Cache operations (flush/invalidate)
     - Polling loop overhead
     - PRINT statement execution
   - Visible in waveforms as `dbg_7_reg_data`

**Software Timer Validation**:

Software measurements using 50 MHz CPU timer are accurate:
```c
READ_TIMER(start_time);              // CPU timer @ 50 MHz
cca_transfer_submit();               // Start DMA (external bus @ 25 MHz)
cca_transfer_wait(...);              // Poll until complete
READ_TIMER(end_time);                // Stop timer
// Timing measures CPU cycles spent waiting for external bus operations
```

This placement ensures software timer measures the same interval as `dbg_6_reg_data`.

**Best Practice**: 
- Use waveform cursors on debug registers for hardware-level timing
- Software timers measure CPU cycles (50 MHz), accurate for performance analysis
- Understand clock domain differences when analyzing waveforms

## HPS FreeRTOS Performance Results

### Test Configuration

**Hardware Platform**: Intel Agilex 5 E-Series Premium Development Kit (dk-a5e065bb32aes1)  
**Processor**: HPS ARM Cortex-A55 @ 400 MHz  
**DMA Engine**: msgDMA with prefetcher and dispatcher  
**Memory**: HPS DDR4 SDRAM via ACE5-Lite coherent interface  
**Software**: FreeRTOS with cache coherency enabled  

**MSGDMA Configuration**:
- **Max transfer length**: 512 KB
- **Data path FIFO depth**: 2 KB (512 × 32-bit words)
- **Descriptor FIFO depth**: 128 descriptors
- **Practical descriptor size limit**: ~32 KB (16-20× FIFO depth)
- **Burst size**: 128 bytes (optimal for DDR4)

### Performance Summary

#### Buffer Size vs Throughput

| Buffer Size | Descriptors | Descriptor Size | Throughput (Avg) | Polling Time | Status |
|-------------|-------------|----------------|------------------|--------------|--------|
| 2 KB | 1 × 2 KB | 2 KB | 95.68 MB/s | ~50 ms | ✅ Fast complete |
| 128 KB | **4 × 32 KB** | **32 KB** | **426.64 MB/s** | **~199 ms** | ✅ **Optimal** |
| 128 KB | 8 × 16 KB | 16 KB | 415.88 MB/s | ~295 ms | ✅ More overhead |
| 256 KB | 4 × 64 KB | 64 KB | ❌ FAIL | N/A | ❌ Exceeds FIFO limit |
| 256 KB | **8 × 32 KB** | **32 KB** | **411.54 MB/s** | **~624 ms** | ✅ **Works** |

**Key Findings**:

1. **Optimal descriptor size: 32 KB**
   - Provides best throughput with minimal overhead
   - Matches practical FIFO limit (2 KB × 16 = 32 KB safe zone)

2. **Descriptor granularity matters**:
   - 128 KB with 4×32KB descriptors: **426.64 MB/s** (best)
   - 128 KB with 8×16KB descriptors: **415.88 MB/s** (2.5% slower)
   - Smaller descriptors = more prefetcher overhead

3. **FIFO depth limits descriptor size**:
   - 256 KB with 4×64KB descriptors **fails immediately**
   - Error status: 0x0000000d (BUSY + STOPPED_ON_ERR + ERROR)
   - 64 KB exceeds 2KB FIFO × ~16-20 ratio
   - 256 KB with 8×32KB descriptors **works** (within limits)

4. **Polling overhead dominates at large sizes**:
   - 256 KB: ~624 ms polling, but actual transfer likely ~1-2 ms
   - Fast-path optimization helps when hardware completes quickly
   - IRQ implementation would eliminate polling for accurate measurement

#### Detailed Performance Metrics

**128 KB Test (4 × 32KB descriptors):**
```
Min time:   0.604 ms  (433.77 MB/s)
Max time:   0.605 ms  (433.05 MB/s)
Avg time:   0.604 ms  (433.44 MB/s)
Polling:    ~199 ticks @ 1ms = ~0.2 seconds
Fast-path:  Sometimes (rd_done=1, wr_done=0 on first check)
Slow-path:  Usually enters polling, completes within 1st poll
```

**128 KB Test (8 × 16KB descriptors):**
```
Min time:   0.598 ms  (418.32 MB/s)
Max time:   0.605 ms  (413.56 MB/s)
Avg time:   0.601 ms  (415.88 MB/s)
Polling:    ~295 ticks @ 1ms = ~0.3 seconds
Fast-path:  Rarely hit
Slow-path:  More polling overhead due to smaller descriptors
```

**256 KB Test (8 × 32KB descriptors):**
```
Min time:   1.211 ms  (413.01 MB/s)
Max time:   1.218 ms  (410.45 MB/s)
Avg time:   1.215 ms  (411.54 MB/s)
Polling:    ~624 ticks @ 1ms = ~0.6 seconds
Fast-path:  Never (too long for fast completion)
Slow-path:  Always, many poll iterations
```

### Optimization Impact

**Fast-Path Optimization** (immediate dispatcher status check after submit):
- **2 KB buffers**: Fast-path hit occasionally, saves ~50 ms polling
- **128 KB buffers**: Fast-path miss (wr_done=0), enters polling but completes in ~1-2 polls
- **256 KB buffers**: Fast-path never hits, always requires polling

**Polling Configuration**:
- Poll interval: 1 ms (`vTaskDelay(pdMS_TO_TICKS(1))`)
- Print frequency: Every 250 ticks (to minimize console spam)
- Timeout: 5000 ticks (5 seconds)

### Design Recommendations

**Buffer Size Selection**:
- **Small transfers (<16 KB)**: Use single descriptor, expect fast-path hits
- **Medium transfers (16-128 KB)**: Use 4 descriptors with 32 KB chunks (optimal)
- **Large transfers (>128 KB)**: Use 8 descriptors with 32 KB chunks (max safe size)

**Descriptor Sizing Rules**:
- **Target descriptor size**: 32 KB (optimal for 2KB FIFO)
- **Maximum safe size**: ~32 KB (16-20× FIFO depth)
- **Minimum descriptors**: Use fewest descriptors that keep chunk size ≤32 KB
- **Formula**: `NUM_DESCRIPTORS = ceil(BUFFER_SIZE / 32KB)`

**Performance Expectations**:
- **Peak throughput**: ~430 MB/s (with optimal descriptor sizing)
- **Sustained throughput**: ~410-420 MB/s (larger transfers, more polling)
- **Bottleneck**: Polling overhead in current implementation
- **Future improvement**: IRQ-based completion for true hardware performance

### Comparison: Simulation vs Hardware

| Metric | Nios V Simulation | HPS FreeRTOS |
|--------|-------------------|--------------|
| **Processor** | Nios V @ 100 MHz | ARM A55 @ 400 MHz |
| **Memory** | 32-bit RAM @ 25 MHz | DDR4 @ multi-GB/s |
| **Buffer Size** | 8-16 KB | 128-256 KB |
| **Throughput** | ~5.4 MB/s | ~410-430 MB/s |
| **Speedup** | Baseline | **75-80× faster** |
| **Bottleneck** | Testbench RAM BW | Polling overhead |
| **Fast-path** | Common (small buffers) | Rare (large buffers) |

**Hardware Performance**: As expected, production hardware with DDR4 SDRAM and 400 MHz processor achieves 75-80× better throughput than the simulated testbench environment. The msgDMA engine with ACE5-Lite cache coherency delivers consistent 400+ MB/s bandwidth.

## Future Enhancements

1. Complete MSGDMA register test suite in main.c
2. Add comprehensive documentation for FreeRTOS task integration
3. Provide example porting workflow with before/after code
4. Add automated testing framework for simulation validation
5. Document interrupt handling for DMA completion
6. **Consider dual-port RAM** for testbench to reduce contention and improve simulation throughput
7. **Document clock domain architecture** - CPU @ 50 MHz vs external bus @ 25 MHz for porting clarity

## Hardware Bug — Root Cause of msgDMA WRITE Not Completing (2026-06-22)

### Symptom

In the FreeRTOS PASS_THROUGH test on `dk-a5e065ab32aea-enablement`, the
`u_msgdma_write` channel appeared to complete (dispatcher status `0x0000000b` =
IDLE+BUSY+STOPPED_ON_ERR), but the destination buffer contained corrupted data:
words 0–2071 were correct, then from word 2072 onward every value was shifted
forward by **16 words (64 bytes)**. SignalTap confirmed the hardware DMA counters
incremented correctly, so the DMA itself completed — but the data written to DRAM
was wrong.

### Root Cause

A one-line RTL difference in `src/cca_accel_core.sv` (line 243):

**`dk-a5e065ab32aea-enablement` (broken):**
```systemverilog
avst_rd_ready <= (avst_sink_valid && stage_0_enable);  // && ~avst_rd_valid_q);
```

**`dk-a5e065bb32aes1-enablement` (correct):**
```systemverilog
avst_rd_ready <= (avst_sink_valid && stage_0_enable && ~avst_rd_valid_q);
```

The `&& ~avst_rd_valid_q` guard was **commented out** in the ab32aea design.
Without it, the CCA pipeline de-asserts backpressure toward the READ msgDMA while
the previous beat's registered valid (`avst_rd_valid_q`) is still high. This
allows the core to accept a new data beat one cycle too early at descriptor
boundaries, producing a 64-byte (16-word) shift in the WRITE output data.

### Fix

Restore the `~avst_rd_valid_q` term in `src/cca_accel_core.sv`:

```systemverilog
avst_rd_ready <= (avst_sink_valid && stage_0_enable && ~avst_rd_valid_q);
```

This was identified by comparing the two platform designs side-by-side:
all other RTL files (`cca_accel_csr.sv`, `cca_accel_top.sv`,
`fpga2hps_msgdma2axi4.sv`, `cca_hal.h`, `cca_f2h_task.c`) are identical between
the two platforms. The fix was confirmed in FreeRTOS testing on hardware
(2026-06-22).

### Open Question — Throughput Impact of the Fix (TODO 2026-06-23)

Restoring `~avst_rd_valid_q` may **halve theoretical throughput** because:

```
avst_rd_valid_q <= (avst_sink_valid && avst_rd_ready);   // line 245
avst_rd_ready   <= (avst_sink_valid && stage_0_enable && ~avst_rd_valid_q);  // line 243
stage_0_valid   <= avst_rd_valid_q;  // line 306
```

Every accepted beat sets `avst_rd_valid_q=1` the next cycle, which immediately
blocks `avst_rd_ready` for one cycle before it can fall again. In steady-state
streaming this creates an alternating accept/stall pattern = **~50% throughput**.

The bb32aes1 design accepts this penalty (correctness over throughput). The
ab32aea design had it commented out — presumably an intentional attempt to
restore full throughput, but it introduced the data shift bug.

**Tomorrow's task:** analyze whether `~avst_rd_valid_q` can be replaced with a
more targeted guard that only stalls at descriptor boundaries (end-of-packet /
`avst_sink_endofpacket`) rather than every beat. This would restore correctness
without the 50% throughput penalty. Key signals to review in
`src/cca_accel_core.sv`:
- `avst_rd_valid_q` (line 96, 237, 245)
- `stage_0_enable` (line 103, 299)
- `stage_0_valid` / `stage_0_data` (lines 104-105, 303-306)
- `avst_sink_valid`, `avst_sink_endofpacket` (input from WRITE msgDMA side)
- The full pipeline stage chain (stage_0 → stage_1 → … → output)

### Contributing Software Fixes (also applied during debugging)

Two software bugs were also discovered and fixed in the FreeRTOS app
(`software/freertos/cc_accel_app/single_thread/cc_accel_tasks/cca_f2h_task.c`):

1. **STOPPED_ON_ERR not recognized as completion** — the polling loop only
   accepted `IDLE=1 && BUSY=0` as done. Per the Intel msgDMA spec, end-of-chain
   exits with `STOPPED_ON_ERR=1` (status `0x0b`). Fixed by adding
   `|| (disp_stat & DISPATCHER_STOPPED_ON_ERR_MSK)` to both `rd_done` and
   `wr_done` conditions.

2. **Missing prefetcher + dispatcher reset before each iteration** — after the
   first transfer the dispatcher stays in STOPPED_ON_ERR and refuses a new
   descriptor chain until reset. Fixed by adding four reset+poll sequences
   (READ prefetcher, WRITE prefetcher, READ dispatcher, WRITE dispatcher) at the
   top of `cca_transfer_submit()`, ported from the Nios V reference.

## References

- See `main.c` for Nios V testbench implementation
- See `fabric_dmac/` for MSGDMA HAL reference
- See `f2h_task.c` and `lwh2f_task.c` for FreeRTOS examples
- See `cca_hal.h` for unified API details
