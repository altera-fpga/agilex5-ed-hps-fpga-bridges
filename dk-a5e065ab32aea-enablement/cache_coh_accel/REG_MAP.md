
## FPGA2HPS msgDMA Subsystem — Nios V Simulation Address Map

|-------------------------------|------------------|------------------|------------------|-------|---------|-------------------------------------------|
| Component                     | Subsystem Offset | Nios V Address   | End Address      |  Size | Access  | Description                               |
|-------------------------------|------------------|------------------|------------------|-------|---------|-------------------------------------------|
| Descriptor OCM (s1)           |   0x0020_0000    |    0x3020_0000   |    0x3020_3FFF   | 16 KB |   R/W   | DMA descriptor memory (Nios V config path)|
| msgDMA Write Prefetcher CSR   |   0x0020_4000    |    0x3020_4000   |    0x3020_401F   | 32 B  |   R/W   | Write DMA prefetcher CSR                  |
| msgDMA Read Prefetcher CSR    |   0x0020_4020    |    0x3020_4020   |    0x3020_403F   | 32 B  |   R/W   | Read DMA prefetcher CSR                   |
| msgDMA Write Dispatcher CSR   |   0x0020_4040    |    0x3020_4040   |    0x3020_405F   | 32 B  |   R/W   | Write DMA dispatcher control/status       |
| msgDMA Read Dispatcher CSR    |   0x0020_4060    |    0x3020_4060   |    0x3020_407F   | 32 B  |   R/W   | Read DMA dispatcher control/status        |
| Accelerator CSR               |   0x0040_0000    |    0x3040_0000   |    0x3040_00FF   | 256 B |   R/W   | Accelerator control/status registers      |
| Accelerator RAM               |        -         |    0x2000_0000   |    0x2000_FFFF   | 64 KB |   R/W   | AXI4-attached data buffer (Nios V view)   |
|-------------------------------|------------------|------------------|------------------|-------|---------|-------------------------------------------|
| Note: Descriptor OCM s2 port is internally accessed by the msgDMA engines via MSGDMA view address 0x2020_0000.                                       |
| Note: Accelerator RAM is accessed by msgDMA data masters at MSGDMA view address 0x0005_0000.                                                         |
|-------------------------------|------------------|------------------|------------------|-------|---------|-------------------------------------------|

### Software Base Address Definitions

```c
#define MSGDMA_WR_BASE           0x30204000  // Write Prefetcher CSR
#define MSGDMA_WR_DISPATCHER_BASE 0x30204040 // Write Dispatcher CSR
#define MSGDMA_RD_BASE           0x30204020  // Read  Prefetcher CSR
#define MSGDMA_RD_DISPATCHER_BASE 0x30204060 // Read  Dispatcher CSR
#define CCA_BASE                 0x30400000  // Cache Coherent Accelerator CSR
#define OCM_BASE                 0x30200000  // Descriptor OCM (Nios V view)
#define OCM_BASE_MSGDMA          0x20200000  // Descriptor OCM (MSGDMA descriptor master view)
#define RAM_BASE                 0x20000000  // Accelerator RAM (Nios V view)
#define RAM_BASE_MSGDMA          0x00050000  // Accelerator RAM (MSGDMA data master view)
#define DMA_READ_BUF             0x20000000  // DMA read source buffer  (Nios V view, lower 2KB)
#define DMA_WRITE_BUF            0x20000800  // DMA write dest buffer   (Nios V view, upper 2KB)
#define DMA_READ_BUF_MSGDMA      0x00050000  // DMA read source buffer  (MSGDMA view)
#define DMA_WRITE_BUF_MSGDMA     0x00050800  // DMA write dest buffer   (MSGDMA view, 2KB offset)
```
---

msgDMA READ Descriptor Chain (MM2ST: RAM → Accelerator AvST Sink)
OCM base (Nios V view): 0x30200000 | OCM base (MSGDMA view): 0x20200000

