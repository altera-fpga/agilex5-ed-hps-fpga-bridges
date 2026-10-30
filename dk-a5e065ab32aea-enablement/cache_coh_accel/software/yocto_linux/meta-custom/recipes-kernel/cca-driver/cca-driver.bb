SUMMARY = "Cache Coherent Accelerator kernel driver"
DESCRIPTION = "Out-of-tree kernel module for the CCA FPGA subsystem. \
Maps CSR and DMA register regions declared in cache_coh_accel.dts."

LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

SRC_URI = "file://COPYING \
           file://Makefile \
           file://cca.c \
           file://cca-dma-helper.c \
           file://cca.h \
           file://cca-uapi.h \
          "

S = "${UNPACKDIR}"

RPROVIDES:${PN} += "kernel-module-cca-driver"
