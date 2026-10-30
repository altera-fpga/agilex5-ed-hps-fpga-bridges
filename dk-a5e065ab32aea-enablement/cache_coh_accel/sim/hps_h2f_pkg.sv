// Defines constants for HPS to FPGA bridge interface
package hps_h2f_pkg;

    // H2F (HPS-to-FPGA) bridge parameters (cache_coh_accel design)
    localparam H2F_ID_WIDTH = 4;
    localparam H2F_DATA_WIDTH = 128;
    localparam H2F_ADDR_WIDTH = 38;
    localparam H2F_SYMBOLS_PER_WORD = H2F_DATA_WIDTH / 8;

    // On-Chip Memory (OCM) via H2F bridge
    localparam H2F_OCRAM_BASE_ADDR = 32'h0000_0000;  // 0x0002_0000_0000 in HPS address space
    localparam H2F_OCRAM_SIZE = 32'h0008_0000;  // 512 KB

    // No GPIO/PIO peripherals in this design

endpackage
