#ifndef __PORT_H
#define __PORT_H

#include "stm32f10x.h"
#include "minios.h"

/**
 * @brief 计算 32 位整型末尾连续零的个数 (Count Trailing Zeros)
 * @note 仅适用于 ARMv7-M (Cortex-M3/M4) 架构，展开为 RBIT + CLZ 单周期指令
 * @param[in] mask 非零掩码值，禁止传入 0（传入 0 时结果未定义）
 * @return uint32_t 最低有效置位 bit 的序号，范围为 0 ~ 31
 */
static inline uint32_t port_ctz(uint32_t mask)
{
    return __CLZ(__RBIT(mask));
}

static inline void data_barrier(void)
{
    __DSB();
}

static inline void instruction_barrier(void)
{
    __ISB();
}

/**
 * @brief 初始化调度器所需的 Cortex-M3 异常优先级与 SysTick 硬件定时器
 * @param[in] beat_ms 预留节拍毫秒参数（当前实际节拍由 SYSTICK_RATE_HZ 决定）
 * @return 无返回值
 * @note 仅配置寄存器而不立即开启 SysTick，须在首个任务就绪后调用 start_first_task 启动
 */
void port_hardware_init(uint32_t beat_ms);

/**
 * @brief 在指定任务的栈顶构造 Cortex-M3 异常返回初始上下文假帧
 * @param[in,out] tcb 任务控制块指针，不可为 NULL；tcb->buf 与 tcb->buf_size 须保证 8 字节向下对齐后至少剩余 64 字节空间，tcb->func 不可为 NULL
 * @return 无返回值；构建完成后更新 tcb->stack_ptr 指向初始栈帧的 R4 槽位
 */
void task_build_first_frame(task_block_t *tcb);

/**
 * @brief 配置临时 PSP 缓冲区、启动 SysTick 节拍并触发 PendSV 启动首个任务调度
 * @param 无参数
 * @return 无返回值；调用前须确保至少已有就绪任务且所有就绪任务已完成栈帧构建
 */
void start_first_task(void);

/**
 * @brief 保存当前任务上下文栈顶并选取下一个最高优先级就绪任务
 * @param[in] sp 切出任务压入 R4-R11 后的 PSP 栈顶地址（仅在首次启动切换时允许为临时缓冲区栈顶）
 * @return uint32_t 下一个待恢复任务的栈顶地址（指向其 R4 槽位）
 * @note 仅由 PendSV_Handler 汇编代码调用，不可删除全局符号可见性；系统无就绪任务时将进入断点死循环
 */
uint32_t minios_task_switch(uint32_t sp);

#endif
