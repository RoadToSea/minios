#ifndef __MINIOS_H
#define __MINIOS_H

#include <stdint.h>
#include "tools/rlist.h"

typedef enum task_state
{
    TASK_STATE_READY = 0,
    TASK_STATE_RUNNING,
    TASK_STATE_DELAY,
    TASK_STATE_DELETED
} task_state_t;


typedef void (*task_func_t)(void *);

typedef struct task_block{
    const char *name;
    rlist_t  task_node;
    task_state_t state;
    task_func_t func;
    void *func_arg;
    uint8_t *buf;
    uint32_t buf_size;
    void *stack_ptr;
    uint32_t delay_expire;      /* 绝对到期节拍，仅在延时队列中有效 */
}task_block_t;

/* 全局任务表与当前任务：定义在 task.c，minios.c / port.c 都要碰 */
extern rlist_t __ready_list;
extern task_block_t *__current_task;

task_block_t *minios_get_next(task_block_t *cur);
int minios_init(void);
int minios_start(void);

#endif
