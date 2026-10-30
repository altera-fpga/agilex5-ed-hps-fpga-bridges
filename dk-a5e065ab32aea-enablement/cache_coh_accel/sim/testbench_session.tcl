# Begin_DVE_Session_Save_Info
# DVE full session
# Saved on Thu Jul 9 08:13:37 2026
# Designs open: 1
#   V1: vcdplus.vpd
# Toplevel windows open: 2
# 	TopLevel.1
# 	TopLevel.2
#   Wave.1: 52 signals
#   Group count = 5
#   Group CCA_Accel signal count = 0
#   Group msgDMA_wr signal count = 0
#   Group msgDMA_rd signal count = 0
#   Group config_S0 signal count = 23
#   Group CCA_CSR signal count = 29
# End_DVE_Session_Save_Info

# DVE version: U-2023.03-SP2-1_Full64
# DVE build date: Oct 13 2023 21:05:31


#<Session mode="Full" path="/tmp/plupas/work/apps.fpga.soc.agilex5e-ed-ghrd/dk-a5e065ab32aea-enablement/cache_coh_accel/sim/testbench_session.tcl" type="Debug">

gui_set_loading_session_type Post
gui_continuetime_set

# Close design
if { [gui_sim_state -check active] } {
    gui_sim_terminate
}
gui_close_db -all
gui_expr_clear_all

# Close all windows
gui_close_window -type Console
gui_close_window -type Wave
gui_close_window -type Source
gui_close_window -type Schematic
gui_close_window -type Data
gui_close_window -type DriverLoad
gui_close_window -type List
gui_close_window -type Memory
gui_close_window -type HSPane
gui_close_window -type DLPane
gui_close_window -type Assertion
gui_close_window -type CovHier
gui_close_window -type CoverageTable
gui_close_window -type CoverageMap
gui_close_window -type CovDetail
gui_close_window -type Local
gui_close_window -type Stack
gui_close_window -type Watch
gui_close_window -type Group
gui_close_window -type Transaction



# Application preferences
gui_set_pref_value -key app_default_font -value {Helvetica,10,-1,5,50,0,0,0,0,0}
gui_src_preferences -tabstop 8 -maxbits 24 -windownumber 1
#<WindowLayout>

# DVE top-level session


# Create and position top-level window: TopLevel.1

if {![gui_exist_window -window TopLevel.1]} {
    set TopLevel.1 [ gui_create_window -type TopLevel \
       -icon $::env(DVE)/auxx/gui/images/toolbars/dvewin.xpm] 
} else { 
    set TopLevel.1 TopLevel.1
}
gui_show_window -window ${TopLevel.1} -show_state normal -rect {{473 145} {2123 1124}}

# ToolBar settings
gui_set_toolbar_attributes -toolbar {TimeOperations} -dock_state top
gui_set_toolbar_attributes -toolbar {TimeOperations} -offset 0
gui_show_toolbar -toolbar {TimeOperations}
gui_hide_toolbar -toolbar {&File}
gui_set_toolbar_attributes -toolbar {&Edit} -dock_state top
gui_set_toolbar_attributes -toolbar {&Edit} -offset 0
gui_show_toolbar -toolbar {&Edit}
gui_hide_toolbar -toolbar {CopyPaste}
gui_set_toolbar_attributes -toolbar {&Trace} -dock_state top
gui_set_toolbar_attributes -toolbar {&Trace} -offset 0
gui_show_toolbar -toolbar {&Trace}
gui_hide_toolbar -toolbar {TraceInstance}
gui_hide_toolbar -toolbar {BackTrace}
gui_set_toolbar_attributes -toolbar {&Scope} -dock_state top
gui_set_toolbar_attributes -toolbar {&Scope} -offset 0
gui_show_toolbar -toolbar {&Scope}
gui_set_toolbar_attributes -toolbar {&Window} -dock_state top
gui_set_toolbar_attributes -toolbar {&Window} -offset 0
gui_show_toolbar -toolbar {&Window}
gui_set_toolbar_attributes -toolbar {Signal} -dock_state top
gui_set_toolbar_attributes -toolbar {Signal} -offset 0
gui_show_toolbar -toolbar {Signal}
gui_set_toolbar_attributes -toolbar {Zoom} -dock_state top
gui_set_toolbar_attributes -toolbar {Zoom} -offset 0
gui_show_toolbar -toolbar {Zoom}
gui_set_toolbar_attributes -toolbar {Zoom And Pan History} -dock_state top
gui_set_toolbar_attributes -toolbar {Zoom And Pan History} -offset 0
gui_show_toolbar -toolbar {Zoom And Pan History}
gui_set_toolbar_attributes -toolbar {Grid} -dock_state top
gui_set_toolbar_attributes -toolbar {Grid} -offset 0
gui_show_toolbar -toolbar {Grid}
gui_hide_toolbar -toolbar {Simulator}
gui_hide_toolbar -toolbar {Interactive Rewind}
gui_hide_toolbar -toolbar {Testbench}

