#
# SPDX-License-Identifier: MIT-0
# SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
#

# 
# request TCL package from ACDS 25.3.1
# 
package require -exact qsys 25.3.1


# 
# module clocks_and_resets
# 
set_module_property DESCRIPTION ""
set_module_property NAME u_clocks_and_resets
set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property BSP_CPU false
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME clocks_and_resets
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true

# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH generate_fileset
set_fileset_property QUARTUS_SYNTH TOP_LEVEL clocks_and_resets
 
add_fileset SIM_VERILOG SIM_VERILOG generate_fileset
set_fileset_property SIM_VERILOG TOP_LEVEL clocks_and_resets

proc generate_fileset {entity_name} {
add_fileset_file clocks_and_resets.sv SYSTEM_VERILOG PATH ../src/clocks_and_resets.sv TOP_LEVEL_FILE
}

# 
# parameters
# 


# 
# display items
# 


# 
# connection point fpga_clk
# 
add_interface fpga_clk clock end
set_interface_property fpga_clk ENABLED true
set_interface_property fpga_clk EXPORT_OF ""
set_interface_property fpga_clk PORT_NAME_MAP ""
set_interface_property fpga_clk CMSIS_SVD_VARIABLES ""
set_interface_property fpga_clk SVD_ADDRESS_GROUP ""
set_interface_property fpga_clk IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property fpga_clk SV_INTERFACE_TYPE ""
set_interface_property fpga_clk SV_INTERFACE_MODPORT_TYPE ""

add_interface_port fpga_clk fpga_clk clk Input 1


# 
# connection point fpga_reset_n
# 
add_interface fpga_reset_n reset end
set_interface_property fpga_reset_n associatedClock ""
set_interface_property fpga_reset_n synchronousEdges NONE
set_interface_property fpga_reset_n ENABLED true
set_interface_property fpga_reset_n EXPORT_OF ""
set_interface_property fpga_reset_n PORT_NAME_MAP ""
set_interface_property fpga_reset_n CMSIS_SVD_VARIABLES ""
set_interface_property fpga_reset_n SVD_ADDRESS_GROUP ""
set_interface_property fpga_reset_n IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property fpga_reset_n SV_INTERFACE_TYPE ""
set_interface_property fpga_reset_n SV_INTERFACE_MODPORT_TYPE ""

add_interface_port fpga_reset_n fpga_reset_n reset_n Input 1


# 
# connection point h2f_reset_in
# 
add_interface h2f_reset_in reset end
set_interface_property h2f_reset_in associatedClock ""
set_interface_property h2f_reset_in synchronousEdges NONE
set_interface_property h2f_reset_in ENABLED true
set_interface_property h2f_reset_in EXPORT_OF ""
set_interface_property h2f_reset_in PORT_NAME_MAP ""
set_interface_property h2f_reset_in CMSIS_SVD_VARIABLES ""
set_interface_property h2f_reset_in SVD_ADDRESS_GROUP ""
set_interface_property h2f_reset_in IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property h2f_reset_in SV_INTERFACE_TYPE ""
set_interface_property h2f_reset_in SV_INTERFACE_MODPORT_TYPE ""

add_interface_port h2f_reset_in h2f_reset_in reset Input 1


# 
# connection point usbpr_clock
# 
add_interface usbpr_clock clock start
set_interface_property usbpr_clock associatedDirectClock ""
set_interface_property usbpr_clock ENABLED true
set_interface_property usbpr_clock EXPORT_OF ""
set_interface_property usbpr_clock PORT_NAME_MAP ""
set_interface_property usbpr_clock CMSIS_SVD_VARIABLES ""
set_interface_property usbpr_clock SVD_ADDRESS_GROUP ""
set_interface_property usbpr_clock IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property usbpr_clock SV_INTERFACE_TYPE ""
set_interface_property usbpr_clock SV_INTERFACE_MODPORT_TYPE ""

add_interface_port usbpr_clock usbpr_clock clk Output 1


