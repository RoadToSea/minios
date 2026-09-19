#ifndef __TIME_SLICE_CORE_H__
#define __TIME_SLICE_CORE_H__

#include <stdint.h>
#include "tools/rlist.h"

#define MS_TO_TICK(ms)      (1000/ms)


typedef void (*task_func_t)(void *);

typedef struct task_block{
    const char *name;
    rlist_t  task_node;
    task_func_t func;
    void *func_arg;
    uint8_t *buf;
    uint32_t buf_size;
    void *stack_ptr;
    uint32_t delay_expire;      /* 绝对到期节拍，仅在延时队列中有效 */
}task_block_t;

/* 全局任务表与当前任务：定义在 time_slice_core.c，task.c / port.c 都要碰 */
extern rlist_t __task_list;
extern task_block_t *__current_task;

task_block_t *time_slice_get_next(task_block_t *cur);
int time_slice_core_init(void);
int time_slice_start_schedule(void);

#endif
