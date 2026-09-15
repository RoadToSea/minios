#include <stdint.h>

#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include "time_slice_core.h"

static volatile uint32_t task_counter;
static uint8_t task_stack[128];

static void TaskEntry(void *argument)
{
    (void)argument;

    while (1) {
        task_counter++;
    }
}

static task_block_t task = {
    .name = "task",
    .func = TaskEntry,
    .buf = task_stack,
    .buf_size = sizeof(task_stack),
};

int main(void)
{
    SystemInit();
    time_slice_core_init();
    time_slice_add_task(&task);

    /* PendSV performs the first PSP restore and enters TaskEntry(). */
    time_slice_switch_first_task();

    while (1) {
        __NOP();
    }
}
