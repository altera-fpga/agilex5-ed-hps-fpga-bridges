DESCRIPTION = "Cache Coherent Accelerator (CCA) Linux throughput benchmark"
SUMMARY = "Measures maximum transfer throughput of the Agilex 5 CCA design."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/../../recipes-kernel/cca-driver/files:"

SRC_URI = "file://cca-benchmark.c \
           file://cca-uapi.h \
          "

S = "${UNPACKDIR}"

do_compile() {
    ${CC} ${LDFLAGS} ${S}/cca-benchmark.c -o ${B}/cca-benchmark
}

do_install() {
    install -d ${D}/home/root/cca
    install -m 0755 ${B}/cca-benchmark ${D}/home/root/cca/cca-benchmark
}

FILES:${PN} = "/home/root/cca/cca-benchmark"
