# FreeRTOS application QSPI build (cca_user_app)
# Required inputs: REVISION, INSTALL_ROOT_BINARIES
# Entry targets: clean-sw, build-sw, install-sw

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
CCA_USER_APP_FOLDER = $(FREERTOS_TOP_FOLDER)/cca_user_app

# build folder
ST_BUILD_FOLDER = $(CCA_USER_APP_FOLDER)/build

# JIC file
ST_JIC = $(ST_BUILD_FOLDER)/fpga_binaries/qspi_image.jic

.PHONY: build-st-sw
build-st-sw: output_files/$(REVISION).sof
	$(CCA_USER_APP_FOLDER)/build.sh

.PHONY: clean-sw
clean-sw:
	rm -rf $(ST_BUILD_FOLDER)

.PHONY: build-sw
build-sw: output_files/$(REVISION).sof
	$(CCA_USER_APP_FOLDER)/build.sh

.PHONY: install-sw
install-sw: build-sw
	mkdir -p $(INSTALL_ROOT_BINARIES)/software/freertos/cca_user_app/
	cp -f $(ST_JIC) $(INSTALL_ROOT_BINARIES)/software/freertos/cca_user_app/