# End ToolBar settings

# Docked window settings
set HSPane.1 [gui_create_window -type HSPane -parent ${TopLevel.1} -dock_state left -dock_on_new_line true -dock_extent 686]
catch { set Hier.1 [gui_share_window -id ${HSPane.1} -type Hier] }
gui_set_window_pref_key -window ${HSPane.1} -key dock_width -value_type integer -value 686
gui_set_window_pref_key -window ${HSPane.1} -key dock_height -value_type integer -value 578
gui_set_window_pref_key -window ${HSPane.1} -key dock_offset -value_type integer -value 0
gui_update_layout -id ${HSPane.1} {{left 0} {top 0} {width 685} {height 678} {dock_state left} {dock_on_new_line true} {child_hier_colhier 477} {child_hier_coltype 132} {child_hier_colpd 0} {child_hier_col1 0} {child_hier_col2 1} {child_hier_col3 -1}}
set DLPane.1 [gui_create_window -type DLPane -parent ${TopLevel.1} -dock_state left -dock_on_new_line true -dock_extent 470]
catch { set Data.1 [gui_share_window -id ${DLPane.1} -type Data] }
gui_set_window_pref_key -window ${DLPane.1} -key dock_width -value_type integer -value 470
gui_set_window_pref_key -window ${DLPane.1} -key dock_height -value_type integer -value 556
gui_set_window_pref_key -window ${DLPane.1} -key dock_offset -value_type integer -value 0
gui_update_layout -id ${DLPane.1} {{left 0} {top 0} {width 469} {height 678} {dock_state left} {dock_on_new_line true} {child_data_colvariable 264} {child_data_colvalue 131} {child_data_coltype 166} {child_data_col1 0} {child_data_col2 1} {child_data_col3 2}}
set Console.1 [gui_create_window -type Console -parent ${TopLevel.1} -dock_state bottom -dock_on_new_line true -dock_extent 230]
gui_set_window_pref_key -window ${Console.1} -key dock_width -value_type integer -value -1
gui_set_window_pref_key -window ${Console.1} -key dock_height -value_type integer -value 230
gui_set_window_pref_key -window ${Console.1} -key dock_offset -value_type integer -value 0
gui_update_layout -id ${Console.1} {{left 0} {top 0} {width 247} {height 229} {dock_state bottom} {dock_on_new_line true}}
set DriverLoad.1 [gui_create_window -type DriverLoad -parent ${TopLevel.1} -dock_state bottom -dock_on_new_line false -dock_extent 230]
gui_set_window_pref_key -window ${DriverLoad.1} -key dock_width -value_type integer -value 150
gui_set_window_pref_key -window ${DriverLoad.1} -key dock_height -value_type integer -value 230
gui_set_window_pref_key -window ${DriverLoad.1} -key dock_offset -value_type integer -value 0
gui_update_layout -id ${DriverLoad.1} {{left 0} {top 0} {width 1402} {height 229} {dock_state bottom} {dock_on_new_line false}}
#### Start - Readjusting docked view's offset / size
set dockAreaList { top left right bottom }
foreach dockArea $dockAreaList {
  set viewList [gui_ekki_get_window_ids -active_parent -dock_area $dockArea]
  foreach view $viewList {
      if {[lsearch -exact [gui_get_window_pref_keys -window $view] dock_width] != -1} {
        set dockWidth [gui_get_window_pref_value -window $view -key dock_width]
        set dockHeight [gui_get_window_pref_value -window $view -key dock_height]
        set offset [gui_get_window_pref_value -window $view -key dock_offset]
        if { [string equal "top" $dockArea] || [string equal "bottom" $dockArea]} {
          gui_set_window_attributes -window $view -dock_offset $offset -width $dockWidth
        } else {
          gui_set_window_attributes -window $view -dock_offset $offset -height $dockHeight
        }
      }
  }
}
#### End - Readjusting docked view's offset / size
gui_sync_global -id ${TopLevel.1} -option true


