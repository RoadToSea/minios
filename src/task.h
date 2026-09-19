#ifndef __TASK_H
#define __TASK_H

#include "minios.h"

void task_init(void);                       /* 初始化任务模块私有的延时队列 */
int  task_add(task_block_t *tcb);           /* 新任务加入就绪表 */
int  task_delete(task_block_t *tcb);        /* 删除任务（支持删除正在运行的任务） */
int  task_delay(uint32_t ms);               /* 当前任务睡 ms 毫秒，到期后自动回到就绪表 */
void task_delay_scan(void);                 /* 每个心跳调用：扫延时队列，唤醒到期任务 */

#endif
