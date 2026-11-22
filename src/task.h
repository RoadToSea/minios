#ifndef __TASK_H
#define __TASK_H

#include "minios.h"
#include "minios_err.h"

/**
 * @brief 获取系统tick计数
 * @param 无参数
 * @return 系统tick
 */
uint32_t minios_get_tick(void);

/**
 * @brief 初始化任务管理模块、延时链表及各优先级就绪队列
 * @param 无参数
 * @return 无返回值
 * @note 会重置系统节拍计数、当前任务指针与优先级就绪位图，仅可在调度器启动前调用
 */
void task_init(void);

/**
 * @brief 查询系统就绪队列中是否存在至少一个可运行任务
 * @param 无参数
 * @return int 查询结果
 *         - 1 (非零): 存在至少一个可运行任务
 *         - 0: 所有优先级就绪队列均为空
 */
int task_has_ready(void);

/**
 * @brief 初始化任务初始栈帧并将任务控制块加入对应优先级的就绪队列
 * @param[in,out] tcb 任务控制块指针，不可为 NULL；tcb->func 与 tcb->buf 不可为 NULL，
 *                    8 字节对齐后的可用栈空间不得小于 64 字节，tcb->priority 须小于 MINIOS_PRIORITY_MAX_NUM
 * @return minios_err_t 执行结果
 *         - MINIOS_EOK: 添加成功（若调度器已启动且新任务优先级高于当前任务，会触发立即抢占）
 *         - MINIOS_EINVAL: 参数为空、栈空间不足或优先级越界
 *         - MINIOS_EBUSY: 任务节点当前已挂载在活跃调度或延时链表中
 */
minios_err_t task_add(task_block_t *tcb);

/**
 * @brief 从就绪表或延时队列中安全移除指定任务
 * @param[in,out] tcb 待删除的任务控制块指针，不可为 NULL；目标任务须已通过 task_add 注册且未处于已删除状态
 * @return minios_err_t 执行结果
 *         - MINIOS_EOK: 删除成功；若删除当前线程模式运行任务，将在触发切换后阻塞等待 PendSV 接管而不返回
 *         - MINIOS_EINVAL: 参数为空、任务未注册、处于非法状态或已被删除
 */
minios_err_t task_delete(task_block_t *tcb);

/**
 * @brief 将当前正在运行的任务挂入延时队列并让出 CPU 指定毫秒数
 * @param[in] ms 延时毫秒数，须满足 ms > 0 且 MS_TO_TICK(ms) <= 0x7FFFFFFF
 * @return minios_err_t 执行结果
 *         - MINIOS_EOK: 延时到期重新被调度唤醒后返回
 *         - MINIOS_EINVAL: ms 为 0 或换算后超出最大节拍上限
 *         - MINIOS_EISR: 在中断服务例程上下文中非法调用
 *         - MINIOS_ERROR: 当前无运行任务或任务状态非 TASK_STATE_RUNNING
 */
minios_err_t task_delay(uint32_t ms);

/**
 * @brief 推进系统节拍并检查延时队列是否到期需要任务切换
 * @param 无参数
 * @return 是否需要任务切换
 *     MINIOS_EOK:       无需任务切换
 *     MINIOS_ETASK_TOG: 需要任务切换
 * @note 供 SysTick 中断服务例程每拍调用一次，内部自行维护共享队列与位图原子性
 */
int tick_inc_handler(void);

#endif
