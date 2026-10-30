`timescale 1 ps / 1 ps
`default_nettype none
// // // //
// `include "altr_cmn/altr_cmn_macros.sv"
// `include "altr_cmn/altr_verbosity_uvm_pkg.sv"
// // // //
/** Include UVM Package */
// `include "uvm_pkg.sv"
// // // //
module testbench ();

    // import verbosity_pkg::*;
    // `define VERBOSITY_INFO     1
    // `define VERBOSITY_FAILURE  2
    // typedef enum int {
    //      VERBOSITY_NONE,     // 0
    //      VERBOSITY_FAILURE,  // 1
    //      VERBOSITY_ERROR,    // 2
    //      VERBOSITY_WARNING,  // 3
    //      VERBOSITY_INFO,     // 4
    //      VERBOSITY_DEBUG     // 5
    // } Verbosity_t;
    //==================================================================================================
    // Parameters
    //==================================================================================================
    //DUT

    covergroup cg_input_parametrization;

    endgroup : cg_input_parametrization

    cg_input_parametrization input_parametrization;


    //==================================================================================================
    // Local Signals
    //==================================================================================================

    // Clock and reset
    logic clk250M;
    logic sys_rst;

    // DUT I/O
    logic [31:0] gpi, gpo;
    logic nios_start, nios_pass, nios_done;
    logic nios_cca, nios_ocm, nios_ram, nios_buf;
    logic nios_dma, nios_bad, nios_ok;

    // BFM Signals

    //==================================================================================================
    // DUT
    //==================================================================================================
    /* from main.c
        #define NIOS_START_BIT (0x1 << 31)
        #define NIOS_PASS_BIT (0x1 << 30)
        #define NIOS_DONE_BIT (0x1 << 29)
    */
    tb u0 (
        .clk_clk    (clk250M),  //   input,   width = 1,   clk.clk
        .reset_reset(sys_rst),  //   input,   width = 1, reset.reset
        .gpi_export (gpi),      //   input,  width = 32,   gpi.export
        .gpo_export (gpo)       //  output,  width = 32,   gpo.export
    );
    /*
        // to accelerator csr
        // Avalon-MM Master Interface
        .accelerator_csr_m0_waitrequest   (),      //   input,   width = 1,              accelerator_csr_m0.waitrequest
        .accelerator_csr_m0_readdata      (),      //   input,  width = 32,                                .readdata
        .accelerator_csr_m0_readdatavalid (),      //   input,   width = 1,                                .readdatavalid
        .accelerator_csr_m0_burstcount    (),      //  output,   width = 1,                                .burstcount
        .accelerator_csr_m0_writedata     (),      //  output,  width = 32,                                .writedata
        .accelerator_csr_m0_address       (),      //  output,  width = 20,                                .address
        .accelerator_csr_m0_write         (),  //  output,   width = 1,                                .write
        .accelerator_csr_m0_read          (),  //  output,   width = 1,                                .read
        .accelerator_csr_m0_byteenable    (),      //  output,   width = 4,                                .byteenable
        .accelerator_csr_m0_debugaccess   (),      //  output,   width = 1,                                .debugaccess

        // from accelerator irq output
        .accelerator_irq_in_receiver_irq_irq (),      //   input,   width = 1, accelerator_irq_in_receiver_irq.irq

        // from accelerator dma output
        .dma_avmm_in_s0_waitrequest          (),      //  output,   width = 1,                  dma_avmm_in_s0.waitrequest
        .dma_avmm_in_s0_readdata             (),      //  output,  width = 32,                                .readdata
        .dma_avmm_in_s0_readdatavalid        (),      //  output,   width = 1,                                .readdatavalid
        .dma_avmm_in_s0_burstcount           (),      //   input,   width = 1,                                .burstcount
        .dma_avmm_in_s0_writedata            (),      //   input,  width = 32,                                .writedata
        .dma_avmm_in_s0_address(),  //   input,  width = 19,                                .address
        .dma_avmm_in_s0_write(),  //   input,   width = 1,                                .write
        .dma_avmm_in_s0_read(),  //   input,   width = 1,                                .read
        .dma_avmm_in_s0_byteenable           (),      //   input,   width = 4,                                .byteenable
        .dma_avmm_in_s0_debugaccess          ()       //   input,   width = 1,                                .debugaccess
    ); // */
    // =========================================================================
    assign nios_start = gpo[31];
    assign nios_pass = gpo[30];
    assign nios_done = gpo[29];
    // =========================================================================
    assign nios_cca = gpo[28];
    assign nios_ocm = gpo[27];
    assign nios_ram = gpo[26];
    assign nios_buf = gpo[25];
    assign nios_dma = gpo[24];
    // =========================================================================
    assign nios_bad = gpo[23];
    assign nios_ok = gpo[22];
    // =========================================================================
    //      //        //      //       //      //       //       // 
    //==================================================================================================
    // clk and reset
    //==================================================================================================

    // Clock generators
    initial begin
        clk250M <= 1'b0;
        forever begin
            #2ns clk250M <= ~clk250M;  // 250 MHz
        end
    end

    // Reset generator
    initial begin
        sys_rst <= 1'b1;
        #200ns $display("Deasserting reset");
        sys_rst <= 1'b0;
    end

    //==================================================================================================
    // BFMs
    //==================================================================================================

    //==================================================================================================
    // Testbench tasks
    //==================================================================================================
    task wait_for_nios_done();
        // print(VERBOSITY_INFO, "== ==== Waiting for Nios Done ==== ==");
        fork : wait_watched
            begin
                while (nios_done != 1'b1) begin
                    @clk250M;
                end
            end
            begin
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== 1 sec timeout waiting for Nios Done ==== ==");
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== 2 secs timeout waiting for Nios Done ==== ==");
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== 3 secs timeout waiting for Nios Done ==== ==");
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== 4 secs timeout waiting for Nios Done ==== ==");
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== 5 secs timeout waiting for Nios Done ==== ==");
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== 6 secs timeout waiting for Nios Done ==== ==");
                #1000ms;
                // print(VERBOSITY_INFO, "\n");
                // print(VERBOSITY_INFO, " == ==== Timeout waiting for Nios Done ==== ==");
                // print(VERBOSITY_FAILURE, " == ==== Nios Done not received ==== ==");
                $fatal;
            end
        join_any
        disable wait_watched;
        // print(VERBOSITY_INFO, "\n");
        // print(VERBOSITY_INFO, " == ==== Nios Done Received ==== ==");
    endtask

    task wait_for_nios_start();
        // print(VERBOSITY_INFO, "\n");
        // print(VERBOSITY_INFO, " == ==== Waiting for Nios Start ==== ==");
        fork : wait_watched
            begin
                while (nios_start != 1'b1) begin
                    @clk250M;
                end
            end
            begin
                #100ms;
                // print(VERBOSITY_INFO, " == ==== Timeout waiting for Nios Start ==== ==");
                // print(VERBOSITY_FAILURE, " == ==== Nios Start not received ==== ==");
                $fatal;
            end
        join_any
        disable wait_watched;
        // print(VERBOSITY_INFO, " == ==== Nios Start Received ==== ==");
    endtask

    //==================================================================================================
    // test initial block
    //==================================================================================================
    initial begin
`ifdef SIM_VERBOSITY
        if (`SIM_VERBOSITY == "VERBOSITY_INFO") set_verbosity(VERBOSITY_INFO);
        else if (`SIM_VERBOSITY == "VERBOSITY_DEBUG") set_verbosity(VERBOSITY_DEBUG);
        else if (`SIM_VERBOSITY == "VERBOSITY_WARNING") set_verbosity(VERBOSITY_WARNING);
        else if (`SIM_VERBOSITY == "VERBOSITY_ERROR") set_verbosity(VERBOSITY_ERROR);
        else if (`SIM_VERBOSITY == "VERBOSITY_FAILURE") set_verbosity(VERBOSITY_FAILURE);
        else if (`SIM_VERBOSITY == "VERBOSITY_NONE") set_verbosity(VERBOSITY_NONE);
        else begin
            $display("Invalid verbosity level: %s", `SIM_VERBOSITY);
            $fatal;
        end
`endif
    end

    initial begin
`ifdef ENABLE_VCS_DEBUG
        // Enable debugging
        // only enable when debug required delta cycle information
        // $vcdplusdeltacycleon();
        $vcdpluson();
        // example to dump only minimum signal to reduce simulation time
        // $vcdpluson(0, sim_top.tb.dut.u_pfr.u_core.u_rfnvram_master);
        $fsdbDumpvars();
`ifdef ENABLE_VCS_MEM_DEBUG
        $vcdplusmemon();
`endif
`endif
        // Run the test
        // print(VERBOSITY_INFO, " == ==== Starting test ==== ==");
        // print_divider(VERBOSITY_INFO);
        repeat (100)
        @clk250M;  // Wait for reset deassertion and some time for the system to stabilize

        wait_for_nios_start();

        wait_for_nios_done();

        repeat (100) @clk250M;  // Wait for some time for the system to stabilize

        if (nios_pass != 1'b1) begin
            // print(VERBOSITY_FAILURE, " == ==== TEST_FAIL ==== ==");
        end else begin
            // print(VERBOSITY_INFO, " == ==== TEST_PASS ==== ==");
        end

        // Finish the simulation
        repeat (400) @clk250M;  // Wait for some time for the system to stabilize
        // print(VERBOSITY_INFO, " == ==== Test complete ==== ==");
        $finish;
    end

    // Sample the input parameterization
    initial begin
        input_parametrization = new();
        input_parametrization.sample();
    end

endmodule
