DESCRIPTION = "Cache Coherent Accelerator (CCA) Linux userspace demo"
SUMMARY = "Demo application for the Agilex 5 CCA design."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${THISDIR}/../../recipes-kernel/cca-driver/files:"

SRC_URI = "file://cca-demo.c \
           file://cca-uapi.h \
          "

S = "${UNPACKDIR}"

do_compile() {
    ${CC} ${LDFLAGS} ${S}/cca-demo.c -o ${B}/cca-demo
}

do_install() {
    install -d ${D}/home/root/cca
    install -m 0755 ${B}/cca-demo ${D}/home/root/cca/cca-demo
}

FILES:${PN} = "/home/root/cca/cca-demo"

