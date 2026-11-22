#ifndef __MINIOS_ERR_H
#define __MINIOS_ERR_H

#include <stdint.h>

/**
 * @brief 系统通用错误与执行状态枚举
 * 
 * 返回值约定：
 * - 0 代表操作成功完成。
 * - 负数代表发生错误或异常，具体原因对应各枚举值。
 */
typedef enum minios_err {
    MINIOS_EOK          =  0,   /**< 操作成功 */
    MINIOS_ERROR        = -1,   /**< 未指定通用错误 */
    MINIOS_EINVAL       = -2,   /**< 无效参数（如传入空指针或数值越界） */
    MINIOS_ENOMEM       = -3,   /**< 内存不足或栈空间分配失败 */
    MINIOS_EBUSY        = -4,   /**< 目标资源被占用或状态冲突 */
    MINIOS_ETIMEOUT     = -5,   /**< 等待超时 */
    MINIOS_EISR         = -6,   /**< 中断上下文非法调用 */
    MINIOS_EEXPIRED     = -7,   /**< 任务延时到期*/
    MINIOS_ETASK_TOG    = -8,   /**< 需要进行任务切换 */
    MINIOS_ERR_RESERVED = 0x7FFFFFFF /**< 避免编译器优化压缩枚举尺寸，保持 32 位整型对齐 */
} minios_err_t;

#endif /* __MINIOS_ERR_H */
