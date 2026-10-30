
# Simulation Makefile for dk-a5e065ab32aea-enablement/cache_coh_accel (SM7+ board)

THIS_MK_ABSPATH := $(abspath $(lastword $(MAKEFILE_LIST)))
THIS_MK_DIR := $(patsubst %/,%,$(dir $(THIS_MK_ABSPATH)))

# Enable pipefail for all commands
SHELL=/bin/bash -o pipefail

# Enable second expansion
.SECONDEXPANSION:

# Clear all built in suffixes
.SUFFIXES:

NOOP :=
SPACE := $(NOOP) $(NOOP)
COMMA := ,
HOSTNAME := $(shell hostname)

##############################################################################
# Environment check
##############################################################################

##############################################################################
# Design Configuration
##############################################################################
# Set the design name. This needs to match the name from the design repo root
DESIGN_NAME := top
REVISION_NAME := top

##############################################################################
# Configuration
##############################################################################

# VCS environment vars
export VCS_ARCH_OVERRIDE=linux
export VCS_LIC_EXPIRE_WARNING=0
export QUARTUS_ROOTDIR_ABS=$(realpath $(QUARTUS_ROOTDIR))
export QUARTUS_DEVICES_ROOTDIR_ABS=$(realpath $(QUARTUS_ROOTDIR)/../devices)

NUM_PROCS=$(shell nproc)

TEST_SEED=0
all: compile_all run_multi
simple: compile_all run_simple

REPO_ROOT_DIR := $(realpath $(THIS_MK_DIR)/..)
VENV_DIR := $(REPO_ROOT_DIR)/venv
VENV_PY := $(VENV_DIR)/bin/python3

DESIGN_ROOT_DIR := $(REPO_ROOT_DIR)
export DESIGN_ROOT_DIR_ABS=$(realpath $(DESIGN_ROOT_DIR))
SRC_DIR := $(DESIGN_ROOT_DIR)/src

RTL_FILES += \
	timescale_top.sv \
	$(SRC_DIR)/cca_accel_core.sv \
	$(SRC_DIR)/cca_accel_csr.sv

TB_FILES = \
	sim_top.sv \
	testbench.sv

FUNDAMENTAL_FILES = \
	$(QUARTUS_ROOTDIR_ABS)/../ip/altera/sopc_builder_ip/verification/lib/verbosity_pkg.sv \

VIP_IFS = \

ifndef CM_NAME
	CM_NAME = test
endif

# SIM_VERBOSITY=VERBOSITY_HIGH
# typedef enum int {VERBOSITY_NONE,
# VERBOSITY_FAILURE,
# VERBOSITY_ERROR,
# VERBOSITY_WARNING,
# VERBOSITY_INFO,
# VERBOSITY_DEBUG} Verbosity_t;

SIM_VERBOSITY=VERBOSITY_DEBUG
ifeq ($(SIM_VERBOSITY),)
	SIM_VERBOSITY=VERBOSITY_INFO
endif

TOOL_COMMAND = vcs -l vcslog.txt -full64 -nc -j$(NUM_PROCS) -lca -liblist_work
TOOL_OPTIONS_RTL = +v2k +vcs+lic+wait
TOOL_OPTIONS_SV  = -sverilog +incdir+. +incdir+vkits/jtag +incdir+vkits/max10
TOOL_OPTIONS = +vcs+lic+wait -t ps -timescale=1ps/1ps +error+100
COMP_LINT_OPTIONS =
TOOL_LINT_OPTIONS = +warn=all,noVCM-NOCOV-ENC +lint=all,noSV-PIU,noWMIA-L,noULCO -msg_config=vcs_sim_msg_config.txt

SIMV_PROFILE_OPTIONS :=
ifeq ($(ENABLE_SIM_PROFILE),1)
TOOL_OPTIONS += -simprofile=time
SIMV_PROFILE_OPTIONS += -simprofile=time
endif

COMP_COMMAND = vlogan -full64 -nc
COMP_OPTIONS = $(TOOL_OPTIONS_RTL) $(TOOL_OPTIONS_SV) +define+SIMULATION

ifneq ($(ENABLE_DEBUG),)
	COMP_COMMAND += -lca -kdb
	TOOL_COMMAND += -lca -kdb
	TOOL_OPTIONS_SV += +define+ENABLE_VCS_DEBUG
	TOOL_OPTIONS += -P $(VERDI_HOME)/share/PLI/VCS/LINUX64/novas.tab $(VERDI_HOME)/share/PLI/VCS/LINUX64/pli.a
ifeq ($(ENABLE_DEBUG_ACCESS),1)
	TOOL_OPTIONS += -debug_access+all
else
	TOOL_OPTIONS += -debug_acc+pp+dmptf -debug_region+cell+encrypt -debug_access+r -debug_access+calltf
endif
endif