# 
# connection point usbpr_reset_n
# 
add_interface usbpr_reset_n reset start
set_interface_property usbpr_reset_n associatedClock usbpr_clock
set_interface_property usbpr_reset_n associatedDirectReset ""
set_interface_property usbpr_reset_n associatedResetSinks h2f_reset_in
set_interface_property usbpr_reset_n synchronousEdges DEASSERT
set_interface_property usbpr_reset_n ENABLED true
set_interface_property usbpr_reset_n EXPORT_OF ""
set_interface_property usbpr_reset_n PORT_NAME_MAP ""
set_interface_property usbpr_reset_n CMSIS_SVD_VARIABLES ""
set_interface_property usbpr_reset_n SVD_ADDRESS_GROUP ""
set_interface_property usbpr_reset_n IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property usbpr_reset_n SV_INTERFACE_TYPE ""
set_interface_property usbpr_reset_n SV_INTERFACE_MODPORT_TYPE ""

add_interface_port usbpr_reset_n usbpr_reset_n reset_n Output 1


# 
# connection point h2f_clock
# 
add_interface h2f_clock clock start
set_interface_property h2f_clock associatedDirectClock ""
set_interface_property h2f_clock ENABLED true
set_interface_property h2f_clock EXPORT_OF ""
set_interface_property h2f_clock PORT_NAME_MAP ""
set_interface_property h2f_clock CMSIS_SVD_VARIABLES ""
set_interface_property h2f_clock SVD_ADDRESS_GROUP ""
set_interface_property h2f_clock IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property h2f_clock SV_INTERFACE_TYPE ""
set_interface_property h2f_clock SV_INTERFACE_MODPORT_TYPE ""

add_interface_port h2f_clock h2f_clock clk Output 1


# 
# connection point h2f_reset_n
# 
add_interface h2f_reset_n reset start
set_interface_property h2f_reset_n associatedClock h2f_clock
set_interface_property h2f_reset_n associatedDirectReset ""
set_interface_property h2f_reset_n associatedResetSinks h2f_reset_in
set_interface_property h2f_reset_n synchronousEdges DEASSERT
set_interface_property h2f_reset_n ENABLED true
set_interface_property h2f_reset_n EXPORT_OF ""
set_interface_property h2f_reset_n PORT_NAME_MAP ""
set_interface_property h2f_reset_n CMSIS_SVD_VARIABLES ""
set_interface_property h2f_reset_n SVD_ADDRESS_GROUP ""
set_interface_property h2f_reset_n IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property h2f_reset_n SV_INTERFACE_TYPE ""
set_interface_property h2f_reset_n SV_INTERFACE_MODPORT_TYPE ""

add_interface_port h2f_reset_n h2f_reset_n reset_n Output 1


# 
# connection point lwh2f_clock
# 
add_interface lwh2f_clock clock start
set_interface_property lwh2f_clock associatedDirectClock ""
set_interface_property lwh2f_clock ENABLED true
set_interface_property lwh2f_clock EXPORT_OF ""
set_interface_property lwh2f_clock PORT_NAME_MAP ""
set_interface_property lwh2f_clock CMSIS_SVD_VARIABLES ""
set_interface_property lwh2f_clock SVD_ADDRESS_GROUP ""
set_interface_property lwh2f_clock IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property lwh2f_clock SV_INTERFACE_TYPE ""
set_interface_property lwh2f_clock SV_INTERFACE_MODPORT_TYPE ""

add_interface_port lwh2f_clock lwh2f_clock clk Output 1


# 
# connection point lwh2f_reset_n
# 
add_interface lwh2f_reset_n reset start
set_interface_property lwh2f_reset_n associatedClock lwh2f_clock
set_interface_property lwh2f_reset_n associatedDirectReset ""
set_interface_property lwh2f_reset_n associatedResetSinks h2f_reset_in
set_interface_property lwh2f_reset_n synchronousEdges DEASSERT
set_interface_property lwh2f_reset_n ENABLED true
set_interface_property lwh2f_reset_n EXPORT_OF ""
set_interface_property lwh2f_reset_n PORT_NAME_MAP ""
set_interface_property lwh2f_reset_n CMSIS_SVD_VARIABLES ""
set_interface_property lwh2f_reset_n SVD_ADDRESS_GROUP ""
set_interface_property lwh2f_reset_n IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property lwh2f_reset_n SV_INTERFACE_TYPE ""
set_interface_property lwh2f_reset_n SV_INTERFACE_MODPORT_TYPE ""

