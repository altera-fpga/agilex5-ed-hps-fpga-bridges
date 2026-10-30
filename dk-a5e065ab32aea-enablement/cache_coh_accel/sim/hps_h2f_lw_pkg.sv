// Defines constants for HPS to FPGA LW bridge interface
package hps_h2f_lw_pkg;

    // LWH2F (Lightweight HPS-to-FPGA) bridge parameters (cache_coh_accel design)
    localparam LWH2F_DATA_WIDTH = 32;
    localparam LWH2F_ADDR_WIDTH = 29;
    localparam LWH2F_SYMBOLS_PER_WORD = LWH2F_DATA_WIDTH / 8;

    localparam LWH2F_SYSID_BASE_ADDR = 32'h0001_0000;
    localparam LWH2F_SYSID_SIZE = 32'h0000_0010;
    localparam LWH2F_SYSID_VALUE = 32'h0000_0001;

    // Peripheral base addresses (relative to LWH2F bridge base 0x2000_0000)
    localparam LWH2F_USB_PHY_RECONFIG_BASE_ADDR = 32'h0000_0000;  // 0x2000_0000
    localparam LWH2F_HPS2FPGA_MSGDMA_BASE_ADDR = 32'h0100_0000;  // 0x2100_0000
    localparam LWH2F_FPGA2HPS_MSGDMA_BASE_ADDR = 32'h0200_0000;  // 0x2200_0000
    localparam LWH2F_ACCEL_CSR_BASE_ADDR = 32'h0040_0000;  // 0x2040_0000

    // Additional params for compatibility with datamover design
    localparam LWH2F_BUTTON_PIO_BASE_ADDR = 32'h0001_0060;
    localparam LWH2F_BUTTON_PIO_SIZE = 32'h0000_0010;
    localparam LWH2F_DIPSW_PIO_BASE_ADDR = 32'h0001_0070;
    localparam LWH2F_DIPSW_PIO_SIZE = 32'h0000_0010;
    localparam LWH2F_LED_PIO_BASE_ADDR = 32'h0001_0080;
    localparam LWH2F_LED_PIO_SIZE = 32'h0000_0010;

    // Sizes (from REGISTER_MAP.md)
    localparam LWH2F_USB_PHY_RECONFIG_SIZE = 32'h0000_1000;  // 4 KB
    localparam LWH2F_HPS2FPGA_MSGDMA_SIZE = 32'h0010_0000;  // 1 MB
    localparam LWH2F_FPGA2HPS_MSGDMA_SIZE = 32'h0010_0000;  // 1 MB
    localparam LWH2F_ACCEL_CSR_SIZE = 32'h0010_0000;  // 1 MB
    // =============================================================================
    // Peripheral base addresses (relative to LWH2F bridge base 0x2000_0000)
    // =============================================================================
    //   //   //
    // =============================================================================
    // Accelerator CSR Register Absolute Addresses (base 0x2040_0000 = LWH2F 0x2000_0000 + 0x0040_0000)
    // =============================================================================
    localparam ACCEL_CSR_FPGA_ID_ADDR = 32'h2040_0000;  // FPGA ID
    localparam ACCEL_CSR_FPGA_REV_ADDR = 32'h2040_0004;  // FPGA Revision
    localparam ACCEL_CSR_ACCEL_CTRL_ADDR = 32'h2040_0010;  // Accelerator Control (ACCEL_CONTROL)
    localparam ACCEL_CSR_ACCEL_STAT_ADDR = 32'h2040_0014;  // Accelerator Status  (ACCEL_STATUS)
    localparam ACCEL_CSR_STAT_SNK_ADDR   = 32'h2040_0018;  // Sink byte counter status (ACCEL_STAT_SNK)
    localparam ACCEL_CSR_STAT_SRC_ADDR   = 32'h2040_001C;  // Source byte counter status (ACCEL_STAT_SRC)
    // Accelerator CSR Debug Register Absolute Addresses
    localparam ACCEL_CSR_DBG_TST_REG_OFFSET = 32'h2040_0040;  // Debug Test Register
    localparam ACCEL_CSR_DBG_0_OFFSET = 32'h2040_0044;  // Debug Data 0
    localparam ACCEL_CSR_DBG_1_OFFSET = 32'h2040_0048;  // Debug Data 1
    localparam ACCEL_CSR_DBG_2_OFFSET = 32'h2040_004C;  // Debug Data 2
    localparam ACCEL_CSR_DBG_3_OFFSET = 32'h2040_0050;  // Debug Data 3
    localparam ACCEL_CSR_DBG_4_OFFSET = 32'h2040_0054;  // Debug Data 4
    localparam ACCEL_CSR_DBG_5_OFFSET = 32'h2040_0058;  // Debug Data 5
    localparam ACCEL_CSR_DBG_6_OFFSET = 32'h2040_005C;  // Debug Data 6
    localparam ACCEL_CSR_DBG_7_OFFSET = 32'h2040_0060;  // Debug Data 7
    localparam ACCEL_CSR_DBG_8_OFFSET = 32'h2040_0064;  // Debug Data 8
    localparam ACCEL_CSR_DBG_9_OFFSET = 32'h2040_0068;  // Debug Data 9
    // Accelerator CSR Debug Register test values
    localparam ACCEL_CSR_DBG_0_VALUE = 32'h0102_0304;  // Debug Data 0
    localparam ACCEL_CSR_DBG_1_VALUE = 32'h0506_0708;  // Debug Data 1
    localparam ACCEL_CSR_DBG_2_VALUE = 32'h090A_0B0C;  // Debug Data 2
    localparam ACCEL_CSR_DBG_3_VALUE = 32'h0D0E_0F10;  // Debug Data 3
    localparam ACCEL_CSR_DBG_4_VALUE = 32'h1112_1314;  // Debug Data 4
    localparam ACCEL_CSR_DBG_5_VALUE = 32'h1516_1718;  // Debug Data 5
    localparam ACCEL_CSR_DBG_6_VALUE = 32'h191A_1B1C;  // Debug Data 6
    localparam ACCEL_CSR_DBG_7_VALUE = 32'h1D1E_1F20;  // Debug Data 7
    localparam ACCEL_CSR_DBG_8_VALUE = 32'h2122_2324;  // Debug Data 8
    localparam ACCEL_CSR_DBG_9_VALUE = 32'h2526_2728;  // Debug Data 9
    // Full address for debug registers: LWH2F_ACCEL_CSR_BASE_ADDR + offset
    // Example usage in testbench:
    //      wr_addr = LWH2F_ACCEL_CSR_BASE_ADDR + ACCEL_CSR_DBG_TST_REG_OFFSET;
    //      rd_addr = LWH2F_ACCEL_CSR_BASE_ADDR + ACCEL_CSR_DBG_0_OFFSET;
    // =============================================================================
    // FPGA2HPS msgDMA Subsystem (LWH2F Base: 0x2200_0000)
    // =============================================================================
    // Descriptor OCM 
    localparam logic [31:0] F2H_DESC_OCM_BASE = 32'h2200_0000;  // Descriptor OCM base
    // Write Prefetcher CSR 
    localparam logic [31:0] F2H_WR_PF_CONTROL = 32'h2200_4000;  // Write PF Control
    localparam logic [31:0] F2H_WR_PF_NEXT_DESC_LO = 32'h2200_4004;  // Write PF Next Desc Low
    localparam logic [31:0] F2H_WR_PF_NEXT_DESC_HI = 32'h2200_4008;  // Write PF Next Desc High
    localparam logic [31:0] F2H_WR_PF_STATUS = 32'h2200_4010;  // Write PF Status
    localparam logic [31:0] F2H_WR_PF_DESC_POLL_FREQ = 32'h2200_4014;  // Write PF Poll Freq
    // Read Prefetcher CSR 
    localparam logic [31:0] F2H_RD_PF_CONTROL = 32'h2200_4020;  // Read PF Control
    localparam logic [31:0] F2H_RD_PF_NEXT_DESC_LO = 32'h2200_4024;  // Read PF Next Desc Low
    localparam logic [31:0] F2H_RD_PF_NEXT_DESC_HI = 32'h2200_4028;  // Read PF Next Desc High
    localparam logic [31:0] F2H_RD_PF_STATUS = 32'h2200_4030;  // Read PF Status
    localparam logic [31:0] F2H_RD_PF_DESC_POLL_FREQ = 32'h2200_4034;  // Read PF Poll Freq
    // Write Dispatcher CSR 
    localparam logic [31:0] F2H_WR_DISP_STATUS = 32'h2200_4040;  // Write DISP Status
    localparam logic [31:0] F2H_WR_DISP_CONTROL = 32'h2200_4044;  // Write DISP Control
    localparam logic [31:0] F2H_WR_DISP_FILL_LEVEL = 32'h2200_4048;  // Write DISP Fill Level
    localparam logic [31:0] F2H_WR_DISP_RSP_FILL_LEVEL = 32'h2200_404C;  // Write DISP Rsp Fill
    localparam logic [31:0] F2H_WR_DISP_SEQ_NUMBER = 32'h2200_4050;  // Write DISP Seq Num
    // Read Dispatcher CSR 
    localparam logic [31:0] F2H_RD_DISP_STATUS = 32'h2200_4060;  // Read DISP Status
    localparam logic [31:0] F2H_RD_DISP_CONTROL = 32'h2200_4064;  // Read DISP Control
    localparam logic [31:0] F2H_RD_DISP_FILL_LEVEL = 32'h2200_4068;  // Read DISP Fill Level
    localparam logic [31:0] F2H_RD_DISP_RSP_FILL_LEVEL = 32'h2200_406C;  // Read DISP Rsp Fill
    localparam logic [31:0] F2H_RD_DISP_SEQ_NUMBER = 32'h2200_4070;  // Read DISP Seq Num
    // =============================================================================
    // msgDMA Register Bit Masks
    // =============================================================================
    // Prefetcher Control bits 
    localparam logic [31:0] PF_CTRL_RUN = 32'h0000_0001;  // bit 0
    localparam logic [31:0] PF_CTRL_PARK = 32'h0000_0002;  // bit 1
    localparam logic [31:0] PF_CTRL_RESET = 32'h0000_0004;  // bit 2
    localparam logic [31:0] PF_CTRL_IRQ_ENABLE = 32'h0000_0008;  // bit 3
    localparam logic [31:0] PF_CTRL_DESC_POLL_EN = 32'h0000_0010;  // bit 4
    localparam logic [31:0] PF_CTRL_GLOBAL_IRQ_MASK = 32'h0000_0020;  // bit 5
    localparam logic [31:0] PF_CTRL_STOP_DESC = 32'h0000_0040;  // bit 6
    // Prefetcher Status bits 
    localparam logic [31:0] PF_STAT_IRQ = 32'h0000_0001;  // bit 0
    localparam logic [31:0] PF_STAT_BUSY = 32'h0000_0002;  // bit 1
    localparam logic [31:0] PF_STAT_DESC_BUF_EMPTY = 32'h0000_0004;  // bit 2
    localparam logic [31:0] PF_STAT_DESC_BUF_FULL = 32'h0000_0008;  // bit 3
    localparam logic [31:0] PF_STAT_RSP_BUF_EMPTY = 32'h0000_0010;  // bit 4
    localparam logic [31:0] PF_STAT_RSP_BUF_FULL = 32'h0000_0020;  // bit 5
    localparam logic [31:0] PF_STAT_STOPPED = 32'h0000_0040;  // bit 6
    // Dispatcher Control bits 
    localparam logic [31:0] DISP_CTRL_STOP = 32'h0000_0001;  // bit 0
    localparam logic [31:0] DISP_CTRL_RESET = 32'h0000_0002;  // bit 1
    localparam logic [31:0] DISP_CTRL_STOP_ON_ERROR = 32'h0000_0004;  // bit 2
    localparam logic [31:0] DISP_CTRL_IRQ_ON_ERROR = 32'h0000_0008;  // bit 3
    // Dispatcher Status bits 
    localparam logic [31:0] DISP_STAT_BUSY = 32'h0000_0001;  // bit 0
    localparam logic [31:0] DISP_STAT_DESC_BUF_EMPTY = 32'h0000_0002;  // bit 1
    localparam logic [31:0] DISP_STAT_DESC_BUF_FULL = 32'h0000_0004;  // bit 2
    localparam logic [31:0] DISP_STAT_RSP_BUF_EMPTY = 32'h0000_0008;  // bit 3
    localparam logic [31:0] DISP_STAT_RSP_BUF_FULL = 32'h0000_0010;  // bit 4
    localparam logic [31:0] DISP_STAT_STOPPED = 32'h0000_0020;  // bit 5
    localparam logic [31:0] DISP_STAT_RESETTING = 32'h0000_0040;  // bit 6
    localparam logic [31:0] DISP_STAT_STOPPED_EARLY = 32'h0000_0080;  // bit 7
    localparam logic [31:0] DISP_STAT_IRQ = 32'h0000_0100;  // bit 8
    // Descriptor Control bits 
    localparam logic [31:0] DESC_CTRL_OWNED_BY_HW = 32'h4000_0000;  // bit 30
    localparam logic [31:0] DESC_CTRL_WAIT_FOR_WRITE = 32'h0200_0000;  // bit 25
    localparam logic [31:0] DESC_CTRL_EARLY_DONE = 32'h0100_0000;  // bit 24
    localparam logic [31:0] DESC_CTRL_IRQ_ON_COMPLETE = 32'h0000_4000;  // bit 14
    localparam logic [31:0] DESC_CTRL_EARLY_TERM = 32'h0000_2000;  // bit 13
    // Descriptor Control composite values 
    localparam logic [31:0] DESC_CTRL_MID_CHAIN = 32'h4100_0000;  // OWNED_BY_HW | EARLY_DONE
    localparam logic [31:0] DESC_CTRL_LAST              = 32'h4200_4000; // OWNED_BY_HW | WAIT_FOR_WRITE | IRQ_ON_COMPLETE
    localparam logic [31:0] DESC_CTRL_NULL = 32'h0000_0000;  // Null terminator
    // Expected reset values 
    localparam logic [31:0] PF_CTRL_RESET_VAL = 32'h0000_0000;  // PF Control reset
    localparam logic [31:0] PF_STAT_RESET_VAL = 32'h0000_0004;  // PF Status reset (DESC_BUF_EMPTY)
    localparam logic [31:0] DISP_CTRL_RESET_VAL = 32'h0000_0000;  // DISP Control reset
    localparam logic [31:0] DISP_STAT_RESET_VAL         = 32'h0000_0002; // DISP Status reset (DESC_BUF_EMPTY)
    // =============================================================================
    // HPS2FPGA msgDMA Subsystem (LWH2F Base: 0x2300_0000)
    // =============================================================================
    // Descriptor OCM 
    localparam logic [31:0] H2F_DESC_OCM_BASE = 32'h2300_0000;  // Descriptor OCM base
    // Write Prefetcher CSR 
    localparam logic [31:0] H2F_WR_PF_CONTROL = 32'h2300_4000;  // Write PF Control
    localparam logic [31:0] H2F_WR_PF_NEXT_DESC_LO = 32'h2300_4004;  // Write PF Next Desc Low
    localparam logic [31:0] H2F_WR_PF_NEXT_DESC_HI = 32'h2300_4008;  // Write PF Next Desc High
    localparam logic [31:0] H2F_WR_PF_STATUS = 32'h2300_4010;  // Write PF Status
    localparam logic [31:0] H2F_WR_PF_DESC_POLL_FREQ = 32'h2300_4014;  // Write PF Poll Freq
    // Read Prefetcher CSR 
    localparam logic [31:0] H2F_RD_PF_CONTROL = 32'h2300_4020;  // Read PF Control
    localparam logic [31:0] H2F_RD_PF_NEXT_DESC_LO = 32'h2300_4024;  // Read PF Next Desc Low
    localparam logic [31:0] H2F_RD_PF_NEXT_DESC_HI = 32'h2300_4028;  // Read PF Next Desc High
    localparam logic [31:0] H2F_RD_PF_STATUS = 32'h2300_4030;  // Read PF Status
    localparam logic [31:0] H2F_RD_PF_DESC_POLL_FREQ = 32'h2300_4034;  // Read PF Poll Freq
    // Write Dispatcher CSR 
    localparam logic [31:0] H2F_WR_DISP_STATUS = 32'h2300_4040;  // Write DISP Status
    localparam logic [31:0] H2F_WR_DISP_CONTROL = 32'h2300_4044;  // Write DISP Control
    localparam logic [31:0] H2F_WR_DISP_FILL_LEVEL = 32'h2300_4048;  // Write DISP Fill Level
    localparam logic [31:0] H2F_WR_DISP_RSP_FILL_LEVEL = 32'h2300_404C;  // Write DISP Rsp Fill
    localparam logic [31:0] H2F_WR_DISP_SEQ_NUMBER = 32'h2300_4050;  // Write DISP Seq Num
    // Read Dispatcher CSR 
    localparam logic [31:0] H2F_RD_DISP_STATUS = 32'h2300_4060;  // Read DISP Status
    localparam logic [31:0] H2F_RD_DISP_CONTROL = 32'h2300_4064;  // Read DISP Control
    localparam logic [31:0] H2F_RD_DISP_FILL_LEVEL = 32'h2300_4068;  // Read DISP Fill Level
    localparam logic [31:0] H2F_RD_DISP_RSP_FILL_LEVEL = 32'h2300_406C;  // Read DISP Rsp Fill
    localparam logic [31:0] H2F_RD_DISP_SEQ_NUMBER = 32'h2300_4070;  // Read DISP Seq Num
    // =============================================================================
endpackage