ifneq ($(ENABLE_MEM_DEBUG),)
	TOOL_OPTIONS_SV += +define+ENABLE_VCS_MEM_DEBUG
endif

TOPLEVEL_NAME = sim_top

##############################################################################
# Set default goal before any targets. The default goal here is "build"
##############################################################################
DEFAULT_TARGET := run

.DEFAULT_GOAL := default
.PHONY: default
default: run

##############################################################################
# Makefile starts here
##############################################################################

vcs_sim_msg_config_auto.txt : auto_suppress.py
	$(VENV_PY) auto_suppress.py

vcs_sim_msg_config.txt : vcs_sim_msg_config_base.txt vcs_sim_msg_config_auto.txt
	cat $^ > $@

.PHONY: dev-libraries
dev-libraries: dev-libraries.done

dev-libraries.done:
	rm -rf libraries
	./comp_sim.sh |& tee dev-libraries.log
	touch $@

.PHONY: vip-libraries
vip-libraries: vip-libraries.done

vip-libraries.done : dev-libraries.done $(FUNDAMENTAL_FILES)
	$(COMP_COMMAND) $(COMP_OPTIONS) $(FUNDAMENTAL_FILES) | tee vip-libraries.log
	touch $@

.PHONY: clean-vip
clean-vip: clean-simv
	rm -f vip-libraries.done vip-libraries.log

.PHONY : compile_all
compile_all: compile
compile_all: compile

.PHONY : compile
compile: simv

simv: vip-libraries.done $(RTL_FILES) $(SCM_RTL_FILES) $(TB_FILES) $(MODEL_FILES) nios_mem.hex
	rm -f simv
	$(COMP_COMMAND) $(COMP_OPTIONS) $(COMP_LINT_OPTIONS) $(RTL_FILES) $(SCM_RTL_FILES) | tee compile.log
	$(COMP_COMMAND) $(COMP_OPTIONS) $(COMP_LINT_OPTIONS) $(TB_FILES) $(MODEL_FILES) | tee -a compile.log
	$(TOOL_COMMAND) $(TOOL_OPTIONS) $(TOOL_LINT_OPTIONS) $(COVERAGE_OPTIONS) $(TOPLEVEL_NAME) | tee -a compile.log

../nios_mem.hex:
	make -C ../ nios_mem.hex

nios_mem.hex: ../nios_mem.hex
	cp $< $@

.PHONY : run
run: simv
	./simv -l simulation.log

run_gui:
	./simv -gui

dve:
	dve -full64 -vpd vcdplus.vpd -session testbench_session.tcl

verdi:
	verdi -simflow -simdir . -top work.sim_top -ssf novas.fsdb -ssr testbench_verdi.ses

clean-simv: 
	-rm -rf simv simulation.*.log *monitor.log *.transaction_trace simv.vdb *.vpd *.fsdb
	-rm -rf *.ver *.dat pfr_sys_u_nios_cpu.tr compile.log simulation.log

clean: clean-simv
	-rm -rf synopsys_sim.setup synopsys_sim.tmp.setup *_sim.setup *.done
	-rm -rf csrc DVEfiles simv.daidir simv.cst AN.DB libraries tr_db.log avalon_config.ver avalon_config.hex
	-rm -f sim.log sim.*.log vcs.key *.vcdplus.vpd inter.vpd simv simulation.log simulation.*.log vcslog.txt
	-rm -f .restartSimSession.tcl.old ucli.key .synopsys_dve_rebuild.tcl vc_hdrs.h .vlogansetup.env .vlogansetup.args
	-rm -rf simv.vdb *.vpd urgReport .inter.vpd.uvm cm.log *.fsdb verdiLog verdi_config_file
	-rm -rf simulation.*.log *monitor.log *.transaction_trace novas_dump.log novas.conf novas.rc compile.log dev-libraries.log vip-libraries.log
	-rm -rf *.mif *.ver *.dat *.hex pfr_sys_u_nios_cpu.tr
	-rm -rf spi_flash_files/*.txt
	-rm -rf vcs_sim_msg_config.txt vcs_sim_msg_config_auto.txt

dev-clean:
	git clean -dfx .

coverage:
	urg -full64 -dir simv.vdb -elfile verdi_coverage.el -plan dut.hvp -hvp_score_missing

coverage_gui:
	verdi -cov -covdir simv.vdb -elfile verdi_coverage.el

.PHONY: grep-lint
grep-lint: vcslog.txt
	-grep -Pzo '^\s*Lint.*\n.*,\s*\d+' $^

.PHONY: grep-warning
grep-warnings: vcslog.txt
	-grep -Pzo '^\s*Warning.*\n.*,\s*\d+' $^

.PHONY: check-warnings
check-warnings: vcslog.txt
	$(info Checking $^ for warnings)
	test $(shell grep -P '^\s*Warning' $^ | wc -l) -eq 0
