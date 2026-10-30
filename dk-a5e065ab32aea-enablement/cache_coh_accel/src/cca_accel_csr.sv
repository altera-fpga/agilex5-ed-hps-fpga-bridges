// ****************************************************************************
//
//    SPDX-License-Identifier: MIT-0
//    SPDX-FileCopyrightText: Copyright (C) 2026 Altera Corporation
//
// ****************************************************************************
//    Cache Coherency Accelerator CSR Interface
// ****************************************************************************
//    This module decodes the AvMM bus transactions, implements the CSR 
//    register block and provides the control / status interface to the 
//    cache coherency accelerator core
// ****************************************************************************
`timescale 1 ps / 1 ps
// // // //  
`default_nettype none
// // // //  
module cca_accel_csr #(
) (
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Clock and reset
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    input  wire        sys_clk,                 // system clock
    input  wire        sys_rst,                 // active high reset, synchronized
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Avalon-MM CSR Slave Interface
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    output wire        avmm_csr_waitrequest,    // AvMM active high wait request
    input  wire        avmm_csr_write,          // AvMM active high write request
    input  wire [19:0] avmm_csr_address,        // AvMM address
    input  wire [ 3:0] avmm_csr_byteenable,     // AvMM byte enable
    input  wire [31:0] avmm_csr_writedata,      // AvMM write data
    input  wire        avmm_csr_read,           // AvMM active high read request
    output wire        avmm_csr_readdatavalid,  // AvMM active high read data valid
    output wire [31:0] avmm_csr_readdata,       // AvMM read data
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // System status inputs
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    input  wire        accel_busy,              // Accelerator busy status
    input  wire        accel_done,              // Accelerator operation complete
    input  wire        accel_error,             // Accelerator error flag
    input  wire        accel_alive,             // Accelerator alive flag
    input  wire [31:0] accel_ip_id,             // Accelerator IP ID
    input  wire [31:0] accel_ip_rev,            // Accelerator IP version
    input  wire [31:0] accel_stat_data,         // Accelerator status data
    input  wire [31:0] accel_snk_byte,          // Accelerator sink byte count
    input  wire [31:0] accel_src_byte,          // Accelerator source byte count
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // System control outputs
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    output wire        cca_reset,               // Full subsystem reset
    output wire [31:0] cca_ctrl_data            // Accelerator control data
);
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // AXI Response codes
    localparam logic [1:0] RESP_OKAY = 2'b00;
    localparam logic [1:0] RESP_SLVERR = 2'b10;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Internal register addresses (per REGISTER_MAP.md)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // FPGA Identification Registers
    localparam FPGA_ID = 16'h0000;  // ro - FPGA design identifier
    localparam FPGA_REVISION = 16'h0004;  // ro - FPGA revision number
    localparam RESERVED_08 = 16'h0008;  // ro - Reserved
    localparam RESERVED_0C = 16'h000C;  // ro - Reserved
    // Accelerator Control and Status Registers
    localparam ACCEL_CONTROL = 16'h0010;  // rw - Accelerator control
    localparam ACCEL_STATUS = 16'h0014;  // ro - Accelerator status
    localparam ACCEL_STAT_SNK = 16'h0018;  // ro - Accelerator sink byte counter status register
    localparam ACCEL_STAT_SRC = 16'h001C;  // ro - Accelerator source byte counter status register
    // Reserved Control and Status Registers
    localparam RESERVED_20 = 16'h0020;  // ro - Reserved : SYSTEM_CONTROL
    localparam RESERVED_24 = 16'h0024;  // ro - Reserved : SYSTEM_STATUS
    localparam RESERVED_28 = 16'h0028;  // ro - Reserved : SYSTEM_STATUS
    localparam RESERVED_2C = 16'h002C;  // ro - Reserved : SYSTEM_STATUS
    // Reserved Control and Status Registers
    localparam RESERVED_30 = 16'h0030;  // ro - Reserved : SYSTEM_CONTROL
    localparam RESERVED_34 = 16'h0034;  // ro - Reserved : SYSTEM_STATUS
    localparam RESERVED_38 = 16'h0038;  // ro - Reserved : SYSTEM_STATUS
    localparam RESERVED_3C = 16'h003C;  // ro - Reserved : SYSTEM_STATUS
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Debug registers (from csr_if_shim.sv)
    localparam ACCEL_DBG_TST_REG = 16'h0040;  // rw  - Debug test register
    localparam ACCEL_DBG_0_REG_DATA = 16'h0044;  // rw  - Debug data 0
    localparam ACCEL_DBG_1_REG_DATA = 16'h0048;  // rw  - Debug data 1
    localparam ACCEL_DBG_2_REG_DATA = 16'h004C;  // rw  - Debug data 2
    localparam ACCEL_DBG_3_REG_DATA = 16'h0050;  // rw  - Debug data 3
    localparam ACCEL_DBG_4_REG_DATA = 16'h0054;  // rw  - Debug data 4
    localparam ACCEL_DBG_5_REG_DATA = 16'h0058;  // rw  - Debug data 5
    localparam ACCEL_DBG_6_REG_DATA = 16'h005C;  // rw  - Debug data 6
    localparam ACCEL_DBG_7_REG_DATA = 16'h0060;  // rw  - Debug data 7
    localparam ACCEL_DBG_8_REG_DATA = 16'h0064;  // rw  - Debug data 8
    localparam ACCEL_DBG_9_REG_DATA = 16'h0068;  // rw  - Debug data 9
    localparam ACCEL_DBG_10_REG_DATA = 16'h006C;  // rw  - Debug data 10
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    localparam RSRVD_08_VAL = 32'h08080808;  // ro - Reserved
    localparam RSRVD_0C_VAL = 32'h0C0C0C0C;  // ro - Reserved
    localparam RSRVD_20_VAL = 32'h20202020;  // ro - Reserved
    localparam RSRVD_24_VAL = 32'h24242424;  // ro - Reserved
    localparam RSRVD_28_VAL = 32'h28282828;  // ro - Reserved
    localparam RSRVD_2C_VAL = 32'h2C2C2C2C;  // ro - Reserved
    localparam RSRVD_30_VAL = 32'h30303030;  // ro - Reserved
    localparam RSRVD_34_VAL = 32'h34343434;  // ro - Reserved
    localparam RSRVD_38_VAL = 32'h38383838;  // ro - Reserved
    localparam RSRVD_3C_VAL = 32'h3C3C3C3C;  // ro - Reserved
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Internal data path
    logic [3:0] wr_strb;
    logic [19:0] wr_addr;
    logic [19:0] rd_addr;
    logic [31:0] wr_data;
    logic [31:0] read_data;
    logic [31:0] rd_out_data;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Address and control signals   
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Control signals
    logic wr_transaction;
    logic wr_transaction_q;
    logic rd_transaction;
    logic rd_transaction_q  /* synthesis preserve */;
    logic access_addr_valid;
    logic acc_addr_valid  /* synthesis preserve */;
    logic csr_wait_req;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    logic rd_valid;
    logic rd_valid_q  /* synthesis preserve */;
    logic rd_valid_2q  /* synthesis preserve */;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	
    //    //    //    //    //
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    // Internal registers (matching REGISTER_MAP.md)
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	   
    logic [31:0] system_control_reg;
    logic [31:0] dma_control_reg;
    logic [31:0] accel_control_reg;
    logic [31:0] transfer_config_reg;
    logic [31:0] irq_ena_reg;
    logic [31:0] irq_mask_reg;
    logic [31:0] irq_clear_reg;
    // internal status registers   
    logic [31:0] accel_status_reg;
    logic [31:0] accel_status_snk_reg;
    logic [31:0] accel_status_src_reg;
    logic [31:0] dma_status_reg;
    logic [31:0] irq_status_reg;

    // 32 cycles = FIFO pipeline delay + inter-packet spacing   
    // internal debug registers   
    logic [31:0] dbg_tst_reg_data;
    logic [31:0] dbg_0_reg_data;  // debug register 0   
    logic [31:0] dbg_1_reg_data;  // debug register 1   
    logic [31:0] dbg_2_reg_data;  // debug register 2   
    logic [31:0] dbg_3_reg_data;  // debug register 3   
    logic [31:0] dbg_4_reg_data;  // debug register 4   
    logic [31:0] dbg_5_reg_data;  // debug register 5   
    logic [31:0] dbg_6_reg_data;  // debug register 6   
    logic [31:0] dbg_7_reg_data;  // debug register 7   
    logic [31:0] dbg_8_reg_data;  // debug register 8   
    logic [31:0] dbg_9_reg_data;  // debug register 9   
    logic [31:0] dbg_10_reg_data;  // debug register 10   

    // internal debug registers   
    logic accel_rst_bit0;
    logic accel_rst_bit0_q1  /* synthesis preserve */;
    logic accel_rst_bit0_q2  /* synthesis preserve */;
    logic sm_sys_rst;  // SM synch reset

    // ============================================================================
    // Address Validation
    // ============================================================================
    // Address validation (must be aligned and within range)   
    assign access_addr_valid = (avmm_csr_address < 20'h00100);  // Valid address range
    // errored transaction   
    // AvMM transaction detection   
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            wr_transaction <= 1'b0;
            rd_transaction <= 1'b0;
            acc_addr_valid <= 1'b0;
            csr_wait_req <= 1'b0;
        end else begin
            wr_transaction <= avmm_csr_write;
            rd_transaction <= avmm_csr_read;
            acc_addr_valid <= access_addr_valid;
            csr_wait_req <= 1'b0;
        end
    end
    // 1 clock delays for transaction timing and response generation
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            // Initialize all registers to 0   
            wr_transaction_q <= 1'b0;
            rd_transaction_q <= 1'b0;
            rd_valid <= 1'b0;
            rd_valid_q <= 1'b0;
        end else begin
            // identify the transaction type   
            wr_transaction_q <= wr_transaction;
            rd_transaction_q <= rd_transaction;
            // assign read valid signal   
            rd_valid <= rd_transaction_q;  // delay one clock cycle  
            rd_valid_q <= rd_valid;  // delay one clock cycle  
        end
    end
    // ============================================================================
    // Register Write Logic
    // ============================================================================
    // ==============================================
    // Wr Data & Strobe signals
    // ==============================================
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            wr_strb <= '0;
            wr_data <= '0;
            wr_addr <= '0;
            rd_addr <= '0;
        end else begin
            // latch the write address -- re-align the full address
            if (avmm_csr_write && !wr_transaction) wr_addr <= {avmm_csr_address[17:0], 2'b00};
            // latch the read address -- re-align the full address
            if (avmm_csr_read && !rd_transaction) rd_addr <= {avmm_csr_address[17:0], 2'b00};
            // latch the correct 32bit word from the data bus   
            if (avmm_csr_write && !wr_transaction) begin
                // strobe-based data latch needed for partial writes   
                wr_strb <= avmm_csr_byteenable[3:0];
                wr_data <= avmm_csr_writedata[31:0];
            end
        end
    end
    // ==============================================
    // Register Write Logic
    // ==============================================
    // write to registers
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            // default control reg value with RESET bit set
            accel_control_reg <= 32'h0000_8000;
            dbg_0_reg_data <= 32'h0;
            dbg_1_reg_data <= 32'h0;
            dbg_2_reg_data <= 32'h0;
            dbg_3_reg_data <= 32'h0;
            dbg_4_reg_data <= 32'h0;
            dbg_5_reg_data <= 32'h0;
            dbg_6_reg_data <= 32'h0;
            dbg_7_reg_data <= 32'h0;
            dbg_8_reg_data <= 32'h0;
            dbg_9_reg_data <= 32'h0;
            dbg_10_reg_data <= 32'h0;
            dbg_tst_reg_data <= 32'h0;
        end else begin
            // Auto-clear control bits (self-clearing)
            accel_control_reg[15] <= 1'b0;  // RESET auto-clear

            accel_control_reg[11] <= 1'b0;  // CNT_RST auto-clear
            accel_control_reg[10] <= 1'b0;  // ABORT auto-clear
            accel_control_reg[9] <= 1'b0;  // STOP auto-clear
            accel_control_reg[8] <= 1'b0;  // START auto-clear

            // decode write transaction
            if (wr_transaction_q && acc_addr_valid) begin
                case (wr_addr[15:0])
                    ACCEL_CONTROL: begin
                        if (wr_strb[0]) accel_control_reg[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) accel_control_reg[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) accel_control_reg[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) accel_control_reg[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_0_REG_DATA: begin
                        if (wr_strb[0]) dbg_0_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_0_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_0_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_0_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_1_REG_DATA: begin
                        if (wr_strb[0]) dbg_1_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_1_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_1_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_1_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_2_REG_DATA: begin
                        if (wr_strb[0]) dbg_2_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_2_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_2_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_2_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_3_REG_DATA: begin
                        if (wr_strb[0]) dbg_3_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_3_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_3_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_3_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_4_REG_DATA: begin
                        if (wr_strb[0]) dbg_4_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_4_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_4_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_4_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_5_REG_DATA: begin
                        if (wr_strb[0]) dbg_5_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_5_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_5_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_5_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_6_REG_DATA: begin
                        if (wr_strb[0]) dbg_6_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_6_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_6_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_6_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_7_REG_DATA: begin
                        if (wr_strb[0]) dbg_7_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_7_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_7_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_7_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_8_REG_DATA: begin
                        if (wr_strb[0]) dbg_8_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_8_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_8_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_8_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_9_REG_DATA: begin
                        if (wr_strb[0]) dbg_9_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_9_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_9_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_9_reg_data[31:24] <= wr_data[31:24];
                    end
                    ACCEL_DBG_10_REG_DATA: begin
                        if (wr_strb[0]) dbg_10_reg_data[7:0] <= wr_data[7:0];
                        if (wr_strb[1]) dbg_10_reg_data[15:8] <= wr_data[15:8];
                        if (wr_strb[2]) dbg_10_reg_data[23:16] <= wr_data[23:16];
                        if (wr_strb[3]) dbg_10_reg_data[31:24] <= wr_data[31:24];
                    end
                    default: begin
                        if (wr_strb[0]) dbg_tst_reg_data[7:0] <= wr_addr[7:0];
                        if (wr_strb[1]) dbg_tst_reg_data[15:8] <= wr_addr[15:8];
                        if (wr_strb[2]) dbg_tst_reg_data[23:16] <= wr_data[7:0];
                        if (wr_strb[3]) dbg_tst_reg_data[31:24] <= wr_data[15:8];
                    end
                endcase
            end
        end
    end
    //=========================================================================
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            accel_status_reg <= 32'h0;
            accel_status_snk_reg <= 32'h0;
            accel_status_src_reg <= 32'h0;
        end else begin
            accel_status_reg <= accel_stat_data;
            accel_status_snk_reg <= accel_snk_byte;
            accel_status_src_reg <= accel_src_byte;
        end
    end
    //=========================================================================
    // detect a change in command register
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            accel_rst_bit0 <= 1'b0;
            accel_rst_bit0_q1 <= 1'b1;
            accel_rst_bit0_q2 <= 1'b1;
            sm_sys_rst <= 1'b1;  // SM synch reset
        end else begin
            // Capture the reset bit 0
            accel_rst_bit0 <= accel_control_reg[15];  // reset bit 15
            accel_rst_bit0_q1 <= accel_rst_bit0;
            accel_rst_bit0_q2 <= accel_rst_bit0_q1;
            sm_sys_rst <= (accel_rst_bit0    || 
                           accel_rst_bit0_q1 || 
                           accel_rst_bit0_q2);  // extend pulse
        end
    end
    // ============================================================================
    // Register Read Logic
    // ============================================================================
    always_ff @(posedge sys_clk) begin
        if (sys_rst) begin
            rd_out_data <= 32'h0;
        end else begin
            if (rd_transaction_q) begin
                case (rd_addr[15:0])
                    FPGA_ID: rd_out_data <= accel_ip_id;
                    FPGA_REVISION: rd_out_data <= accel_ip_rev;
                    RESERVED_08: rd_out_data <= RSRVD_08_VAL;
                    RESERVED_0C: rd_out_data <= RSRVD_0C_VAL;
                    ACCEL_CONTROL: rd_out_data <= accel_control_reg;
                    ACCEL_STATUS: rd_out_data <= accel_status_reg;
                    ACCEL_STAT_SNK: rd_out_data <= accel_status_snk_reg;
                    ACCEL_STAT_SRC: rd_out_data <= accel_status_src_reg;
                    RESERVED_20: rd_out_data <= RSRVD_20_VAL;
                    RESERVED_24: rd_out_data <= RSRVD_24_VAL;
                    RESERVED_28: rd_out_data <= RSRVD_28_VAL;
                    RESERVED_2C: rd_out_data <= RSRVD_2C_VAL;
                    RESERVED_30: rd_out_data <= RSRVD_30_VAL;
                    RESERVED_34: rd_out_data <= RSRVD_34_VAL;
                    RESERVED_38: rd_out_data <= RSRVD_38_VAL;
                    RESERVED_3C: rd_out_data <= RSRVD_3C_VAL;
                    ACCEL_DBG_TST_REG: rd_out_data <= dbg_tst_reg_data;
                    ACCEL_DBG_0_REG_DATA: rd_out_data <= dbg_0_reg_data;
                    ACCEL_DBG_1_REG_DATA: rd_out_data <= dbg_1_reg_data;
                    ACCEL_DBG_2_REG_DATA: rd_out_data <= dbg_2_reg_data;
                    ACCEL_DBG_3_REG_DATA: rd_out_data <= dbg_3_reg_data;
                    ACCEL_DBG_4_REG_DATA: rd_out_data <= dbg_4_reg_data;
                    ACCEL_DBG_5_REG_DATA: rd_out_data <= dbg_5_reg_data;
                    ACCEL_DBG_6_REG_DATA: rd_out_data <= dbg_6_reg_data;
                    ACCEL_DBG_7_REG_DATA: rd_out_data <= dbg_7_reg_data;
                    ACCEL_DBG_8_REG_DATA: rd_out_data <= dbg_8_reg_data;
                    ACCEL_DBG_9_REG_DATA: rd_out_data <= dbg_9_reg_data;
                    ACCEL_DBG_10_REG_DATA: rd_out_data <= dbg_10_reg_data;
                    default: rd_out_data <= 32'hDEADBEEF;
                endcase
            end
        end
    end
    // ============================================================================
    // Output Assignments (per REGISTER_MAP.md bit definitions)
    // ============================================================================
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	
    assign avmm_csr_waitrequest = csr_wait_req;
    assign avmm_csr_readdatavalid = rd_valid_q;
    assign avmm_csr_readdata = rd_out_data;
    // -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --	
    // System Control Register (0x10)
    assign cca_reset = sm_sys_rst;  // Bit 15: ACCEL_RESET
    // Accelerator Control Register (0x18)
    assign cca_ctrl_data = accel_control_reg;  // Accelerator control data
    // ============================================================================
endmodule
