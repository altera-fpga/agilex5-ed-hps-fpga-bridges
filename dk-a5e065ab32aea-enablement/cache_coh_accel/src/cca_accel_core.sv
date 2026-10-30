// ****************************************************************************
//
//    SPDX-License-Identifier: MIT-0
//    SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
//
// ****************************************************************************
//    Cache Coherency Accelerator Core
// ****************************************************************************
//    This module implements the cache coherency accelerator logic with two 
//    Avalon-ST interfaces: 
//         # one sink interface for receiving data from HPS through the 
//           msgDMA Rd IP; 
//         # one source interface for sending data to HPS DDR through the 
//           msgDMA Wr IP;
// ****************************************************************************
// // // // // 
`timescale 1 ps / 1 ps
// // // // // 
`default_nettype none
// // // // //
module cca_accel #(
) (
    // Clock and sys_rst
    input wire sys_clk,
    input wire sys_rst,

    // Control/Status Interface (from CSR)
    input  wire [31:0] cca_ctrl_data,
    output wire [31:0] cca_ip_id,
    output wire [31:0] cca_ip_rev,
    output wire [31:0] cca_stat_reg,
    output wire [31:0] cca_snk_byte_reg,
    output wire [31:0] cca_src_byte_reg,

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
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    localparam ACCEL_IP_ID = 32'hCCA0_0101;  // Accelerator IP ID
    localparam ACCEL_IP_REV = 32'h0101_0003;  // Version 1.0.3
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Command definitions
    localparam logic [7:0] CMD_NOP = 8'h00;
    localparam logic [7:0] CMD_PROCESS_DATA = 8'h01;
    localparam logic [7:0] CMD_PASS_THROUGH = 8'h02;
    localparam logic [7:0] CMD_ERROR_TEST = 8'hFF;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Memory size for internal buffer (in 256-bit words)
    localparam int BUFFER_DEPTH = 1024;  // 32KB buffer
    localparam int BUFFER_ADDR_WIDTH = $clog2(BUFFER_DEPTH);
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // ============================================================================
    // Internal Signals
    // ============================================================================
    // Registered control data for stable processing    
    logic [31:0] cca_ctrl_data_q  /* synthesis preserve */;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    //    Registered control data for stable processing    
    logic [31:0] accel_life_beat_cnt  /* synthesis preserve */;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic cmd_process  /* synthesis preserve */;  // Process data command
    logic cmd_passthru  /* synthesis preserve */;  // Pass-through command
    logic cmd_err_tst  /* synthesis preserve */;  // Error test command
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic cmd_process_q1  /* synthesis preserve */;  // Process data command
    logic cmd_passthru_q1  /* synthesis preserve */;  // Pass-through command
    logic cmd_err_tst_q1  /* synthesis preserve */;  // Error test command
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic accel_cnt_reset  /* synthesis preserve */;  // Bit 10: ABORT
    logic accel_abort  /* synthesis preserve */;  // Bit 10: ABORT
    logic accel_stop  /* synthesis preserve */;  // Bit  9: STOP
    logic accel_start  /* synthesis preserve */;  // Bit  8: START
    logic [7:0] accel_cmd  /* synthesis preserve */;  // Bits 7:0: CMD
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic accel_busy;
    logic accel_done;
    logic accel_error;
    logic accel_alive;
    logic accel_alive_flag;
    logic [31:0] accel_stat_data;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Avalon-ST Sink FFs (HPS2FPGA msgDMA → Accelerator)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic ready_toggle;  // 1-bit shift register for throttling
    logic avst_rd_ready;
    logic avst_rd_valid_q;
    logic [255:0] avst_rd_data_q;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Avalon-ST Sink (HPS2FPGA msgDMA → Accelerator)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Processing Pipeline Control Signals
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic stage_0_enable;
    logic stage_0_valid  /* synthesis preserve */;  // pipeline stage 0 
    logic [255:0] stage_0_data  /* synthesis preserve */;  // pipeline stage 0 
    logic stage_1_enable;
    logic stage_1_valid  /* synthesis preserve */;  // pipeline stage 1 
    logic [255:0] stage_1_data  /* synthesis preserve */;  // pipeline stage 1 
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic pipeline_valid;
    logic pipeline_valid_q1  /* synthesis preserve */;
    logic valid_rd_ena  /* synthesis preserve */;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic [1:0] rd_req_toggle  /* synthesis preserve */;
    logic [255:0] rd_data_q1  /* synthesis preserve */;  // fifo_output.dataout,     
    logic [255:0] rd_data_q2  /* synthesis preserve */;  // fifo_output.dataout,     
    logic [255:0] rd_data_q3  /* synthesis preserve */;  // fifo_output.dataout,     
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic accel_cnt_rst;  // SM synch reset
    logic accel_cnt_rst_bit0;
    logic accel_cnt_rst_bit0_q1  /* synthesis preserve */;
    logic accel_cnt_rst_bit0_q2  /* synthesis preserve */;
    logic accel_cnt_plse;
    logic snk_byte_cnt_ena;
    logic src_byte_cnt_ena;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic [31:0] snk_byte_cnt  /* synthesis preserve */;
    logic [31:0] src_byte_cnt  /* synthesis preserve */;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // Avalon-ST Source (Accelerator → FPGA2HPS msgDMA)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    logic avs_wr_byte_cnt  /* synthesis preserve */;
    logic avs_wr_data_valid  /* synthesis preserve */;
    logic [255:0] avs_wr_data;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    //       System Control Register (0x10)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Command decode
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    assign cmd_process = (accel_cmd == CMD_PROCESS_DATA);  // Process data command
    assign cmd_passthru = (accel_cmd == CMD_PASS_THROUGH);  // Pass-through command
    assign cmd_err_tst = (accel_cmd == CMD_ERROR_TEST);  // Error test command
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            // clear control data on reset
            cca_ctrl_data_q <= 32'h0;

            accel_cnt_reset <= 1'b0;
            accel_stop <= 1'b0;
            accel_abort <= 1'b0;
            accel_start <= 1'b0;
            accel_cmd <= 8'h0;

            cmd_process_q1 <= 1'b0;  // Process data command
            cmd_passthru_q1 <= 1'b0;  // Pass-through command
            cmd_err_tst_q1 <= 1'b0;  // Error test command

        end else begin
            // control data is registered for stable processing in the accelerator logic
            cca_ctrl_data_q <= cca_ctrl_data;
            // otherwise keep the previous command for processing until next START
            accel_cnt_reset <= cca_ctrl_data_q[11];  // Bit 11: CNT_RESET
            accel_abort <= cca_ctrl_data_q[10];  // Bit 10: ABORT
            accel_stop <= cca_ctrl_data_q[9];  // Bit 9: STOP
            accel_start <= cca_ctrl_data_q[8];  // Bit 8: START

            cmd_process_q1 <= cmd_process;  // Process data command
            cmd_passthru_q1 <= cmd_passthru;  // Pass-through command
            cmd_err_tst_q1 <= cmd_err_tst;  // Error test command

            if (accel_abort || accel_stop)  // START bit is set, latch the command;
                accel_cmd <= 8'b0;  // Bits 7:0: CMD Byte
            else if (cca_ctrl_data_q[8])  // START bit is set, latch the command;
                // otherwise keep the previous command for processing until next START
                accel_cmd <= cca_ctrl_data_q[7:0];  // Bits 7:0: CMD Byte
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Accelerator Status Signals
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            accel_busy <= 1'b0;
            accel_done <= 1'b0;
            accel_error <= 1'b0;
            accel_alive <= 1'b0;
            accel_alive_flag <= 1'b0;
        end else begin
            accel_busy <= 1'b0;
            accel_done <= 1'b0;
            accel_error <= 1'b0;
            // Use bit 24 of life beat counter as alive signal 
            // (toggles every ~100ms at 200MHz)
            accel_alive <= accel_life_beat_cnt[2];
            if (accel_alive)
                accel_alive_flag <= 1'b1;  // Set alive flag once alive signal is asserted
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            accel_cnt_rst_bit0 <= 1'b1;
            accel_cnt_rst_bit0_q1 <= 1'b1;
            accel_cnt_rst_bit0_q2 <= 1'b1;
            accel_cnt_plse <= 1'b1;
        end else begin
            accel_cnt_rst_bit0 <= accel_cnt_reset;
            accel_cnt_rst_bit0_q1 <= accel_cnt_rst_bit0;
            accel_cnt_rst_bit0_q2 <= accel_cnt_rst_bit0_q1;
            accel_cnt_plse <= (accel_cnt_rst_bit0 || accel_cnt_rst_bit0_q1 ||
                               accel_cnt_rst_bit0_q2);
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Accelerator Status Signals
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            accel_stat_data <= 32'h0;
        end else begin
            accel_stat_data <= {
                27'h0, accel_error, accel_done, accel_busy, accel_alive, accel_alive_flag
            };
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Avalon-ST Sink Slave Interface (FPGA2HPS msgDMA Read → Accelerator)
    // data and valid are registered on sink interface for better timing and stable processing in accelerator logic
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Pipeline Stage 0 : Register the input
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            ready_toggle <= 1'b0;  // Initialize with one bit set
            avst_rd_ready <= 1'b0;
            avst_rd_valid_q <= 1'b0;
        end else begin
            // Shift register: rotate left by 1 bit
            ready_toggle <= ~ready_toggle;  // Toggle between 0 and 1 for throttling
            // enables the upstream msgDMA read prefetcher 
            // need to throttle it with ready_toggle[0] //
            avst_rd_ready <= (avst_sink_valid && stage_0_enable && ~avst_rd_valid_q);
            // Ready to accept data when the downstream pipeline is ready to process new data
            avst_rd_valid_q <= (avst_sink_valid && avst_rd_ready);
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            avst_rd_data_q <= 256'b0;
        end else begin
            if (avst_sink_valid && avst_rd_ready) avst_rd_data_q <= avst_sink_data;
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            accel_life_beat_cnt <= 32'h0;
        end else begin
            accel_life_beat_cnt <= accel_life_beat_cnt + 1'b1;
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (accel_cnt_plse) begin
            snk_byte_cnt_ena <= 1'b0;
            snk_byte_cnt <= 32'b0;
            src_byte_cnt_ena <= 1'b0;
            src_byte_cnt <= 32'b0;
        end else begin
            snk_byte_cnt_ena <= (stage_0_valid && stage_1_enable);  // count only when data advances
            src_byte_cnt_ena <= avs_wr_byte_cnt;
            if (snk_byte_cnt_ena)
                // when we receive data to Avalon-ST sink interface
                // increment sink byte count 
                snk_byte_cnt <= snk_byte_cnt + 32'd1;  // 32 bytes per transfer : 32'd32
            if (src_byte_cnt_ena)
                // when we successfully send data to Avalon-ST source interface
                // increment source byte count 
                src_byte_cnt <= src_byte_cnt + 32'd1;  // 32 bytes per transfer : 32'd32
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Processing Pipeline Instantiation
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // cca_process_pipeline
    //
    // 8-stage byte-inversion pipeline
    // Each stage inverts 4 bytes (32 bits) of the 256-bit data word.
    // After 8 stages, all 256 bits are inverted.
    //
    // Stage 1 : invert bits [127:  0] (bytes  0- 15)
    // Stage 2 : invert bits [255:128] (bytes 16-31)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Pipeline Stage 1 : invert bits [127: 0] (bytes  0- 15)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Stage  0 enable: advance when empty OR stage_1 can accept
    assign stage_0_enable = !stage_0_valid || stage_1_enable;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            stage_0_valid <= 1'b0;
            stage_0_data <= '0;
        end else if (stage_0_enable) begin
            stage_0_valid <= avst_rd_valid_q;
            if (cmd_passthru_q1)
                // Pass through data without inversion
                stage_0_data <= avst_rd_data_q;
            else
                // Invert bits [127:0], pass through the rest
                stage_0_data <= {
                    avst_rd_data_q[255:128], ~avst_rd_data_q[127:0]
                };
        end
    end
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Pipeline Stage 2 : invert bits [255:128] (bytes 16-31)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Stage  1 enable: advance when empty OR stage_2 can accept
    assign stage_1_enable = !stage_1_valid || avst_source_ready;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            stage_1_valid <= 1'b0;
            stage_1_data <= '0;
        end else if (stage_1_enable) begin
            stage_1_valid <= stage_0_valid;
            if (cmd_passthru_q1)
                // Pass through data without inversion
                stage_1_data <= stage_0_data;
            else
                // Invert bits [255:128], pass through the rest
                stage_1_data <= {
                    ~stage_0_data[255:128], stage_0_data[127:0]
                };
        end
    end
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Upstream pipeline: freeze when Q1 stage is blocked
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            pipeline_valid <= 1'b0;
            pipeline_valid_q1 <= 1'b0;
        end else begin
            pipeline_valid_q1 <= pipeline_valid;
            // Track valid data held in pipeline by busy_w   
            if (stage_1_valid && !avst_source_ready)
                // Valid data but blocked by busy - mark pipeline as valid   
                pipeline_valid <= 1'b1;
            else if (pipeline_valid && avst_source_ready)
                // Busy released and we pushed the held data - clear flag   
                pipeline_valid <= 1'b0;
        end
    end  // */
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    // Output stage: simple passthrough (no gating logic)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            avs_wr_byte_cnt <= 1'b0;
            avs_wr_data_valid <= 1'b0;
            avs_wr_data <= 256'b0;
        end else begin
            // count the bytes sent to Avalon-ST source interface
            avs_wr_byte_cnt <= (stage_1_valid && avst_source_ready);
            if (avst_source_ready) begin
                // output stage data to Avalon-ST source interface
                avs_wr_data_valid <= stage_1_valid;
                avs_wr_data <= stage_1_data;
            end
        end
    end  // */
    // ============================================================================
    // Output assignments 
    // ============================================================================
    // Avalon-ST Sink (HPS2FPGA msgDMA → Accelerator)
    assign avst_sink_ready = avst_rd_ready;
    // Avalon-ST Source (Accelerator → FPGA2HPS msgDMA)
    assign avst_source_valid = avs_wr_data_valid;
    assign avst_source_data = avs_wr_data;
    // Accelerator Status Register (0x18)
    assign cca_ip_id = ACCEL_IP_ID;
    assign cca_ip_rev = ACCEL_IP_REV;
    assign cca_stat_reg = accel_stat_data;
    // Status Counter (0x18)
    assign cca_snk_byte_reg = snk_byte_cnt;
    assign cca_src_byte_reg = src_byte_cnt;
    // =========================================================================   
    //                        Assertions   
    // =========================================================================
    // start of assertions based verification
    // synthesis translate_off
    // =========================================================================
    // Assertion + error flags: avst_source_data must carry incrementing 32-bit
    // words across every accepted write beat (avst_source_valid && avst_source_ready).
    //
    // Bus layout: the msgDMA places words big-endian on the 256-bit bus
    //   (first/lowest-address memory word → bits[255:224]), and each 32-bit word
    //   is byte-reversed (LSB of memory word → MSB of bus lane).
    //   The checker un-swaps each lane to natural order before comparing.
    //   In natural order the step between consecutive words is always +1.
    //
    //   chk_us[0] = natural order of bits[255:224]  (src word 0 of beat)
    //   chk_us[7] = natural order of bits[ 31:  0]  (src word 7 of beat)
    //
    //   intra-beat : chk_us[l] == chk_us[0] + l  for l = 1..7
    //   inter-beat : chk_us[0] of beat N == chk_prev_us7 + 1
    //
    // Waveform-visible signals (sim only, inside translate_off):
    //   chk_intra_err      – pulses 1 for one cycle on intra-beat mismatch
    //   chk_inter_err      – pulses 1 for one cycle on inter-beat mismatch
    //   chk_err_sticky     – latches 1 on first error, never clears (reset only)
    //   chk_in_error       – level: high while current error window is open
    //   chk_beat_cnt       – accepted-beat counter
    //   chk_err_start_beat – beat number where current/last error window opened
    //   chk_fail_lane      – index [0..6] of first failing intra-beat lane pair
    //   chk_fail_got       – actual   value (natural order) at failure point
    //   chk_fail_exp       – expected value (natural order) at failure point
    //
    // Console: ONE print when error window opens, ONE when it closes.
    // =========================================================================

    // ---- combinational: un-swap each bus lane to natural (LE) order ----
    // Bus lane N occupies bits[255-N*32 -: 32].
    // Bus layout per lane: {b0, b1, b2, b3}  (b0 = LSB of original word at MSB of bus)
    // Natural order:       {b3, b2, b1, b0}
    logic [31:0] chk_us[0:7]  /* synthesis noprune */;  // un-swapped lanes

    always_comb begin
        // Lane 0: bits[255:224]
        chk_us[0] = {
            avst_source_data[231:224],
            avst_source_data[239:232],
            avst_source_data[247:240],
            avst_source_data[255:248]
        };
        // Lane 1: bits[223:192]
        chk_us[1] = {
            avst_source_data[199:192],
            avst_source_data[207:200],
            avst_source_data[215:208],
            avst_source_data[223:216]
        };
        // Lane 2: bits[191:160]
        chk_us[2] = {
            avst_source_data[167:160],
            avst_source_data[175:168],
            avst_source_data[183:176],
            avst_source_data[191:184]
        };
        // Lane 3: bits[159:128]
        chk_us[3] = {
            avst_source_data[135:128],
            avst_source_data[143:136],
            avst_source_data[151:144],
            avst_source_data[159:152]
        };
        // Lane 4: bits[127:96]
        chk_us[4] = {
            avst_source_data[103:96],
            avst_source_data[111:104],
            avst_source_data[119:112],
            avst_source_data[127:120]
        };
        // Lane 5: bits[95:64]
        chk_us[5] = {
            avst_source_data[71:64],
            avst_source_data[79:72],
            avst_source_data[87:80],
            avst_source_data[95:88]
        };
        // Lane 6: bits[63:32]
        chk_us[6] = {
            avst_source_data[39:32],
            avst_source_data[47:40],
            avst_source_data[55:48],
            avst_source_data[63:56]
        };
        // Lane 7: bits[31:0]
        chk_us[7] = {
            avst_source_data[7:0],
            avst_source_data[15:8],
            avst_source_data[23:16],
            avst_source_data[31:24]
        };
    end

    logic [31:0] chk_prev_us7;  // chk_us[7] of the previous accepted beat
    logic chk_has_prev;  // 1 once at least one beat has been accepted

    logic chk_intra_err  /* synthesis noprune */;
    logic chk_inter_err  /* synthesis noprune */;
    logic chk_err_sticky  /* synthesis noprune */;
    logic chk_in_error  /* synthesis noprune */;
    logic [31:0] chk_beat_cnt  /* synthesis noprune */;
    logic [31:0] chk_err_start_beat  /* synthesis noprune */;
    logic [2:0] chk_fail_lane  /* synthesis noprune */;  // [0..6]: failing intra pair; 7: inter
    logic [31:0] chk_fail_got  /* synthesis noprune */;
    logic [31:0] chk_fail_exp  /* synthesis noprune */;

    // ---- combinational: error detection in natural order ----
    logic chk_intra_err_c;
    logic chk_inter_err_c;
    logic chk_any_err_c;
    logic [2:0] chk_fail_lane_c;
    logic [31:0] chk_fail_got_c;
    logic [31:0] chk_fail_exp_c;

    always_comb begin
        chk_intra_err_c = 1'b0;
        chk_inter_err_c = 1'b0;
        chk_fail_lane_c = 3'd0;
        chk_fail_got_c = 32'h0;
        chk_fail_exp_c = 32'h0;
        if (avst_source_valid && avst_source_ready) begin
            // -- intra-beat: all lanes relative to lane 0 of same beat --
            if (chk_us[1] != chk_us[0] + 32'd1) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd0;
                chk_fail_got_c = chk_us[1];
                chk_fail_exp_c = chk_us[0] + 32'd1;
            end else if (chk_us[2] != chk_us[0] + 32'd2) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd1;
                chk_fail_got_c = chk_us[2];
                chk_fail_exp_c = chk_us[0] + 32'd2;
            end else if (chk_us[3] != chk_us[0] + 32'd3) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd2;
                chk_fail_got_c = chk_us[3];
                chk_fail_exp_c = chk_us[0] + 32'd3;
            end else if (chk_us[4] != chk_us[0] + 32'd4) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd3;
                chk_fail_got_c = chk_us[4];
                chk_fail_exp_c = chk_us[0] + 32'd4;
            end else if (chk_us[5] != chk_us[0] + 32'd5) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd4;
                chk_fail_got_c = chk_us[5];
                chk_fail_exp_c = chk_us[0] + 32'd5;
            end else if (chk_us[6] != chk_us[0] + 32'd6) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd5;
                chk_fail_got_c = chk_us[6];
                chk_fail_exp_c = chk_us[0] + 32'd6;
            end else if (chk_us[7] != chk_us[0] + 32'd7) begin
                chk_intra_err_c = 1'b1;
                chk_fail_lane_c = 3'd6;
                chk_fail_got_c = chk_us[7];
                chk_fail_exp_c = chk_us[0] + 32'd7;
            end
            // -- inter-beat: lane 0 of this beat == lane 7 of previous + 1 --
            if (chk_has_prev && (chk_us[0] != chk_prev_us7 + 32'd1)) chk_inter_err_c = 1'b1;
        end
        chk_any_err_c = chk_intra_err_c | chk_inter_err_c;
    end

    // ---- sequential: register flags, edge-triggered prints, beat counter ----
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            chk_prev_us7 <= 32'h0;
            chk_has_prev <= 1'b0;
            chk_intra_err <= 1'b0;
            chk_inter_err <= 1'b0;
            chk_err_sticky <= 1'b0;
            chk_in_error <= 1'b0;
            chk_beat_cnt <= 32'h0;
            chk_err_start_beat <= 32'h0;
            chk_fail_lane <= 3'd0;
            chk_fail_got <= 32'h0;
            chk_fail_exp <= 32'h0;
        end else begin
            chk_intra_err <= 1'b0;
            chk_inter_err <= 1'b0;

            if (avst_source_valid && avst_source_ready) begin
                chk_beat_cnt <= chk_beat_cnt + 32'd1;
                chk_intra_err <= chk_intra_err_c;
                chk_inter_err <= chk_inter_err_c;

                if (chk_any_err_c) begin
                    if (chk_intra_err_c) begin
                        chk_fail_lane <= chk_fail_lane_c;
                        chk_fail_got <= chk_fail_got_c;
                        chk_fail_exp <= chk_fail_exp_c;
                    end else begin
                        chk_fail_lane <= 3'd7;  // inter-beat marker
                        chk_fail_got <= chk_us[0];
                        chk_fail_exp <= chk_prev_us7 + 32'd1;
                    end
                    chk_err_sticky <= 1'b1;

                    if (!chk_in_error) begin
                        chk_in_error <= 1'b1;
                        chk_err_start_beat <= chk_beat_cnt;
                        if (chk_intra_err_c)
                            $error(
                                "[CCA CHK] ERROR START beat=%0d  intra pair[%0d] got=0x%08h exp=0x%08h  (lane0=0x%08h)",
                                chk_beat_cnt,
                                chk_fail_lane_c,
                                chk_fail_got_c,
                                chk_fail_exp_c,
                                chk_us[0]
                            );
                        else
                            $error(
                                "[CCA CHK] ERROR START beat=%0d  inter lane0 got=0x%08h exp=0x%08h  (prev_us7=0x%08h)",
                                chk_beat_cnt,
                                chk_us[0],
                                chk_prev_us7 + 32'd1,
                                chk_prev_us7
                            );
                    end
                end else begin
                    if (chk_in_error) begin
                        chk_in_error <= 1'b0;
                        $info(
                            "[CCA CHK] ERROR CLEARED beat=%0d  (error window: beat %0d .. beat %0d)",
                            chk_beat_cnt, chk_err_start_beat, chk_beat_cnt - 32'd1);
                    end
                end

                chk_prev_us7 <= chk_us[7];
                chk_has_prev <= 1'b1;
            end else begin
                // No accepted beat this cycle → clear inter-beat context so the
                // checker does not fire across transfer boundaries (e.g. iter N→N+1).
                chk_has_prev <= 1'b0;
            end
        end
    end  // assertion + error-flag block
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // synthesis translate_on
    // end of assertions based verification
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    // ============================================================================
endmodule
