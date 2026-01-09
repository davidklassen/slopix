#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

void scheduler_init(void);
void scheduler_add(process_t *proc);
void scheduler_schedule(void);

#endif