# Create and position top-level window: TopLevel.2

if {![gui_exist_window -window TopLevel.2]} {
    set TopLevel.2 [ gui_create_window -type TopLevel \
       -icon $::env(DVE)/auxx/gui/images/toolbars/dvewin.xpm] 
} else { 
    set TopLevel.2 TopLevel.2
}
gui_show_window -window ${TopLevel.2} -show_state normal -rect {{55 519} {2554 1332}}

# ToolBar settings
gui_set_toolbar_attributes -toolbar {TimeOperations} -dock_state top
gui_set_toolbar_attributes -toolbar {TimeOperations} -offset 0
gui_show_toolbar -toolbar {TimeOperations}
gui_hide_toolbar -toolbar {&File}
gui_set_toolbar_attributes -toolbar {&Edit} -dock_state top
gui_set_toolbar_attributes -toolbar {&Edit} -offset 0
gui_show_toolbar -toolbar {&Edit}
gui_hide_toolbar -toolbar {CopyPaste}
gui_set_toolbar_attributes -toolbar {&Trace} -dock_state top
gui_set_toolbar_attributes -toolbar {&Trace} -offset 0
gui_show_toolbar -toolbar {&Trace}
gui_hide_toolbar -toolbar {TraceInstance}
gui_hide_toolbar -toolbar {BackTrace}
gui_set_toolbar_attributes -toolbar {&Scope} -dock_state top
gui_set_toolbar_attributes -toolbar {&Scope} -offset 0
gui_show_toolbar -toolbar {&Scope}
gui_set_toolbar_attributes -toolbar {&Window} -dock_state top
gui_set_toolbar_attributes -toolbar {&Window} -offset 0
gui_show_toolbar -toolbar {&Window}
gui_set_toolbar_attributes -toolbar {Signal} -dock_state top
gui_set_toolbar_attributes -toolbar {Signal} -offset 0
gui_show_toolbar -toolbar {Signal}
gui_set_toolbar_attributes -toolbar {Zoom} -dock_state top
gui_set_toolbar_attributes -toolbar {Zoom} -offset 0
gui_show_toolbar -toolbar {Zoom}
gui_set_toolbar_attributes -toolbar {Zoom And Pan History} -dock_state top
gui_set_toolbar_attributes -toolbar {Zoom And Pan History} -offset 0
gui_show_toolbar -toolbar {Zoom And Pan History}
gui_set_toolbar_attributes -toolbar {Grid} -dock_state top
gui_set_toolbar_attributes -toolbar {Grid} -offset 0
gui_show_toolbar -toolbar {Grid}
gui_hide_toolbar -toolbar {Simulator}
gui_hide_toolbar -toolbar {Interactive Rewind}
gui_hide_toolbar -toolbar {Testbench}

# End ToolBar settings

# Docked window settings
gui_sync_global -id ${TopLevel.2} -option true

# MDI window settings
set Wave.1 [gui_create_window -type {Wave}  -parent ${TopLevel.2}]
gui_show_window -window ${Wave.1} -show_state maximized
gui_update_layout -id ${Wave.1} {{show_state maximized} {dock_state undocked} {dock_on_new_line false} {child_wave_left 386} {child_wave_right 2108} {child_wave_colname 196} {child_wave_colvalue 186} {child_wave_col1 0} {child_wave_col2 1}}

# End MDI window settings

gui_set_env TOPLEVELS::TARGET_FRAME(Source) ${TopLevel.1}
gui_set_env TOPLEVELS::TARGET_FRAME(Schematic) ${TopLevel.1}
gui_set_env TOPLEVELS::TARGET_FRAME(PathSchematic) ${TopLevel.1}
gui_set_env TOPLEVELS::TARGET_FRAME(Wave) none
gui_set_env TOPLEVELS::TARGET_FRAME(List) none
gui_set_env TOPLEVELS::TARGET_FRAME(Memory) ${TopLevel.1}
gui_set_env TOPLEVELS::TARGET_FRAME(DriverLoad) none
gui_update_statusbar_target_frame ${TopLevel.1}
gui_update_statusbar_target_frame ${TopLevel.2}

