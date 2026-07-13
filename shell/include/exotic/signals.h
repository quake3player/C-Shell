#ifndef SIGNALS_H
#define SIGNALS_H

#include <sys/types.h>

extern volatile pid_t g_foreground_pgid;

void init_signal_handlers(void);

#endif // SIGNALS_H