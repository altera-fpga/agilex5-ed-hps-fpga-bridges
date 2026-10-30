#! /bin/bash
set -eo pipefail

MENTOR_VIP_AE=${MENTOR_VIP_AE:-$QUARTUS_ROOTDIR/../ip/altera/mentor_vip_ae}
export QUESTA_MVC_GCC_LIB=$MENTOR_VIP_AE/common/questa_mvc_core/linux_x86_64_gcc-*_ius
export LD_LIBRARY_PATH=${QUESTA_MVC_GCC_LIB}:${LD_LIBRARY_PATH}
export INCA_64BIT=1

# Determine the path to the bash script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Toplevel design directory
DESIGN_ROOT_DIR="$(realpath $SCRIPT_DIR/..)"

# Toplevel sim script generation
SIM_SCRIPT_GEN_DIR=$DESIGN_ROOT_DIR/sim_setup_cache_coh_accel
CCA_TOP_SIM_DIR=$DESIGN_ROOT_DIR/cache_coh_top/sim

cp -rf $SIM_SCRIPT_GEN_DIR/xcelium/cds.lib $SIM_SCRIPT_GEN_DIR/xcelium/cds_libs $SIM_SCRIPT_GEN_DIR/xcelium/hdl.var .
# Append cache_coh_top sub-IP library definitions (skip duplicates)
grep "^DEFINE" $CCA_TOP_SIM_DIR/xcelium/cds.lib | while read -r line; do
    lib=$(echo "$line" | awk '{print $2}')
    if ! grep -q "^DEFINE[[:space:]]*${lib}[[:space:]]" cds.lib; then
        echo "$line" >> cds.lib
    fi
done

# Compile cache_coh_top sub-IP libraries first
source $CCA_TOP_SIM_DIR/xcelium/xcelium_setup.sh \
SKIP_ELAB=1 \
SKIP_SIM=1 \
QSYS_SIMDIR=$CCA_TOP_SIM_DIR

source $SIM_SCRIPT_GEN_DIR/xcelium/xcelium_setup.sh \
SKIP_ELAB=1 \
SKIP_SIM=1 \
QSYS_SIMDIR=$SIM_SCRIPT_GEN_DIR

# Compile all design files and testbench files, including the top level.
# (These are all the files required for simulation other than the files
# compiled by the IP script)
#
xmvlog -sv $DESIGN_ROOT_DIR/src/clocks_and_resets.sv
xmvlog -sv $DESIGN_ROOT_DIR/src/cca_accel_core.sv
xmvlog -sv $DESIGN_ROOT_DIR/src/cca_accel_csr.sv
xmvlog -sv $DESIGN_ROOT_DIR/src/cca_accel_top.sv
xmvlog -sv $DESIGN_ROOT_DIR/cache_coh_accel.sv
xmvlog -sv $DESIGN_ROOT_DIR/sim/hps_h2f_pkg.sv
xmvlog -sv $DESIGN_ROOT_DIR/sim/hps_h2f_lw_pkg.sv
xmvlog -sv $DESIGN_ROOT_DIR/sim/cache_coh_accel_tb.sv

#
# TOP_LEVEL_NAME is used in this script to set the top-level simulation or
# testbench module/entity name.
#
# Run the IP script again to elaborate and simulate the top level:
# - Specify TOP_LEVEL_NAME and USER_DEFINED_ELAB_OPTIONS.
# - Override the default USER_DEFINED_SIM_OPTIONS. For example, to run
#   until $finish(), set to an empty string: USER_DEFINED_SIM_OPTIONS="".
#

USER_DEFINED_ELAB_OPTIONS="\"\
  -timescale '1ps/1ps' \
  -no_mixed_bus \
  -ENABLE_BIND_WITH_COMMON_PKG \""

USER_DEFINED_SIM_OPTIONS="\"\
  -MESSAGES \
  -sv_root ${QUESTA_MVC_GCC_LIB} \
  -sv_lib libaxi4_IN_SystemVerilog_IUS_full_DVC \""

source $SIM_SCRIPT_GEN_DIR/xcelium/xcelium_setup.sh \
SKIP_FILE_COPY=1 \
SKIP_DEV_COM=1 \
SKIP_COM=1 \
TOP_LEVEL_NAME="'testbench'" \
QSYS_SIMDIR=$SIM_SCRIPT_GEN_DIR \
USER_DEFINED_ELAB_OPTIONS="$USER_DEFINED_ELAB_OPTIONS" \
USER_DEFINED_SIM_OPTIONS="$USER_DEFINED_SIM_OPTIONS"
#
# TOP-LEVEL TEMPLATE - END

# Check simulation results for errors
if [ -f xmsim.log ]; then
    if grep -qiP "(FAIL|\bError\b|Fatal|\*E,)" xmsim.log; then
        echo "ERROR: Simulation failed! Check xmsim.log for details."
        exit 1
    else
        echo "Simulation completed successfully."
        exit 0
    fi
else
    echo "ERROR: xmsim.log file not found!"
    exit 1
fi


