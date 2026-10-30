// ****************************************************************************
//
//    SPDX-License-Identifier: MIT-0
//    SPDX-FileCopyrightText: Copyright (C) 2025, 2026 Altera Corporation
//
// ****************************************************************************
//    Cache Coherency Accelerator IP Top Level Module
// ****************************************************************************
//    This is the top-level module for the Cache Coherency Accelerator IP,
//    encapsulating the CSR register block and the Accelerator Core   
// ****************************************************************************
`timescale 1 ps / 1 ps
// // // //  
`default_nettype none
// // // //
module cca_accel_top #(
) (
    // Clock and reset
    input wire clock_in,
    input wire reset_in,

    // Avalon-MM CSR Slave Interface
    input  wire        avmm_csr_read,
    input  wire        avmm_csr_write,
    input  wire [19:0] avmm_csr_address,
    input  wire [31:0] avmm_csr_writedata,
    input  wire [ 3:0] avmm_csr_byteenable,
    output wire [31:0] avmm_csr_readdata,
    output wire        avmm_csr_waitrequest,
    output wire        avmm_csr_readdatavalid,

    // Avalon-ST Sink (HPS2FPGA msgDMA → Accelerator)
    output wire         avst_sink_ready,
    input  wire         avst_sink_valid,
    input  wire [255:0] avst_sink_data,

    // Avalon-ST Source (Accelerator → FPGA2HPS msgDMA)
    input  wire         avst_source_ready,
    output wire         avst_source_valid,
    output wire [255:0] avst_source_data
);
    // ============================================================================
    // Local Parameters
    // ============================================================================
    // local parameters can be defined here if needed 
    // ============================================================================
    // Internal Signals - CSR Interface
    // ============================================================================
    logic accel_rst_int;
    logic [31:0] cca_ctrl_data;  // accelerator control data
    logic [31:0] cca_stat_data;  // accelerator status data
    logic [31:0] cca_ip_id;  // accelerator IP ID
    logic [31:0] cca_ip_rev;  // accelerator IP version
    logic [31:0] cca_snk_byte_reg;  // accelerator sink byte count
    logic [31:0] cca_src_byte_reg;  // accelerator source byte count
    // ============================================================================
    // CSR Interface Instantiation
    // ============================================================================
    cca_accel_csr #() u_accel_csr_if (
        // Clock and reset
        .sys_clk(clock_in),
        .sys_rst(reset_in),

        // Avalon-MM CSR Interface
        .avmm_csr_read         (avmm_csr_read),
        .avmm_csr_write        (avmm_csr_write),
        .avmm_csr_address      (avmm_csr_address),
        .avmm_csr_writedata    (avmm_csr_writedata),
        .avmm_csr_byteenable   (avmm_csr_byteenable),
        .avmm_csr_readdata     (avmm_csr_readdata),
        .avmm_csr_waitrequest  (avmm_csr_waitrequest),
        .avmm_csr_readdatavalid(avmm_csr_readdatavalid),

        // Accelerator control interface
        .cca_reset      (accel_rst_int),     // Full subsystem reset
        // Accelerator status interface
        .accel_ip_id    (cca_ip_id),
        .accel_ip_rev   (cca_ip_rev),
        .accel_stat_data(cca_stat_data),
        .accel_snk_byte (cca_snk_byte_reg),
        .accel_src_byte (cca_src_byte_reg),
        // System control outputs
        .cca_ctrl_data  (cca_ctrl_data)      // Accelerator control data
    );
    // ============================================================================
    // Cache Coherency Accelerator Instantiation
    // ============================================================================
    cca_accel #() u_cca_accel (
        // Clock and reset
        .sys_clk(clock_in),
        .sys_rst(accel_rst_int),

        // Control/Status Interface (from CSR)
        .cca_ctrl_data   (cca_ctrl_data),     // Accelerator control data
        .cca_ip_id       (cca_ip_id),
        .cca_ip_rev      (cca_ip_rev),
        .cca_stat_reg    (cca_stat_data),
        .cca_snk_byte_reg(cca_snk_byte_reg),
        .cca_src_byte_reg(cca_src_byte_reg),

        // Avalon-ST Sink (HPS2FPGA msgDMA → Accelerator)
        .avst_sink_ready(avst_sink_ready),
        .avst_sink_valid(avst_sink_valid),
        .avst_sink_data (avst_sink_data),

        // Avalon-ST Source (Accelerator → FPGA2HPS msgDMA)
        .avst_source_ready(avst_source_ready),
        .avst_source_valid(avst_source_valid),
        .avst_source_data (avst_source_data)
    );
    // ============================================================================
    // Output assignments 
    // ============================================================================
endmodule
