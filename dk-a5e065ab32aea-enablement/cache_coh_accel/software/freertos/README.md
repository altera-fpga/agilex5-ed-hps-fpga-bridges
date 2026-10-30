# Overview of FreeRTOS
The FreeRTOS framework enables user to create real-time operating systems (RTOS)
with deterministic behaviours and bounded response time. The Altera FreeRTOS framework
provides software drivers for Agilex 5 HPS peripherals and a straightforward build
process for deploying real-time operating system on the Agilex 5 SoC FPGA.

Each FreeRTOS example design is contained inside a XXX-app folder, and all FreeRTOS
example designs use QSPI boot. Following is the top level view of the FreeRTOS framework.

```
.
├── arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/ (installed by make get-toolchain)
├── cca_user_app/
│   ├── CMakeLists.txt
│   ├── build.sh
│   ├── bench_task.c
│   ├── bench_task.h
│   ├── cca.h
│   ├── cca_driver.c
│   ├── cca_driver.h
│   ├── coherency_test.c
│   ├── coherency_test.h
│   ├── main.c
│   └── README.md
├── qspi_flash_image_agilex5_boot.pfg
├── README.md
└── smmu_max_stream_id.patch

```

## Pre-Requisite
- **QUARTUS ToolKit**
Ensure you have Altera Quartus Prime 26.3 correctly configured in your environment

- **Hardware Design**
Ensure that the SOF file for your hardware design is successfully generated:
`dk-a5e065ab32aea-enablement/cache_coh_accel/output_files/cache_coh_accel.sof`

- **ARM toolchain**
Toolchain used for compiling FreeRTOS program. Install toolchain by running the following command in the cache_coh_accel project folder:

```bash
make get-toolchain
```

# Projects
- **CCA User App**
Follow instructions in ```cca_user_app/README.md``` to build the FreeRTOS CCA user application (includes throughput benchmark and coherency verification tasks)