|------|------------|------------|------------|------------|------------|------------|
| Desc | OCM Offset | src_addr   | dest_addr  | length     | next_desc  | control    |
|      | Nios V     | (MSGDMA)   | (stream=0) | (bytes)    | (MSGDMA)   |            |
|------|------------|------------|------------|------------|------------|------------|
|  0   | 0x30200000 | 0x00050000 | 0x00000000 | 0x00000100 | 0x20200040 | 0x41000000 |
|  1   | 0x30200040 | 0x00050100 | 0x00000000 | 0x00000100 | 0x20200080 | 0x41000000 |
|  2   | 0x30200080 | 0x00050200 | 0x00000000 | 0x00000100 | 0x202000C0 | 0x41000000 |
|  3   | 0x302000C0 | 0x00050300 | 0x00000000 | 0x00000100 | 0x20200100 | 0x41000000 |
|  4   | 0x30200100 | 0x00050400 | 0x00000000 | 0x00000100 | 0x20200140 | 0x41000000 |
|  5   | 0x30200140 | 0x00050500 | 0x00000000 | 0x00000100 | 0x20200180 | 0x41000000 |
|  6   | 0x30200180 | 0x00050600 | 0x00000000 | 0x00000100 | 0x202001C0 | 0x41000000 |
|  7   | 0x302001C0 | 0x00050700 | 0x00000000 | 0x00000100 | 0x00000000 | 0x02004000 |
|------|------------|------------|------------|------------|------------|------------|

0x41000000 = OWNED_BY_HW    (0x40000000) | EARLY_DONE (0x01000000) — mid-chain
0x02004000 = WAIT_FOR_WRITE (0x02000000) | IRQ_ON_COMPLETION (0x00004000) — last, SW-owned
Prefetcher NEXT_DESC_PTR_LO = 0x20200000

|------|------------|------------|------------|------------|------------|------------|

READ Desc 0:  src=0x00050000 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x20200040 ✅  ctrl=0x41000000 ✅
READ Desc 1:  src=0x00050100 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x20200080 ✅  ctrl=0x41000000 ✅
READ Desc 2:  src=0x00050200 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x202000c0 ✅  ctrl=0x41000000 ✅
READ Desc 3:  src=0x00050300 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x20200100 ✅  ctrl=0x41000000 ✅
READ Desc 4:  src=0x00050400 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x20200140 ✅  ctrl=0x41000000 ✅
READ Desc 5:  src=0x00050500 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x20200180 ✅  ctrl=0x41000000 ✅
READ Desc 6:  src=0x00050600 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x202001c0 ✅  ctrl=0x41000000 ✅
READ Desc 7:  src=0x00050700 ✅  dest=0x00000000 ✅  len=0x100 ✅  next=0x00000000 ✅  ctrl=0x02004000 ✅

|------|------------|------------|------------|------------|------------|------------|
|------|------------|------------|------------|------------|------------|------------|
|------|------------|------------|------------|------------|------------|------------|

msgDMA WRITE Descriptor Chain (ST2MM: Accelerator AvST Source → RAM)
OCM base (Nios V view): 0x30200200 | OCM base (MSGDMA view): 0x20200200

|------|------------|------------|------------|------------|------------|------------|
| Desc | OCM Offset | src_addr   | dest_addr  | length     | next_desc  | control    |
|      | Nios V     | (stream=0) | (MSGDMA)   | (bytes)    | (MSGDMA)   |            |
|------|------------|------------|------------|------------|------------|------------|
|  0   | 0x30200200 | 0x00000000 | 0x00050800 | 0x00000100 | 0x20200240 | 0x41000000 |
|  1   | 0x30200240 | 0x00000000 | 0x00050900 | 0x00000100 | 0x20200280 | 0x41000000 |
|  2   | 0x30200280 | 0x00000000 | 0x00050A00 | 0x00000100 | 0x202002C0 | 0x41000000 |
|  3   | 0x302002C0 | 0x00000000 | 0x00050B00 | 0x00000100 | 0x20200300 | 0x41000000 |
|  4   | 0x30200300 | 0x00000000 | 0x00050C00 | 0x00000100 | 0x20200340 | 0x41000000 |
|  5   | 0x30200340 | 0x00000000 | 0x00050D00 | 0x00000100 | 0x20200380 | 0x41000000 |
|  6   | 0x30200380 | 0x00000000 | 0x00050E00 | 0x00000100 | 0x202003C0 | 0x41000000 |
|  7   | 0x302003C0 | 0x00000000 | 0x00050F00 | 0x00000100 | 0x00000000 | 0x02004000 |
|------|------------|------------|------------|------------|------------|------------|