#</WindowLayout>

#<Database>

# DVE Open design session: 

if { ![gui_is_db_opened -db {vcdplus.vpd}] } {
	gui_open_db -design V1 -file vcdplus.vpd -nosource
}
gui_set_precision 1ps
gui_set_time_units 1ps
#</Database>

# DVE Global setting session: 


# Global: Bus

# Global: Expressions

# Global: Signal Time Shift

# Global: Signal Compare

# Global: Signal Groups
gui_load_child_values {testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config}


set _session_group_1 CCA_Accel
gui_sg_create "$_session_group_1"
set CCA_Accel "$_session_group_1"


set _session_group_2 msgDMA_wr
gui_sg_create "$_session_group_2"
set msgDMA_wr "$_session_group_2"


set _session_group_3 msgDMA_rd
gui_sg_create "$_session_group_3"
set msgDMA_rd "$_session_group_3"


set _session_group_4 config_S0
gui_sg_create "$_session_group_4"
set config_S0 "$_session_group_4"

gui_sg_addsignal -group "$_session_group_4" { testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.clk testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.reset testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_burstcount testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_byteenable testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_debugaccess testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_waitrequest testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_write testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_address testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_writedata testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_read testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_address testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_readdatavalid testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_readdata testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_address testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_burstcount testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_byteenable testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_debugaccess testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_read testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_readdata testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_readdatavalid testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_waitrequest testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_write testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.m0_writedata }

set _session_group_5 CCA_CSR
gui_sg_create "$_session_group_5"
set CCA_CSR "$_session_group_5"

gui_sg_addsignal -group "$_session_group_5" { testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.sys_clk testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.sys_rst testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_waitrequest testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_write testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_address testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_byteenable testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_writedata testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_read testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_address testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_readdatavalid testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.avmm_csr_readdata testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.wr_transaction_q testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.wr_addr testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.wr_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.rd_transaction_q testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.rd_addr testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.read_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_tst_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_0_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_1_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_2_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_3_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_4_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_5_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_6_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_7_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_8_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_9_reg_data testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_cca_accel_top.u_accel_csr_if.dbg_10_reg_data }

# Global: Highlighting

# Global: Stack
gui_change_stack_mode -mode list

# Post database loading setting...

# Restore C1 time
gui_set_time -C1_only 11007500



# Save global setting...

# Wave/List view global setting
gui_list_create_group_when_add -wave -enable
gui_cov_show_value -switch false

# Close all empty TopLevel windows
foreach __top [gui_ekki_get_window_ids -type TopLevel] {
    if { [llength [gui_ekki_get_window_ids -parent $__top]] == 0} {
        gui_close_window -window $__top
    }
}
gui_set_loading_session_type noSession
# DVE View/pane content session: 


# Hier 'Hier.1'
gui_show_window -window ${Hier.1}
gui_list_set_filter -id ${Hier.1} -list { {Package 1} {All 0} {Process 1} {VirtPowSwitch 0} {UnnamedProcess 1} {UDP 0} {Function 1} {Block 1} {SrsnAndSpaCell 0} {OVA Unit 1} {LeafScCell 1} {LeafVlgCell 1} {Interface 1} {LeafVhdCell 1} {$unit 1} {NamedBlock 1} {Task 1} {VlgPackage 1} {ClassDef 1} {VirtIsoCell 0} }
gui_list_set_filter -id ${Hier.1} -text {*}
gui_hier_list_init -id ${Hier.1}
gui_change_design -id ${Hier.1} -design V1
catch {gui_list_expand -id ${Hier.1} testbench}
catch {gui_list_expand -id ${Hier.1} testbench.dut}
catch {gui_list_expand -id ${Hier.1} testbench.dut.u_cache_coh_top}
catch {gui_list_expand -id ${Hier.1} testbench.dut.u_cache_coh_top.u_cca_fab_subsys}
catch {gui_list_expand -id ${Hier.1} testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys}
catch {gui_list_select -id ${Hier.1} {testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config}}
gui_view_scroll -id ${Hier.1} -vertical -set 181
gui_view_scroll -id ${Hier.1} -horizontal -set 0

