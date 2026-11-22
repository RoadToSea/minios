#ifndef __MINIOS_H
#define __MINIOS_H

#include <stdint.h>
#include "tools/rlist.h"
#include "minios_err.h"

typedef enum task_state
{
    TASK_STATE_READY = 0,
    TASK_STATE_RUNNING,
    TASK_STATE_DELAY,
    TASK_STATE_DELETED
} task_state_t;


typedef void (*task_func_t)(void *);

typedef struct task_block{
    const char      *name;
    rlist_t         task_node;
    task_state_t    state;
    uint32_t        priority;
    task_func_t     func;
    void            *func_arg;
    uint8_t         *buf;
    uint32_t        buf_size;
    void            *stack_ptr;
    uint32_t        delay_expire;      /* 绝对到期节拍，仅在延时队列中有效 */
}task_block_t;

/* 当前正在运行的任务：定义在 task.c */
extern task_block_t *__current_task;

/**
 * @brief 初始化内核基础设施（硬件定时器与任务管理模块）
 * @param 无参数
 * @return minios_err_t 执行结果
 *         - MINIOS_EOK: 初始化成功
 * @note 必须在调用 task_add 或 minios_start 之前调用，仅在调度器未启动时于特权线程模式调用一次
 */
minios_err_t minios_init(void);

/**
 * @brief 启动内核多任务调度器并切入最高优先级就绪任务
 * @param 无参数
 * @return minios_err_t 执行结果
 *         - MINIOS_EOK: 正常情况下永不返回（仅用于满足编译器返回类型检查）
 *         - MINIOS_ERROR: 就绪表中无用户任务或空闲任务注册失败，调度器未启动
 * @note 调用前须已完成 minios_init() 并至少通过 task_add() 注册一个有效任务
 */
minios_err_t minios_start(void);

#endif