Prefetcher NEXT_DESC_PTR_LO = 0x20200200

|------|------------|------------|------------|------------|------------|------------|

WRITE Desc 0: src=0x00000000 ✅  dest=0x00050800 ✅  len=0x100 ✅  next=0x20200240 ✅  ctrl=0x41000000 ✅
WRITE Desc 1: src=0x00000000 ✅  dest=0x00050900 ✅  len=0x100 ✅  next=0x20200280 ✅  ctrl=0x41000000 ✅
WRITE Desc 2: src=0x00000000 ✅  dest=0x00050a00 ✅  len=0x100 ✅  next=0x202002c0 ✅  ctrl=0x41000000 ✅
WRITE Desc 3: src=0x00000000 ✅  dest=0x00050b00 ✅  len=0x100 ✅  next=0x20200300 ✅  ctrl=0x41000000 ✅
WRITE Desc 4: src=0x00000000 ✅  dest=0x00050c00 ✅  len=0x100 ✅  next=0x20200340 ✅  ctrl=0x41000000 ✅
WRITE Desc 5: src=0x00000000 ✅  dest=0x00050d00 ✅  len=0x100 ✅  next=0x20200380 ✅  ctrl=0x41000000 ✅
WRITE Desc 6: src=0x00000000 ✅  dest=0x00050e00 ✅  len=0x100 ✅  next=0x202003c0 ✅  ctrl=0x41000000 ✅
WRITE Desc 7: src=0x00000000 ✅  dest=0x00050f00 ✅  len=0x100 ✅  next=0x00000000 ✅  ctrl=0x02004000 ✅
---
#### **Prefetcher CSR Block** (32 bytes)

|-----------|--------------------|-------|--------|-------|------------------------------------|
| Offset    | Register Name      | Width | Access | Reset | Description                        |
|-----------|--------------------|-------|--------|-------|------------------------------------|
| 0x00      | **CONTROL**        | 32    | R/W    | 0x00  | Prefetcher control register        |
|           |                    |       |        |       |   • Bit 3: IRQ_EN                  |
|           |                    |       |        |       |   • Bit 2: RESET                   |
|           |                    |       |        |       |   • Bit 1: RESERVED                |
|           |                    |       |        |       |   • Bit 0: RUN (self-clearing)     |
| 0x04      | **NEXT_DESC_LOW**  | 32    | R/W    | 0x00  | Next descriptor pointer [31:0]     |
| 0x08      | **NEXT_DESC_HI**   | 32    | R/W    | 0x00  | Next descriptor pointer [63:32]    |
| 0x0C      | **RESERVED**       | 32    |   -    |   -   | Reserved                           |
| 0x10      | **STATUS**         | 32    | R/W1C  | 0x00  | Status register (write-1-to-clear) |
|           |                    |       |        |       |   • Bit 0: IRQ (transfer done)     |
| 0x14      | **DESC_POLL_FREQ** | 32    | R/W    | 0x00  | Descriptor polling frequency       |
| 0x18-0x1C | **RESERVED**       |  -    |   -    |   -   | Reserved                           |
|-----------|--------------------|-------|--------|-------|------------------------------------|

#### **Dispatcher CSR Block** (32 bytes)

