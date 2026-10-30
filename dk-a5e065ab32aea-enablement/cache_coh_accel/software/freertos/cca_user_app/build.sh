#!/bin/bash
set -e

###############################################################################
# Configuration
###############################################################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_FOLDER="$(realpath "$SCRIPT_DIR/../../..")"
FREERTOS_TOP_FOLDER="$PROJECT_FOLDER/software/freertos"
CC_ACCEL_FOLDER="$FREERTOS_TOP_FOLDER/cca_user_app"

CMAKE_COMMAND=cmake
ARM_TOOLCHAIN="arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf"
ARM_TOOLCHAIN_BIN="$FREERTOS_TOP_FOLDER/$ARM_TOOLCHAIN/bin"
PFG_FILE="$FREERTOS_TOP_FOLDER/qspi_flash_image_agilex5_boot.pfg"
SOF_PATH="$PROJECT_FOLDER/output_files/cache_coh_accel.sof"

###############################################################################
# Build Process
###############################################################################

BUILD_FOLDER="$CC_ACCEL_FOLDER/build"
FPGA_BINARIES_DIR="$BUILD_FOLDER/fpga_binaries"

# Download ARM toolchain
GET_ARM_TOOLCHAIN_DONE="$FREERTOS_TOP_FOLDER/get-arm-toolchain.done"

NEEDS_REBUILD=0
if [ ! -f "$GET_ARM_TOOLCHAIN_DONE" ]; then
    NEEDS_REBUILD=1
fi

if [ "$NEEDS_REBUILD" -eq 1 ]; then
    echo ">>> Downloading ARM toolchain $ARM_TOOLCHAIN"
    cd "$FREERTOS_TOP_FOLDER"
    wget "https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/${ARM_TOOLCHAIN}.tar.xz"
    tar -xvf "${ARM_TOOLCHAIN}.tar.xz"
    rm -f "${ARM_TOOLCHAIN}.tar.xz"
    touch "$GET_ARM_TOOLCHAIN_DONE"
fi

if [ ! -d "$ARM_TOOLCHAIN_BIN" ]; then
    echo "ERROR: ARM toolchain not found at $ARM_TOOLCHAIN_BIN even after download attempt"
    exit 1
fi

echo "==========================================================================="
echo " Building: cca_user_app"
echo "==========================================================================="

# Configure cmake build system on first run
if [ ! -f "$BUILD_FOLDER/Makefile" ]; then
    echo ">>> Configuring cmake build system for cca_user_app"
    mkdir -p "$BUILD_FOLDER"
    cd "$BUILD_FOLDER"
    PATH="$(realpath "$ARM_TOOLCHAIN_BIN"):$PATH" \
        $CMAKE_COMMAND "$CC_ACCEL_FOLDER" -G"Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release -DSMP=OFF
fi

# Build ATF (bl2/bl31), bl2.hex, cca_user_app.bin, and fip.bin — all driven by cmake
echo ">>> Building cca_user_app (ATF + cca_user_app + FIP)"
cd "$BUILD_FOLDER"
PATH="$(realpath "$ARM_TOOLCHAIN_BIN"):$PATH" $CMAKE_COMMAND --build .

# Copy SOF
echo ">>> Copying SOF for cca_user_app"
mkdir -p "$FPGA_BINARIES_DIR"
cp -f "$SOF_PATH" "$FPGA_BINARIES_DIR/cache_coh_accel.sof"

# Generate FSBL SOF
echo ">>> Generating FSBL SOF for cca_user_app"
cd "$FPGA_BINARIES_DIR"
quartus_pfg -c -o hps_path="$FPGA_BINARIES_DIR/bl2.hex" cache_coh_accel.sof fsbl.sof

# Generate core.rbf
echo ">>> Generating core.rbf for cca_user_app"
cd "$FPGA_BINARIES_DIR"
quartus_pfg -c fsbl.sof ghrd.rbf -o hps=ON -o hps_path="$FPGA_BINARIES_DIR/bl2.hex"

# Generate RBF with size header
echo ">>> Generating RBF with size header for cca_user_app"
RBF_SIZE=$(stat -c%s "$FPGA_BINARIES_DIR/ghrd.core.rbf")
python3 -c "import sys;sys.stdout.buffer.write(($RBF_SIZE).to_bytes(4, byteorder='little'))" \
    | cat - "$FPGA_BINARIES_DIR/ghrd.core.rbf" > "$FPGA_BINARIES_DIR/ghrd.core_size.rbf"

# Generate JIC
echo ">>> Generating JIC for cca_user_app"
cd "$FPGA_BINARIES_DIR"
cp -f "$PFG_FILE" .
quartus_pfg -c qspi_flash_image_agilex5_boot.pfg
cp -f "$FPGA_BINARIES_DIR/qspi_image.jic" "$CC_ACCEL_FOLDER/cca_user_app.hps.jic"

echo ">>> cca_user_app build complete"
echo ""

echo "==========================================================================="
echo " cca_user_app build complete!"
echo "==========================================================================="
