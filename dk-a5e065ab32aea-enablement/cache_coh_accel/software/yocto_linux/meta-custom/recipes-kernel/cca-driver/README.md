# Custom Kernel Driver for Cache Coherent Accelerator
This custom kernel driver implements control of the Cache Coherent Accelerator design in Linux by exposing the interfaces to the FPGA fabric via a misc char driver. These interfaces are interacted with the user via IOCTL commands.

By using this driver as a reference, users will learn how to develop a custom out-of-tree kernel driver for FPGA designs.

This driver covers:
- Probing/Initialization of driver
- How to interact with hardware through the driver
- Cleanup and termination of driver


##  cca-driver recipe contents

- cca.c - The main kernel module source code
- cca-dma-helper.c - Helper functions utilized in cca.c to setup DMA buffers and mSGDMA descriptors
- cca-uapi.h - User-included header to expose useful defines and structs to the userspace
- cca.h - Header to expose useful defines and cross-file (`cca.c` and `cca-dma-helper.c`) functions
- COPYING - License file
- Makefile - Makefile to compile the code
- cca-driver.bb - Yocto recipe to build the module

### Directory Layout
```
meta-custom/
    |--recipes-kernel/
        |--linux/
            |--cca-driver/  <---- Root directory of cca-driver recipe.
                |    |--cca-dma-helper.c
                |    |--cca-uapi.h
                |    |--cca.c
                |    |--cca.h
                |    |--Makefile
                |    |--COPYING
                |--cca-driver.bb
```