|-----------|-------------------------|-------|--------|-------|-------------------------------|
|   Offset  | Register Name           | Width | Access | Reset | Description                   |
|-----------|-------------------------|-------|--------|-------|-------------------------------|
| 0x00      | **STATUS**              |  32   | RO     | 0x02  | Dispatcher status register    |
|           |                         |       |        |       |   • Bit 6: RESET_IN_PROGRESS  |
|           |                         |       |        |       |   • Bit 5: STOPPED            |
|           |                         |       |        |       |   • Bit 4: RESETTING          |
|           |                         |       |        |       |   • Bit 3: STOPPED_ON_ERROR   |
|           |                         |       |        |       |   • Bit 2: RESP_BUF_FULL      |
|           |                         |       |        |       |   • Bit 1: STOPPED (idle)     |
|           |                         |       |        |       |   • Bit 0: BUSY               |
| 0x04      | **CONTROL**             |  32   | R/W    | 0x00  | Dispatcher control register   |
|           |                         |       |        |       |   • Bit 4: STOP_ON_EARLY_TERM |
|           |                         |       |        |       |   • Bit 3: STOP_ON_ERROR      |
|           |                         |       |        |       |   • Bit 2: STOP_DESCRIPTORS   |
|           |                         |       |        |       |   • Bit 1: RESET              |
|           |                         |       |        |       |   • Bit 0: STOP               |
| 0x08      | **FILL_LEVEL**          |  32   | RO     | 0x00  | Descriptor buffer fill level  |
| 0x0C      | **RESPONSE_FILL_LEVEL** |  32   | RO     | 0x00  | Response buffer fill level    |
| 0x10      | **SEQUENCE_NUMBER**     |  32   | RO     | 0x00  | Current sequence number       |
| 0x14-0x1C | **RESERVED**            |   -   | -      |  -    | Reserved                      |
|-----------|-------------------------|-------|--------|-------|-------------------------------|

Note: Dispatcher STATUS reset value = 0x02 (STOPPED=1, BUSY=0) is the normal idle state.
      Software should not modify dispatcher registers — the dispatcher is auto-managed by the prefetcher.

---
---

## Accelerator CSR Block (CCA_BASE = 0x3040_0000)

### FPGA Identification Registers

|-------|--------------------------------|--------|------------|------------------------------------------------------------------|
| Offset| Register Name                  | Access | Default    | Description                                                      |
|-------|--------------------------------|--------|------------|------------------------------------------------------------------|
| 0x00  | FPGA_ID                        | RO     | 0x01010003 | FPGA design identifier                                           |
|       |   • Bit 31..24: MAJOR_ID       |        | 0x01       | Major design ID                                                  |
|       |   • Bit 23..16: MINOR_ID       |        | 0x01       | Minor design ID                                                  |
|       |   • Bit 15..8:  RESERVED       |        | 0x00       | Reserved                                                         |
|       |   • Bit  7..0:  VARIANT_ID     |        | 0x03       | Variant / instance ID                                            |
| 0x04  | FPGA_REVISION                  | RO     | 0x01010001 | FPGA revision number                                             |
|       |   • Bit 31..24: MAJOR_REL      |        | 0x01       | Major release number                                             |
|       |   • Bit 23..16: MINOR_REL      |        | 0x01       | Minor release number                                             |
|       |   • Bit 15.. 8: FPGA_REL       |        | 0x00       | FPGA build release                                               |
|       |   • Bit  7.. 0: PATCH_REL      |        | 0x01       | Patch / hotfix release                                           |
| 0x08  | RESERVED_08                    | RO     | 0x08080808 | Reserved — returns fixed pattern for verification                |
| 0x0C  | RESERVED_0C                    | RO     | 0x0C0C0C0C | Reserved — returns fixed pattern for verification                |
|-------|--------------------------------|--------|------------|------------------------------------------------------------------|

### Accelerator Control and Status Registers