add_interface_port lwh2f_reset_n lwh2f_reset_n reset_n Output 1


# 
# connection point f2h_clock
# 
add_interface f2h_clock clock start
set_interface_property f2h_clock associatedDirectClock ""
set_interface_property f2h_clock ENABLED true
set_interface_property f2h_clock EXPORT_OF ""
set_interface_property f2h_clock PORT_NAME_MAP ""
set_interface_property f2h_clock CMSIS_SVD_VARIABLES ""
set_interface_property f2h_clock SVD_ADDRESS_GROUP ""
set_interface_property f2h_clock IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property f2h_clock SV_INTERFACE_TYPE ""
set_interface_property f2h_clock SV_INTERFACE_MODPORT_TYPE ""

add_interface_port f2h_clock f2h_clock clk Output 1


# 
# connection point f2h_reset_n
# 
add_interface f2h_reset_n reset start
set_interface_property f2h_reset_n associatedClock f2h_clock
set_interface_property f2h_reset_n associatedDirectReset ""
set_interface_property f2h_reset_n associatedResetSinks h2f_reset_in
set_interface_property f2h_reset_n synchronousEdges DEASSERT
set_interface_property f2h_reset_n ENABLED true
set_interface_property f2h_reset_n EXPORT_OF ""
set_interface_property f2h_reset_n PORT_NAME_MAP ""
set_interface_property f2h_reset_n CMSIS_SVD_VARIABLES ""
set_interface_property f2h_reset_n SVD_ADDRESS_GROUP ""
set_interface_property f2h_reset_n IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property f2h_reset_n SV_INTERFACE_TYPE ""
set_interface_property f2h_reset_n SV_INTERFACE_MODPORT_TYPE ""

add_interface_port f2h_reset_n f2h_reset_n reset_n Output 1


# 
# connection point f2sdram_clock
# 
add_interface f2sdram_clock clock start
set_interface_property f2sdram_clock associatedDirectClock ""
set_interface_property f2sdram_clock ENABLED true
set_interface_property f2sdram_clock EXPORT_OF ""
set_interface_property f2sdram_clock PORT_NAME_MAP ""
set_interface_property f2sdram_clock CMSIS_SVD_VARIABLES ""
set_interface_property f2sdram_clock SVD_ADDRESS_GROUP ""
set_interface_property f2sdram_clock IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property f2sdram_clock SV_INTERFACE_TYPE ""
set_interface_property f2sdram_clock SV_INTERFACE_MODPORT_TYPE ""

add_interface_port f2sdram_clock f2sdram_clock clk Output 1


# 
# connection point f2sdram_reset_n
# 
add_interface f2sdram_reset_n reset start
set_interface_property f2sdram_reset_n associatedClock f2sdram_clock
set_interface_property f2sdram_reset_n associatedDirectReset ""
set_interface_property f2sdram_reset_n associatedResetSinks h2f_reset_in
set_interface_property f2sdram_reset_n synchronousEdges DEASSERT
set_interface_property f2sdram_reset_n ENABLED true
set_interface_property f2sdram_reset_n EXPORT_OF ""
set_interface_property f2sdram_reset_n PORT_NAME_MAP ""
set_interface_property f2sdram_reset_n CMSIS_SVD_VARIABLES ""
set_interface_property f2sdram_reset_n SVD_ADDRESS_GROUP ""
set_interface_property f2sdram_reset_n IPXACT_REGISTER_MAP_VARIABLES ""
set_interface_property f2sdram_reset_n SV_INTERFACE_TYPE ""
set_interface_property f2sdram_reset_n SV_INTERFACE_MODPORT_TYPE ""

add_interface_port f2sdram_reset_n f2sdram_reset_n reset_n Output 1