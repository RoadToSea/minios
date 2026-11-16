#ifndef __PORT_H
#define __PORT_H

#include "minios.h"


void port_hardware_init(uint32_t beat_ms);
void start_first_task(rlist_t *task_head);
uint32_t minios_task_switch(uint32_t sp);


#endif