# Data 'Data.1'
gui_list_set_filter -id ${Data.1} -list { {Buffer 1} {Input 1} {Others 1} {Linkage 1} {Output 1} {LowPower 1} {Parameter 1} {All 1} {Aggregate 1} {LibBaseMember 1} {Event 1} {Assertion 1} {Constant 1} {Interface 1} {BaseMembers 1} {Signal 1} {$unit 1} {Inout 1} {Variable 1} }
gui_list_set_filter -id ${Data.1} -text {*}
gui_list_show_data -id ${Data.1} {testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config}
gui_show_window -window ${Data.1}
catch { gui_list_select -id ${Data.1} {testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_address }}
gui_view_scroll -id ${Data.1} -vertical -set 0
gui_view_scroll -id ${Data.1} -horizontal -set 0
gui_view_scroll -id ${Hier.1} -vertical -set 181
gui_view_scroll -id ${Hier.1} -horizontal -set 0

# DriverLoad 'DriverLoad.1'

# View 'Wave.1'
gui_wv_sync -id ${Wave.1} -switch false
set groupExD [gui_get_pref_value -category Wave -key exclusiveSG]
gui_set_pref_value -category Wave -key exclusiveSG -value {false}
set origWaveHeight [gui_get_pref_value -category Wave -key waveRowHeight]
gui_list_set_height -id Wave -height 25
set origGroupCreationState [gui_list_create_group_when_add -wave]
gui_list_create_group_when_add -wave -disable
gui_marker_create -id ${Wave.1} M1 289261860000
gui_marker_create -id ${Wave.1} M2 19915390000
gui_marker_set_ref -id ${Wave.1}  C1
gui_wv_zoom_timerange -id ${Wave.1} 10906889 11169034
gui_list_add_group -id ${Wave.1} -after {New Group} {CCA_Accel}
gui_list_add_group -id ${Wave.1} -after {New Group} {msgDMA_wr}
gui_list_add_group -id ${Wave.1} -after {New Group} {msgDMA_rd}
gui_list_add_group -id ${Wave.1} -after {New Group} {config_S0}
gui_list_add_group -id ${Wave.1} -after {New Group} {CCA_CSR}
gui_list_collapse -id ${Wave.1} msgDMA_wr
gui_list_collapse -id ${Wave.1} msgDMA_rd
gui_list_select -id ${Wave.1} {testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_readdatavalid }
gui_seek_criteria -id ${Wave.1} {Rising}



gui_set_env TOGGLE::DEFAULT_WAVE_WINDOW ${Wave.1}
gui_set_pref_value -category Wave -key exclusiveSG -value $groupExD
gui_list_set_height -id Wave -height $origWaveHeight
if {$origGroupCreationState} {
	gui_list_create_group_when_add -wave -enable
}
if { $groupExD } {
 gui_msg_report -code DVWW028
}
gui_list_set_filter -id ${Wave.1} -list { {Buffer 1} {Input 1} {Others 1} {Linkage 1} {Output 1} {Parameter 1} {All 1} {Aggregate 1} {LibBaseMember 1} {Event 1} {Assertion 1} {Constant 1} {Interface 1} {BaseMembers 1} {Signal 1} {$unit 1} {Inout 1} {Variable 1} }
gui_list_set_filter -id ${Wave.1} -text {*}
gui_list_set_insertion_bar  -id ${Wave.1} -group config_S0  -item {testbench.dut.u_cache_coh_top.u_cca_fab_subsys.u_cca_subsys.u_msgdma_config.s0_address[27:0]} -position below

gui_marker_move -id ${Wave.1} {C1} 11007500
gui_view_scroll -id ${Wave.1} -vertical -set 107
gui_show_grid -id ${Wave.1} -enable false
# Restore toplevel window zorder
# The toplevel window could be closed if it has no view/pane
if {[gui_exist_window -window ${TopLevel.1}]} {
	gui_set_active_window -window ${TopLevel.1}
	gui_set_active_window -window ${DLPane.1}
}
if {[gui_exist_window -window ${TopLevel.2}]} {
	gui_set_active_window -window ${TopLevel.2}
	gui_set_active_window -window ${Wave.1}
}
#</Session>

