//****************************************************************************
//
// SPDX-License-Identifier: MIT-0
// SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
//
//****************************************************************************
//
// clocks_and_resets
//
//****************************************************************************

`timescale 1 ps / 1 ps
`default_nettype none

module clocks_and_resets (
    // FPGA fabric Clock
    input  wire fpga_clk_100,
    // External FPGA reset
    input  wire fpga_reset_n,
    // HPS to FPGA reset signal
    input  wire h2f_reset_in,
    // Lightweight HPS to FPGA clock from PLL
    output wire lwh2f_clock,
    // Synchronized lightweight HPS to FPGA reset signal
    output wire lwh2f_reset_n,
    // usb31 phy reconfig clock from PLL
    output wire usbpr_clock,
    // Synchronized usb31 phy reconfig reset signal
    output wire usbpr_reset_n,
    // HPS to FPGA clock from PLL
    output wire h2f_clock,
    // Synchronized HPS to FPGA reset signal
    output wire h2f_reset_n,
    // FPGA to HPS clock from PLL
    output wire f2h_clock,
    // Synchronized FPGA to HPS reset signal
    output wire f2h_reset_n,
    // FPGA to SDRAM clock from PLL
    output wire f2sdram_clock,
    // Synchronized FPGA to SDRAM reset signal
    output wire f2sdram_reset_n
);

    // Clock and Reset
    wire pll_clk_lwh2f;
    wire pll_clk_usbpr;
    wire pll_clk_h2f;
    wire pll_clk_f2h;
    wire pll_clk_f2sdram;
    wire system_reset;
    assign lwh2f_clock = pll_clk_lwh2f;
    assign usbpr_clock = pll_clk_usbpr;
    assign h2f_clock = pll_clk_h2f;
    assign f2h_clock = pll_clk_f2h;
    assign f2sdram_clock = pll_clk_f2sdram;

    // Generate n_initdone
    wire ninit_done;
    user_rst_clkgate u_ninit_done (.ninit_done(ninit_done));

    // Generate a reset signal for the PLL
    // based on the nINIT_DONE signal and the FPGA reset signal
    wire pll_reset;
    assign pll_reset = ninit_done | ~fpga_reset_n;

    // Instantiate the system clock PLL
    // Reset the PLL based on nINIT_DONE
    wire pll_locked;
    sys_pll u_sys_pll (
        .refclk  (fpga_clk_100),
        .locked  (pll_locked),
        .rst     (pll_reset),
        .outclk_0(pll_clk_usbpr),
        .outclk_1(pll_clk_h2f),
        .outclk_2(pll_clk_lwh2f),
        .outclk_3(pll_clk_f2h),
        .outclk_4(pll_clk_f2sdram)
    );

    // Generate a system reset based on the PLL lock status
    // and the HPS to FPGA reset signal
    assign system_reset = ~pll_locked | h2f_reset_in;

    // Create a synchronized usb31 phy reconfig reset signal
    // to the usb31 phy reconfig clock domain
    altera_std_synchronizer #(
        .depth(3)
    ) fpga_reset_n_sync_0 (
        .clk    (pll_clk_usbpr),
        .reset_n(~system_reset),
        .din    (1'b1),
        .dout   (usbpr_reset_n)
    );

    // Create a synchronized HPS to FPGA reset signal
    // to the HPS to FPGA clock domain
    altera_std_synchronizer #(
        .depth(3)
    ) fpga_reset_n_sync_1 (
        .clk    (pll_clk_h2f),
        .reset_n(~system_reset),
        .din    (1'b1),
        .dout   (h2f_reset_n)
    );

    // Create a synchronized lightweight HPS to FPGA reset signal
    // to the lightweight HPS to FPGA clock domain
    altera_std_synchronizer #(
        .depth(3)
    ) fpga_reset_n_sync_2 (
        .clk    (pll_clk_lwh2f),
        .reset_n(~system_reset),
        .din    (1'b1),
        .dout   (lwh2f_reset_n)
    );

    // Create a synchronized FPGA to HPS reset signal
    // to the FPGA to HPS clock domain
    altera_std_synchronizer #(
        .depth(3)
    ) fpga_reset_n_sync_3 (
        .clk    (pll_clk_f2h),
        .reset_n(~system_reset),
        .din    (1'b1),
        .dout   (f2h_reset_n)
    );

    // Create a synchronized FPGA to SDRAM reset signal
    // to the FPGA to SDRAM clock domain
    altera_std_synchronizer #(
        .depth(3)
    ) fpga_reset_n_sync_4 (
        .clk    (pll_clk_f2sdram),
        .reset_n(~system_reset),
        .din    (1'b1),
        .dout   (f2sdram_reset_n)
    );

endmodule
