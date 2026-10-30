# Set the default time format
set_time_format -unit ns -decimal_places 3

# Create shorthand names for clocks
set CLOCK_0 [get_clocks {u_cache_coh_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|u_sys_pll|iopll_0_outclk0}]
set CLOCK_1 [get_clocks {hps_internal_osc}]

# Set asynchronous clock groups between hps_internal_osc and system clocks
set_clock_groups -asynchronous -group $CLOCK_1 -group $CLOCK_0

# Use -no_synchronizer for the following intra-clock false paths
set_false_path -no_synchronizer -from [get_registers {u_cache_coh_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync|dreg[1]}] -to [get_registers {u_cache_coh_top|u_cca_fab_subsys|u_cca_subsys|rst_controller|alt_rst_sync_uq1|altera_reset_synchronizer_int_chain[1]}]

# --- Source 1: PLL lock signal (feeds system_reset -> fpga_reset_n_sync async clear) ---
# --- fpga_reset_n_sync: async clear pin, all sources ---
# better constraint in order to keep the async clear path being analyzed by the timing analyzer 
set_multicycle_path -to [get_registers {u_cache_coh_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync|dreg[*]}] -setup -end 2
set_multicycle_path -to [get_registers {u_cache_coh_top|u_shell_subsys|u_clks_and_rsts|clks_and_rsts|fpga_reset_n_sync|dreg[*]}] -hold -end 1
