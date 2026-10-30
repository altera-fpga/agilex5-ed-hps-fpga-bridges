// // // // // // // // // // //
`timescale 1 ps / 1 ps
// // // // // // // // // // //
`default_nettype none
// // // // // // // // // // //
// import BFM libraries
import altera_axi_bfm_pkg::*;
import host_memory_class_pkg::*;
// // // // // // // // // // //
import hps_h2f_pkg::*;
import hps_h2f_lw_pkg::*;
// // // // // // // // // // //
// Define a helper macro for easier access to the HPS2FPGA BFM instance
`define lwh2f_bfm_m dut.u_cache_coh_top.u_shell_subsys.u_hps_subsys.u_agilex_hps.intel_agilex_5_soc_inst.sm_hps.sundancemesa_hps_inst.lwh2f_bfm_gen.lwh2f_axi4_master_inst
// // // // // // // // // // //
module testbench ();
    //==================================================================================================
    // Local Signals
    //==================================================================================================
    // Clock and reset
    logic clk25M;
    logic clk100M;
    wire system_clock;
    wire system_reset;

    // DUT signals
    logic fpga_reset_n;
    wire [3:0] fpga_led_pio;
    wire [3:0] fpga_dipsw_pio;
    wire [3:0] fpga_button_pio;

    bit [255:0] HPS_DDR_data[0:63];

    // Tie off inputs to HPS
    assign fpga_button_pio = '0;
    assign fpga_dipsw_pio = '0;

    // Test flag
    bit h2f_pass = 1;
    bit lwh2f_pass = 1;
    bit test_pass = 1;

    // hps2fpga test flag
    bit h2f_f1 = 0;
    bit h2f_f2 = 0;
    bit h2f_f3 = 0;
    bit h2f_f4 = 0;
    bit h2f_f5 = 0;
    bit h2f_f6 = 0;
    // lwhps2fpga test flag
    bit lwh2f_f1 = 0;
    bit lwh2f_f2 = 0;

    logic [31:0] lwh2f_lwh2f_rd_data;

    // test signals
    logic fpga_toggle_led0;
    logic fpga_toggle_led1;
    logic fpga_toggle_led2;
    logic fpga_toggle_led3;
    // -----------------------------------------------------------------------------
    int burst_len = 63;  // AXI4 burst length is beats-1, so 64 beats = 63
    // -----------------------------------------------------------------------------
    // F2H Subordinate BFM - Response constants
    // -----------------------------------------------------------------------------
    localparam bit [1:0] AXI4_RESP_OKAY = 2'b00;
    localparam bit [1:0] AXI4_RESP_EXOKAY = 2'b01;
    localparam bit [1:0] AXI4_RESP_SLVERR = 2'b10;
    localparam bit [1:0] AXI4_RESP_DECERR = 2'b11;
    // -----------------------------------------------------------------------------
    // F2H Subordinate BFM - HPS DDR Base Address
    // -----------------------------------------------------------------------------
    localparam longint unsigned HPS_DDR_BASE_ADDR = 64'h0000_0000_0000_0000;
    localparam int unsigned LWH2F_LED_PIO_BASE_ADDR = 32'hFF20_0000;
    // -----------------------------------------------------------------------------    
    // `define LWH2F_LED_PIO_BASE_ADDR 0xFF20_0000;
    // -----------------------------------------------------------------------------
    // h2f_bfm_m
    string h2f_messageQueue[$];
    task automatic h2f_storeMessage(input string msg);
        string formattedMsg = $sformatf("[%0t] %s", $time, msg);
        h2f_messageQueue.push_back(formattedMsg);
    endtask  // */
    // -----------------------------------------------------------------------------
    // lwh2f_bfm_m
    string lwh2f_messageQueue[$];
    task automatic lwh2f_storeMessage(input string msg);
        string formattedMsg = $sformatf("[%0t] %s", $time, msg);
        lwh2f_messageQueue.push_back(formattedMsg);
    endtask  // */
    // -----------------------------------------------------------------------------
    // f2h_bfm_s
    string f2h_messageQueue[$];
    task automatic f2h_storeMessage(input string msg);
        string formattedMsg = $sformatf("[%0t] %s", $time, msg);
        f2h_messageQueue.push_back(formattedMsg);
    endtask  // */
    // =============================================================================
    // write32 - Single 32-bit write via LWHPS2FPGA BFM
    // =============================================================================
    task automatic write32(input logic [31:0] addr, input logic [31:0] data);
        AlteraAxiTransaction wr_tr;
        // Sync to BFM clock 
        @(`lwh2f_bfm_m.m_cb);
        // Build and drive transaction 
        wr_tr = `lwh2f_bfm_m.AXI4MAN.bfm.manager_bfm_wr_tx(1, addr);
        wr_tr.set_size(AXI4_BYTES_4);
        wr_tr.set_burst_length(0);
        wr_tr.set_burst_type(BURST_TYPE_FIXED);
        wr_tr.set_data_words(data, 0);
        wr_tr.set_write_strobes(4'hf, 0);
        `lwh2f_bfm_m.AXI4MAN.bfm.put_transaction(wr_tr);
        `lwh2f_bfm_m.AXI4MAN.bfm.drive_transaction();
        // Post-transaction settling 
        repeat (2) @(posedge clk100M);
    endtask  // */
    // =============================================================================
    // read32 - Single 32-bit read via LWHPS2FPGA BFM
    // =============================================================================
    task automatic read32(input logic [31:0] addr, output logic [31:0] data);
        AlteraAxiTransaction rd_tr;
        // Sync to BFM clock
        @(`lwh2f_bfm_m.m_cb);
        // Build and drive transaction
        rd_tr = `lwh2f_bfm_m.AXI4MAN.bfm.manager_bfm_rd_tx(1, addr);
        rd_tr.set_size(AXI4_BYTES_4);
        rd_tr.set_burst_length(0);
        rd_tr.set_burst_type(BURST_TYPE_FIXED);
        `lwh2f_bfm_m.AXI4MAN.bfm.put_transaction(rd_tr);
        `lwh2f_bfm_m.AXI4MAN.bfm.drive_transaction();
        // Capture return data
        data = rd_tr.get_data_words(0);
        // Post-transaction settling
        repeat (2) @(posedge clk100M);
    endtask
    //==================================================================================================
    // DUT
    //==================================================================================================
    cache_coh_accel dut (
        //  HPS External Oscillator
        .hps_osc_clk                    (clk25M),
        //  FPGA fabric Clock
        .pll_refclk_100                 (clk100M),
        //  External warm reset
        // coming from MAX10 device which cleans it
        .fpga_reset_n                   (fpga_reset_n),
        //  Switches and LEDs
        // .fpga_user_leds              (fpga_led_pio),
        // .fpga_user_switches          (fpga_dipsw_pio),
        // .fpga_user_push_buttons      (fpga_button_pio),
        //
        //  HPS
        //
        //  HPS EMIF
        .emif_hps_emif_mem_0_mem_ck_t   (),
        .emif_hps_emif_mem_0_mem_ck_c   (),
        .emif_hps_emif_mem_0_mem_a      (),
        .emif_hps_emif_mem_0_mem_act_n  (),
        .emif_hps_emif_mem_0_mem_ba     (),
        .emif_hps_emif_mem_0_mem_bg     (),
        .emif_hps_emif_mem_0_mem_cke    (),
        .emif_hps_emif_mem_0_mem_cs_n   (),
        .emif_hps_emif_mem_0_mem_odt    (),
        .emif_hps_emif_mem_0_mem_reset_n(),
        .emif_hps_emif_mem_0_mem_par    (),
        .emif_hps_emif_mem_0_mem_alert_n('0),
        .emif_hps_emif_oct_0_oct_rzqin  ('0),
        .emif_hps_emif_ref_clk_0_clk    ('0),
        .emif_hps_emif_mem_0_mem_dqs_t  (),
        .emif_hps_emif_mem_0_mem_dqs_c  (),
        .emif_hps_emif_mem_0_mem_dq     (),
        //
        //  HPS Peripherals
        .usb31_io_vbus_det              ('0),
        .usb31_io_flt_bar               ('0),
        .usb31_io_usb_ctrl              (),
        .usb31_io_usb31_id              ('0),
        .usb31_phy_refclk_p             ('0),
        .usb31_phy_rx_serial_n          ('1),
        .usb31_phy_rx_serial_p          ('0),
        .usb31_phy_tx_serial_n          (),
        .usb31_phy_tx_serial_p          (),
        .hps_usb1_DATA0                 (),
        .hps_usb1_DATA1                 (),
        .hps_usb1_DATA2                 (),
        .hps_usb1_DATA3                 (),
        .hps_usb1_DATA4                 (),
        .hps_usb1_DATA5                 (),
        .hps_usb1_DATA6                 (),
        .hps_usb1_DATA7                 (),
        .hps_usb1_CLK                   ('0),
        .hps_usb1_STP                   (),
        .hps_usb1_DIR                   ('0),
        .hps_usb1_NXT                   ('0),
        .hps_jtag_tck                   ('0),
        .hps_jtag_tms                   ('0),
        .hps_jtag_tdo                   (),
        .hps_jtag_tdi                   ('0),
        .hps_sdmmc_CCLK                 (),
        .hps_sdmmc_CMD                  (),
        .hps_sdmmc_D0                   (),
        .hps_sdmmc_D1                   (),
        .hps_sdmmc_D2                   (),
        .hps_sdmmc_D3                   (),
        .hps_emac2_TX_CLK               (),
        .hps_emac2_RX_CLK               ('0),
        .hps_emac2_TX_CTL               (),
        .hps_emac2_RX_CTL               ('0),
        .hps_emac2_TXD0                 (),
        .hps_emac2_TXD1                 (),
        .hps_emac2_RXD0                 ('0),
        .hps_emac2_RXD1                 ('0),
        .hps_emac2_PPS                  (),
        .hps_emac2_PPS_TRIG             ('0),
        .hps_emac2_TXD2                 (),
        .hps_emac2_TXD3                 (),
        .hps_emac2_RXD2                 ('0),
        .hps_emac2_RXD3                 ('0),
        .hps_emac2_MDIO                 (),
        .hps_emac2_MDC                  (),
        .hps_uart0_RX                   ('0),
        .hps_uart0_TX                   (),
        .hps_i3c1_SDA                   (),
        .hps_i3c1_SCL                   (),
        .hps_gpio0_io0                  (),
        .hps_gpio0_io1                  (),
        .hps_gpio0_io11                 (),
        .hps_gpio1_io3                  (),
        .hps_gpio1_io4                  ()
        //
    );
    //==================================================================================================
    // clk and reset
    //==================================================================================================
    // Clock generators -- 25 MHz
    initial begin
        clk25M <= 1'b0;
        forever begin
            #20ns clk25M <= ~clk25M;  // 25MHz
        end
    end
    // Clock generators -- 100 MHz
    initial begin
        clk100M <= 1'b0;
        forever begin
            #5ns clk100M <= ~clk100M;  // 100MHz
        end
    end
    //==================================================================================================
    // HPS2FPGA AXI4 Object handles.
    //==================================================================================================
    //---------------------------------------------------------
    // HPS2FPGA AXI4 Object handles.
    //---------------------------------------------------------
    // AlteraAxiTransaction h2f_wr_tr, h2f_rd_tr;
    //---------------------------------------------------------
    // LWHPS2FPGA AXI4 Object handles.
    //---------------------------------------------------------
    AlteraAxiTransaction lwh2f_wr_tr, lwh2f_rd_tr;
    //---------------------------------------------------------
    // FPGA2HPS AXI4 Object handles.
    //---------------------------------------------------------
    // AlteraAxiTransaction wr_rsp, rd_rsp, rd_rsp_burst;
    // AlteraAxiTransaction f2h_rd_resp_tr, f2h_wr_resp_tr;
    //---------------------------------------------------------
    // Enable BFM debug mode
    //assign `h2f_bfm_m.dbg = 1;
    //---------------------------------------------------------

    // This is the objects you call methods on to drive the BFMs in the testbench
    // `lwh2f_bfm_m.AXI4MAN.bfm.hello();
    // Print initialization of the BFMs
    initial begin
        `lwh2f_bfm_m.AXI4MAN.bfm.hello();
    end
    //==================================================================================================
    // Simulation Overrides
    //==================================================================================================
    // Override the behavior of the HPS IP generated reset. Use the input bridge
    // reset to hold the simulation model BFM in reset until the PLL clock is stable.
    initial begin
        // force dut.u_cache_coh_top.u_shell_subsys.u_hps_subsys.u_agilex_hps.intel_agilex_5_soc_inst.sm_hps.sundancemesa_hps_inst.h2f_bfm_gen.h2f_axi4_master_inst.rstn = !dut.u_cache_coh_top.u_shell_subsys.u_hps_subsys.u_agilex_hps.intel_agilex_5_soc_inst.sm_hps.hps2fpga_rst;
        force dut.u_cache_coh_top.u_shell_subsys.u_hps_subsys.u_agilex_hps.intel_agilex_5_soc_inst.sm_hps.sundancemesa_hps_inst.lwh2f_bfm_gen.lwh2f_axi4_master_inst.rstn = !dut.u_cache_coh_top.u_shell_subsys.u_hps_subsys.u_agilex_hps.intel_agilex_5_soc_inst.sm_hps.hps2fpga_rst;
    end
    // ==================================================================================================
    //     Simulation Main
    // ==================================================================================================
    // Crete a simulation watchdog to avoid infinite simulation runs
    initial begin
        #5ms;  // 5ms simulation timeout
        $display("Test Timeout...");
        $display("Baseline Simulation Test Failed!");
`ifdef ENABLE_VCS_DEBUG
        $vcdplusclose;
`endif
        $finish;
    end
    // ==============================================================
    // Watchdog for internal reset timeout 
    initial begin
        #1ms;
        if (dut.system_reset_n != 1'b1) begin
            $fatal("INIT_DONE not asserted within 1ms");
        end
    end
    // ==============================================================
    // Wait for system_reset_n to deassert
    initial begin
        // wait to reset for 10 clock cycles
        repeat (10) @(posedge clk100M);
        $display("Waiting for Reset to de-assert");
        while (dut.system_reset_n != 1'b1) begin
            @clk100M;
        end
        $display("system_reset_n deasserted - INIT_DONE");
    end
    // ==============================================================
    // --------------------------------------------------------------
    // Initialize HPS_DDR_data with a repeating pattern, incrementing each beat
    // --------------------------------------------------------------
    initial begin
        bit [255:0] base_pattern = 256'h00010203_04050607_08090A0B_0C0D0E0F_00010203_04050607_08090A0B_0C0D0E0F;
        for (int i = 0; i < 64; i++) begin
            HPS_DDR_data[i] = base_pattern + i;
        end
    end
    // ==============================================================
    initial begin
`ifdef ENABLE_VCS_DEBUG
        // Enable VPD waveform dumping (only for VCS)
        $vcdpluson(0, testbench);
        $vcdplusmemon(0, testbench);
`endif
        // // // // // // // // // // // //
        $display("Init Reset");
        fpga_reset_n = 1'b1;
        // test signals
        fpga_toggle_led0 = 1'b0;
        fpga_toggle_led1 = 1'b0;
        fpga_toggle_led2 = 1'b0;
        fpga_toggle_led3 = 1'b0;
        // // // // // // // // // // // //
        // test signals
        // // // // // // // // // // // //
        // Hold reset for 10 clock cycles
        repeat (10) @(posedge clk100M);
        // // // // // // // // //
        $display("Calling lwh2f_bfm_m.AXI4MAN.bfm.m_reset() at %0t", $time);
        `lwh2f_bfm_m.AXI4MAN.bfm.m_reset();
        // // // // // // // // //
        // Hold reset for 10 clock cycles
        repeat (10) @(posedge clk100M);
        // // // // // // // // //
        $display("Assert Reset");
        fpga_reset_n = 1'b0;
        // // // // // // // // // // // //
        // Hold reset for 50 clock cycles
        repeat (50) @(posedge clk100M);
        // // // // // // // // // // // //
        $display("De-assert Reset");
        fpga_reset_n = 1'b1;
        #10ns;
        // // // // // // // // // // // //
        $display("Waiting for Reset to de-assert");
        while (dut.system_reset_n != 1'b1) begin
            @clk100M;
        end
        // // // // // // // // // // // //
        $display("system_reset_n deasserted - INIT_DONE");
        `lwh2f_bfm_m.AXI4MAN.bfm.m_reset();
        `lwh2f_bfm_m.AXI4MAN.bfm.do_collect_responses();
        // reset_done = 1;

        // Post reset cycles
        repeat (10) @(posedge clk100M);
        // test signals
        fpga_toggle_led3 = 1'b0;
        fpga_toggle_led2 = 1'b0;
        fpga_toggle_led1 = 1'b0;
        fpga_toggle_led0 = 1'b1;
        // test signals

        // Post reset cycles
        repeat (100) @(posedge clk100M);
        // test signals
        fpga_toggle_led3 = 1'b0;
        fpga_toggle_led2 = 1'b0;
        fpga_toggle_led1 = 1'b1;
        fpga_toggle_led0 = 1'b1;
        // test signals

        // Post reset cycles
        repeat (100) @(posedge clk100M);
        // test signals
        fpga_toggle_led3 = 1'b0;
        fpga_toggle_led2 = 1'b1;
        fpga_toggle_led1 = 1'b1;
        fpga_toggle_led0 = 1'b1;
        // test signals

        // Post reset cycles
        repeat (100) @(posedge clk100M);
        // test signals
        fpga_toggle_led3 = 1'b0;
        fpga_toggle_led2 = 1'b1;
        fpga_toggle_led1 = 1'b1;
        fpga_toggle_led0 = 1'b0;
        // test signals
        //=======================================================================================
        //                        LW HPS to FPGA Bridge Test
        //=======================================================================================
        begin : lw_bridge_test
            // ===========================================================
            // Sync to BFM clock before starting
            // ===========================================================
            @(`lwh2f_bfm_m.m_cb);
            @(`lwh2f_bfm_m.m_cb);
            // ===========================================================
            lwh2f_storeMessage("*******************************************************");
            lwh2f_storeMessage("*************** Test LWHPS2FPGA Bridge *****************");
            lwh2f_storeMessage("*******************************************************");
            // ===========================================================
            //  Accel IP  CSR Debug Registers Write Test Sequence
            // ===========================================================
            lwh2f_storeMessage("*******************************************************");
            lwh2f_storeMessage("*****       START TEST ACCEL IP CSR Block        ***** \n");
            lwh2f_storeMessage("*******************************************************");
            lwh2f_storeMessage("[TESTINFO]: Test Sequence for Write Started");
            // -----------------------------------------------------------
            //  Write to the ACCEL_CSR_DBG_0_OFFSET register
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 1st Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #1 - Write data (32'h01020304) at ACCEL_CSR_DBG_0_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #1 ***\n", $time);
            write32(ACCEL_CSR_DBG_0_OFFSET, ACCEL_CSR_DBG_0_VALUE);
            lwh2f_storeMessage("**** TEST 1st Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 2nd Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #2 - Write data (32'h05060708) at ACCEL_CSR_DBG_1_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #2 ***\n", $time);
            write32(ACCEL_CSR_DBG_1_OFFSET, ACCEL_CSR_DBG_1_VALUE);
            lwh2f_storeMessage("**** TEST 2nd Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 3rd Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #3 - Write data (32'h090a0b0c) at ACCEL_CSR_DBG_2_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #3 ***\n", $time);
            write32(ACCEL_CSR_DBG_2_OFFSET, ACCEL_CSR_DBG_2_VALUE);
            lwh2f_storeMessage("**** TEST 3rd Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 4th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #4 - Write data (32'h0d0e0f10) at ACCEL_CSR_DBG_3_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #4 ***\n", $time);
            write32(ACCEL_CSR_DBG_3_OFFSET, ACCEL_CSR_DBG_3_VALUE);
            lwh2f_storeMessage("**** TEST 4th Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 5th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #5 - Write data (32'h11121314) at ACCEL_CSR_DBG_4_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #5 ***\n", $time);
            write32(ACCEL_CSR_DBG_4_OFFSET, ACCEL_CSR_DBG_4_VALUE);
            lwh2f_storeMessage("**** TEST 5th Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 6th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #6 - Write data (32'h15161718) at ACCEL_CSR_DBG_5_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #6 ***\n", $time);
            write32(ACCEL_CSR_DBG_5_OFFSET, ACCEL_CSR_DBG_5_VALUE);
            lwh2f_storeMessage("**** TEST 6th Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 7th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #7 - Write data (32'h191A1B1C) at ACCEL_CSR_DBG_6_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #7 ***\n", $time);
            write32(ACCEL_CSR_DBG_6_OFFSET, ACCEL_CSR_DBG_6_VALUE);
            lwh2f_storeMessage("**** TEST 7th Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 8th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #8 - Write data (32'h1D1E1F20) at ACCEL_CSR_DBG_7_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #8 ***\n", $time);
            write32(ACCEL_CSR_DBG_7_OFFSET, ACCEL_CSR_DBG_7_VALUE);
            lwh2f_storeMessage("**** TEST 8th Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 9th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #9 - Write data (32'h21222324) at ACCEL_CSR_DBG_8_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #9 ***\n", $time);
            write32(ACCEL_CSR_DBG_8_OFFSET, ACCEL_CSR_DBG_8_VALUE);
            lwh2f_storeMessage("**** TEST 9th Write **** \n");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 10th Write **** \n");
            lwh2f_storeMessage(
                "LWHPS2FPGA Write #10 - Write data (32'h25262728) at ACCEL_CSR_DBG_9_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - WRITE #10 ***\n", $time);
            write32(ACCEL_CSR_DBG_9_OFFSET, ACCEL_CSR_DBG_9_VALUE);
            lwh2f_storeMessage("**** TEST 10th Write **** \n");
            // ===========================================================
            //  H2F msgDMA IP CSR Debug Registers Write Test Sequence
            // ===========================================================
            //  */
            // -----------------------------------------------------------
            // wait cycles - sync with LW_H2F bus clock            
            repeat (20) @(posedge clk100M);
            @(`lwh2f_bfm_m.m_cb);
            // -----------------------------------------------------------
            // ===========================================================
            //                  Read-back Verify Sequence
            // ===========================================================

            lwh2f_storeMessage("[TESTINFO]: Test Sequence for Read Started");
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 1st Read **** \n");
            lwh2f_storeMessage("LWHPS2FPGA Read #1 at ACCEL_CSR_DBG_0_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - READ #1 ***\n", $time);
            read32(ACCEL_CSR_DBG_0_OFFSET, lwh2f_lwh2f_rd_data);
            if (lwh2f_lwh2f_rd_data == ACCEL_CSR_DBG_0_VALUE) begin
                lwh2f_f1 = 1'b1;
                lwh2f_storeMessage($sformatf("lwh2f_f1 = %b  PASS", lwh2f_f1));
                lwh2f_storeMessage(
                    $sformatf(
                    "Read correct data 0x%08h at ACCEL_CSR_DBG_0_OFFSET", lwh2f_lwh2f_rd_data));
            end else begin
                lwh2f_storeMessage($sformatf(
                                   "FAIL: Expected 0x%08h at ACCEL_CSR_DBG_0_OFFSET, got 0x%08h",
                                   ACCEL_CSR_DBG_0_VALUE,
                                   lwh2f_lwh2f_rd_data
                                   ));
            end
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 1st Read **** \n");
            // -----------------------------------------------------------
            repeat (10) @(posedge clk100M);
            @(`lwh2f_bfm_m.m_cb);
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 2nd Read **** \n");
            lwh2f_storeMessage("LWHPS2FPGA Read #2 at ACCEL_CSR_DBG_1_OFFSET");
            $display("\n@%0t *** LWHPS2FPGA - READ #2 ***\n", $time);
            read32(ACCEL_CSR_DBG_1_OFFSET, lwh2f_lwh2f_rd_data);
            if (lwh2f_lwh2f_rd_data == ACCEL_CSR_DBG_1_VALUE) begin
                lwh2f_f2 = 1'b1;
                lwh2f_storeMessage($sformatf("lwh2f_f2 = %b  PASS", lwh2f_f2));
                lwh2f_storeMessage(
                    $sformatf(
                    "Read correct data 0x%08h at ACCEL_CSR_DBG_1_OFFSET", lwh2f_lwh2f_rd_data));
            end else begin
                lwh2f_storeMessage($sformatf(
                                   "FAIL: Expected 0x%08h at ACCEL_CSR_DBG_1_OFFSET, got 0x%08h",
                                   ACCEL_CSR_DBG_1_VALUE,
                                   lwh2f_lwh2f_rd_data
                                   ));
            end
            // -----------------------------------------------------------
            //  */
            // -----------------------------------------------------------
            lwh2f_storeMessage("**** TEST 2nd Read **** \n");
            // -----------------------------------------------------------
            repeat (10) @(posedge clk100M);
            @(`lwh2f_bfm_m.m_cb);
            // -----------------------------------------------------------

            // ===========================================================
            //               FPGA2HPS msgDMA Subsystem
            //                  Base: 0x2200_0000
            // ===========================================================

            // -----------------------------------------------------------
            //     FPGA2HPS Descriptor OCM (0x2200_0000)
            // -----------------------------------------------------------

            // ===========================================================
            //               HPS2FPGA msgDMA Subsystem
            //                  Base: 0x2300_0000
            // ===========================================================

            // -----------------------------------------------------------
            //     HPS2FPGA Descriptor OCM (0x2300_0000)
            // -----------------------------------------------------------

            // ===========================================================
            //                  Test Result
            // ===========================================================
            repeat (10) @(posedge clk100M);
            @(`lwh2f_bfm_m.m_cb);
            // -----------------------------------------------------------
            if (lwh2f_f1 == 1'b1 && lwh2f_f2 == 1'b1) begin
                lwh2f_storeMessage("LWH2F Test Pass!");
                lwh2f_pass = 1;
            end else begin
                lwh2f_storeMessage("LWH2F Test Fail!");
                lwh2f_pass = 0;
            end
            // ===========================================================
        end : lw_bridge_test
        // =======================================================================================
        //                     End LW HPS to FPGA Bridge Test
        // =======================================================================================

        // // // // // // // // // // //
        /////////////////////////////////////////////////////////////////////////////////
        foreach (lwh2f_messageQueue[i]) begin
            $display("%s", lwh2f_messageQueue[i]);
        end  // */
        /////////////////////////////////////////////////////////////////////////////////
        // // // // // // // // // // //

        // =======================================================================================
        //                           FPGA to HPS Bridge Test
        // =======================================================================================
        /////////////////////////////////////////////////////////////////////////////////
        // Post reset cycles
        repeat (10) @(posedge clk100M);
        // Sync this thread to the BFM clock
        @(`lwh2f_bfm_m.m_cb);
        // Sync this thread to the BFM clock
        @(`lwh2f_bfm_m.m_cb);

        // =======================================================================================
        //                        End FPGA to HPS Bridge Test
        // =======================================================================================
        $display("@%0t*********************************************", $time);
        $display("@%0t************** TEST SUMMARY *****************", $time);
        $display("@%0t*********************************************", $time);

        if (lwh2f_pass) begin
            $display("LWHPS2FPGA Test -- PASS");
        end else begin
            $display("LWHPS2FPGA Test -- FAIL");
            test_pass = 0;
        end

        if (test_pass == 0) begin

`ifdef ENABLE_VCS_DEBUG
            $vcdplusclose;
`endif
            $fatal("Baseline Simulation Test Failed!");
        end else begin
            $display("\n@%0t**************** END OF TEST ****************\n", $time);
            $display("\n@%0t************ Simulation complete ************\n", $time);

`ifdef ENABLE_VCS_DEBUG
            $vcdplusclose;
`endif
            #0 $finish(1);
        end
    end
    // ==============================================================
    //    //    //    //    //
    // ==============================================================
    //            // // // // // // // // // // // //
    // ==============================================================
    //    //    //    //    //
    // ==============================================================

    // ==============================================================
    // F2H Subordinate BFM - Write Responder (BRESP OKAY)
    // ==============================================================

    // ==============================================================
    //            // // // // // // // // // // // //
    // ==============================================================

    // ==============================================================
    // F2H Subordinate BFM - Burst Read Responder for msgDMA
    // ==============================================================

    // ==============================================================
    //            // // // // // // // // // // // //
    // ==============================================================

    ////////////////////////////////////////////////////////////
    //  //  //  //  //  //  //  //
    ////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////
    //  //  //  //  //  //  //  //
    ////////////////////////////////////////////////////////////	
endmodule
