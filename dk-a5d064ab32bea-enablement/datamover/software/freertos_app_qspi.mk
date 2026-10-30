# FreeRTOS application QSPI build (datamover-app)
# Required inputs: REVISION, INSTALL_ROOT_BINARIES
# Entry targets: clean-sw, build-st-sw, build-mt-sw, build-sw, install-sw

THIS_MK_ABSPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
THIS_MK_DIR := $(dir $(THIS_MK_ABSPATH))

# Enable pipefail for all commands
SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

# Enable second expansion
.SECONDEXPANSION:

# Clear all built in suffixes
.SUFFIXES:

ifeq ($(strip $(REVISION)),)
$(error ERROR: REVISION was not set. Pass REVISION=<revision> on the command line)
endif

ifeq ($(strip $(INSTALL_ROOT_BINARIES)),)
$(error ERROR: INSTALL_ROOT_BINARIES was not defined before freertos_app_qspi.mk was parsed)
endif

PROJECT_FOLDER = $(realpath .)
FREERTOS_TOP_FOLDER = $(PROJECT_FOLDER)/software/freertos
DATAMOVER_FOLDER = $(FREERTOS_TOP_FOLDER)/datamover-app

# build folders
ST_BUILD_FOLDER = $(DATAMOVER_FOLDER)/single-threaded/build
MT_BUILD_FOLDER = $(DATAMOVER_FOLDER)/multi-threaded/build

# JIC files and build sentinels
ST_JIC = $(ST_BUILD_FOLDER)/fpga_binaries/qspi_image.jic
MT_JIC = $(MT_BUILD_FOLDER)/fpga_binaries/qspi_image.jic
ST_BUILD_DONE = $(ST_BUILD_FOLDER)/fpga_binaries/build.done
MT_BUILD_DONE = $(MT_BUILD_FOLDER)/fpga_binaries/build.done

.PHONY: clean-sw
clean-sw:
	rm -rf $(ST_BUILD_FOLDER)
	rm -rf $(MT_BUILD_FOLDER)

.PHONY: build-st-sw
build-st-sw: $(ST_BUILD_DONE)

$(ST_BUILD_DONE): output_files/$(REVISION).sof
	$(DATAMOVER_FOLDER)/build.sh single-threaded
	touch $(ST_BUILD_DONE)

.PHONY: build-mt-sw
build-mt-sw: $(MT_BUILD_DONE)

$(MT_BUILD_DONE): output_files/$(REVISION).sof
	$(DATAMOVER_FOLDER)/build.sh multi-threaded
	touch $(MT_BUILD_DONE)

.PHONY: build-sw
build-sw: $(ST_BUILD_DONE) $(MT_BUILD_DONE)

.PHONY: install-sw
install-sw: \
	$(INSTALL_ROOT_BINARIES)/freertos/datamover-app/single-threaded/qspi_image.jic \
	$(INSTALL_ROOT_BINARIES)/freertos/datamover-app/multi-threaded/qspi_image.jic

$(INSTALL_ROOT_BINARIES)/freertos/datamover-app/single-threaded/qspi_image.jic: $(ST_BUILD_DONE)
	mkdir -p $(dir $@)
	cp -f $(ST_JIC) $@

$(INSTALL_ROOT_BINARIES)/freertos/datamover-app/multi-threaded/qspi_image.jic: $(MT_BUILD_DONE)
	mkdir -p $(dir $@)
	cp -f $(MT_JIC) $@
