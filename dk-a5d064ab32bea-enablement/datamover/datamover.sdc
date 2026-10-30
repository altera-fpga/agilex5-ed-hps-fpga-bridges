# Set the default time format
set_time_format -unit ns -decimal_places 3

# Create shorthand names for the PLL-generated clocks
set USBPR_CLOCK   [get_clocks {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0_outclk0}]
set H2F_CLOCK     [get_clocks {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0_outclk1}]
set LWH2F_CLOCK   [get_clocks {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0_outclk2}]
set F2H_CLOCK     [get_clocks {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0_outclk3}]
set F2SDRAM_CLOCK [get_clocks {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0_outclk4}]

# Set asynchronous clock groups between hps_internal_osc and each PLL-generated clock domain.
set_clock_groups -asynchronous \
    -group [get_clocks {hps_internal_osc}] \
    -group $USBPR_CLOCK \
    -group $H2F_CLOCK \
    -group $LWH2F_CLOCK \
    -group $F2H_CLOCK \
    -group $F2SDRAM_CLOCK

# Use -no_synchronizer for the following intra-clock false paths
set_false_path -no_synchronizer -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_1|dreg[1]}] -to [get_registers {u_datamover_top|u_fabric_subsys|rst_controller_002|alt_rst_sync_uq1|altera_reset_synchronizer_int_chain[1]}]
set_false_path -no_synchronizer -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_2|dreg[1]}] -to [get_registers {u_datamover_top|u_fabric_subsys|rst_controller_003|alt_rst_sync_uq1|altera_reset_synchronizer_int_chain[1]}]
set_false_path -no_synchronizer -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_3|dreg[1]}] -to [get_registers {u_datamover_top|u_fabric_subsys|rst_controller|alt_rst_sync_uq1|altera_reset_synchronizer_int_chain[1]}]
set_false_path -no_synchronizer -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_4|dreg[1]}] -to [get_registers {u_datamover_top|u_fabric_subsys|rst_controller_001|alt_rst_sync_uq1|altera_reset_synchronizer_int_chain[1]}]

# Set false path for reset synchronizer chains
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_0|dreg[0]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_0|dreg[1]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_1|dreg[0]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_1|dreg[1]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_2|dreg[0]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_2|dreg[1]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_3|dreg[0]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_3|dreg[1]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_4|dreg[0]}]
set_false_path -from [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0|tennm_ph2_iopll~pll_ctrl_reg}] -to [get_registers {u_datamover_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync_4|dreg[1]}]