#ifndef __TIME_SLICE_CORE_H__
#define __TIME_SLICE_CORE_H__

#include <stdint.h>
#include "tools/rlist.h"

typedef void (*task_func_t)(void *);

typedef struct task_block{
    const char *name;
    rlist_t  task_node;
    task_func_t func;
    void *func_arg;
    uint8_t *buf;
    uint32_t buf_size;
    void *stack_ptr;
}task_block_t;

int time_slice_core_init(void);
int time_slice_add_task(task_block_t * tcb);
void time_slice_switch_first_task(void);
int time_slice_start_schedule(void);

#endif