|--------|------------------------|--------|---------------|------------------------------------------------------|
| Offset | Register Name          | Access | Default       | Description                                          |
|--------|------------------------|--------|---------------|------------------------------------------------------|
| 0x0010 | ACCEL_CONTROL          | R/W    | 0x00008000    | Accelerator control register (reset default=RESET)   |
|        |                        |        |               |   • Bit 15: RESET  (self-clearing, extends 3 cycles) |
|        |                        |        |               |   • Bit 10: ABORT                                    |
|        |                        |        |               |   • Bit  9: STOP                                     |
|        |                        |        |               |   • Bit  8: START  (self-clearing)                   |
|        |                        |        |               |   • Bit  7..0: CMD[7:0]                              |
|        |                        |        |               |       0x00 = CMD_NOP                                 |
|        |                        |        |               |       0x01 = CMD_PROCESS_DATA                        |
|        |                        |        |               |       0x02 = CMD_PASS_THROUGH                        |
|        |                        |        |               |       0xFF = CMD_ERROR_TEST                          |
| 0x0014 | ACCEL_STATUS           | RO     | 0x00000000    | Accelerator status register                          |
|        |                        |        |               |   • Bit 4: ACCEL_ERROR                               |
|        |                        |        |               |   • Bit 3: ACCEL_DONE                                |
|        |                        |        |               |   • Bit 2: ACCEL_BUSY                                |
|        |                        |        |               |   • Bit 1: ACCEL_ALIVE (toggles ~100ms @ 200MHz)     |
|        |                        |        |               |   • Bit 0: ACCEL_ALIVE_FLAG (set once alive seen)    |
| 0x0018 | RESERVED_18            | RO     | 0x18181818    | Reserved — returns fixed pattern for verification    |
| 0x001C | RESERVED_1C            | RO     | 0x1C1C1C1C    | Reserved — returns fixed pattern for verification    |
|--------|------------------------|--------|---------------|------------------------------------------------------|

### Debug and Test Registers

|--------|------------------------|--------|---------------|--------------------------------------|
| Offset | Register Name          | Access | Default       | Description                          |
|--------|------------------------|--------|---------------|--------------------------------------|
| 0x0040 | ACCEL_DBG_TST_REG      | R/W    | 0x00000000    | Debug test register                  |
| 0x0044 | ACCEL_DBG_0_REG_DATA   | R/W    | 0x00000000    | Debug data register 0                |
| 0x0048 | ACCEL_DBG_1_REG_DATA   | R/W    | 0x00000000    | Debug data register 1                |
| 0x004C | ACCEL_DBG_2_REG_DATA   | R/W    | 0x00000000    | Debug data register 2                |
| 0x0050 | ACCEL_DBG_3_REG_DATA   | R/W    | 0x00000000    | Debug data register 3                |
| 0x0054 | ACCEL_DBG_4_REG_DATA   | R/W    | 0x00000000    | Debug data register 4                |
| 0x0058 | ACCEL_DBG_5_REG_DATA   | R/W    | 0x00000000    | Debug data register 5                |
| 0x005C | ACCEL_DBG_6_REG_DATA   | R/W    | 0x00000000    | Debug data register 6                |
| 0x0060 | ACCEL_DBG_7_REG_DATA   | R/W    | 0x00000000    | Debug data register 7                |
| 0x0064 | ACCEL_DBG_8_REG_DATA   | R/W    | 0x00000000    | Debug data register 8                |
| 0x0068 | ACCEL_DBG_9_REG_DATA   | R/W    | 0x00000000    | Debug data register 9                |
| 0x006C | ACCEL_DBG_10_REG_DATA  | R/W    | 0x00000000    | Debug data register 10               |
|--------|------------------------|--------|---------------|--------------------------------------|

Note: All Accelerator CSR registers accessed via Nios V at base address 0x30400000.
      Full address = 0x30400000 + offset.
      ACCEL_DBG_10_REG_DATA (0x006C) is used as a test breadcrumb register in firmware.

