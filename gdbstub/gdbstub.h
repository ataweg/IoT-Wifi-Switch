// 2018-07-19  AWe  see https://github.com/valexby/esp-gdbstub/commit/c026ef2f678a7cdd972aa5df914b5e8e30d2c42d

#ifndef GDBSTUB_H
#define GDBSTUB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void gdbstub_init(bool break_on_init);
void gdbstub_do_break(void);

#ifdef __cplusplus
}
#endif

#endif