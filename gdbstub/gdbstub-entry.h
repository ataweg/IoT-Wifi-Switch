// 2018-07-19  AWe see https://github.com/slaff/esp-gdbstub/commit/013f870775885a96f13cb86ea6a6eccbb65ac562

#ifndef GDBSTUB_ENTRY_H
#define GDBSTUB_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _BC_
void gdbstub_init_debug_entry();
#endif // _BC_
void gdbstub_do_break();
void gdbstub_icount_ena_single_step();
void gdbstub_save_extra_sfrs_for_exception();
void gdbstub_uart_entry();

int gdbstub_set_hw_breakpoint( int addr, int len );
int gdbstub_set_hw_watchpoint( int addr, int len, int type );
int gdbstub_del_hw_breakpoint( int addr );
int gdbstub_del_hw_watchpoint( int addr );

void* gdbstub_do_break_breakpoint_addr;

#ifdef __cplusplus
}
#endif

#endif