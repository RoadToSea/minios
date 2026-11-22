#ifdef TEST_CASE_PREEMPT

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "minios.h"
#include "minios_err.h"
#include "task.h"

/* ============================================================================
 * 公共资源区：优先级约定、共享静态栈及节拍对齐辅助
 * 说明：4 个用例按顺序串行执行，每个用例退出前都会 delete 自己的临时任务，
 *       因此可安全复用 s_worker_stack_a / s_worker_stack_b，避免浪费 RAM。
 * ============================================================================ */

#define PRIO_HIGH         1U
#define PRIO_RUNNER       2U
#define PRIO_LOW          3U

#define WAKE_ROUNDS       5U
#define WAKE_MS           50U
#define STARVE_MS         100U
#define DECOY_MS          500U

#define RUNNER_STACK_SIZE 1024U
#define WORKER_STACK_A    512U
#define WORKER_STACK_B    512U

static uint8_t s_runner_stack[RUNNER_STACK_SIZE];
static uint8_t s_worker_stack_a[WORKER_STACK_A];
static uint8_t s_worker_stack_b[WORKER_STACK_B];

static void wait_next_tick_edge(void)
{
    uint32_t cur = minios_get_tick();
    while (minios_get_tick() == cur) {
    }
}

/* ============================================================================
 * Case 1：运行期 task_add 立即抢占测试
 * ----------------------------------------------------------------------------
 * 测试目的：验证当前运行任务调用 task_add() 注册更高优先级任务时，必须在
 *           task_add() 返回前立刻发生抢占，而非等到下一次 SysTick 中断才切换。
 * 执行步骤：1. 等一个 tick 跳变边沿，记录步序 1 与当前 tick；
 *           2. runner(prio 2) 调用 task_add() 注册 high(prio 1)；
 *           3. high 入口立即记录步序 2 与进入时 tick，随后 task_delay 让权；
 *           4. runner 从 task_add() 返回后记录步序 3，并删除 high。
 * 通过标准：步序严格为 [1, 2, 3] 且步骤 2 与步骤 1 处于同一 tick（dt == 0）。
 * ============================================================================ */

static volatile uint8_t  s_c1_seq[4];
static volatile uint32_t s_c1_idx = 0U;
static volatile uint32_t s_c1_high_tick = 0U;

static void c1_record(uint8_t step)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (s_c1_idx < 4U) {
        s_c1_seq[s_c1_idx++] = step;
    }
    __set_PRIMASK(primask);
}

static void c1_high_entry(void *arg)
{
    (void)arg;
    s_c1_high_tick = minios_get_tick();
    c1_record(2U);
    while (1) {
        task_delay(1000U);
    }
}

static task_block_t s_c1_high_task = {
    .name     = "c1_high",
    .priority = PRIO_HIGH,
    .func     = c1_high_entry,
    .buf      = s_worker_stack_a,
    .buf_size = sizeof(s_worker_stack_a)
};

static int case1_add_immediate_preempt(void)
{
    uint32_t before_tick;
    uint32_t dt;
    minios_err_t err;
    int pass;

    printf("[Case 1] task_add immediate preempt ... ");
    s_c1_idx = 0U;
    s_c1_high_tick = 0U;
    for (uint32_t i = 0U; i < 4U; i++) {
        s_c1_seq[i] = 0U;
    }

    wait_next_tick_edge();
    before_tick = minios_get_tick();
    c1_record(1U);
    err = task_add(&s_c1_high_task);
    c1_record(3U);

    if (err == MINIOS_EOK) {
        (void)task_delete(&s_c1_high_task);
    }

    dt = s_c1_high_tick - before_tick;
    pass = (err == MINIOS_EOK && s_c1_idx == 3U &&
            s_c1_seq[0] == 1U && s_c1_seq[1] == 2U && s_c1_seq[2] == 3U &&
            dt == 0U) ? 1 : 0;

    printf("%s (seq=%u,%u,%u dt=%lu)\r\n",
           pass ? "PASS" : "FAIL",
           (unsigned int)s_c1_seq[0],
           (unsigned int)s_c1_seq[1],
           (unsigned int)s_c1_seq[2],
           (unsigned long)dt);
    return pass;
}

/* ============================================================================
 * Case 2：SysTick 延时唤醒强行抢占“忙等”低优先级任务测试
 * ----------------------------------------------------------------------------
 * 测试目的：验证低优先级任务死循环独占 CPU、绝不主动让权时，高优先级任务
 *           延时到期后仍能靠 SysTick + PendSV 强行夺回 CPU。
 * 执行步骤：1. runner(prio 2) 拉起 low(prio 3)，low 死循环递增 s_c2_low_cnt；
 *           2. runner 连续 5 轮调用 task_delay(50ms)，让 low 充分忙等占满 CPU；
 *           3. 每轮醒来校验 s_c2_low_cnt 严格递增，且睡眠耗时为 50~51 ticks；
 *           4. 结束后删除 low。
 * 通过标准：5 轮全部按时唤醒，且每轮期间低优任务均有推进（证明双向交接正常）。
 * ============================================================================ */

static volatile uint32_t s_c2_low_cnt = 0U;

static void c2_low_entry(void *arg)
{
    (void)arg;
    while (1) {
        s_c2_low_cnt++;
    }
}

static task_block_t s_c2_low_task = {
    .name     = "c2_low",
    .priority = PRIO_LOW,
    .func     = c2_low_entry,
    .buf      = s_worker_stack_a,
    .buf_size = sizeof(s_worker_stack_a)
};

static int case2_systick_preempt_busy_low(void)
{
    minios_err_t err;
    int pass = 1;

    printf("[Case 2] SysTick preempt busy low   ... ");
    s_c2_low_cnt = 0U;
    err = task_add(&s_c2_low_task);

    if (err != MINIOS_EOK) {
        pass = 0;
    } else {
        for (uint32_t i = 0U; i < WAKE_ROUNDS; i++) {
            uint32_t prev_cnt  = s_c2_low_cnt;
            uint32_t prev_tick = minios_get_tick();
            uint32_t dt;

            task_delay(WAKE_MS);
            dt = minios_get_tick() - prev_tick;
            if (s_c2_low_cnt <= prev_cnt || dt < WAKE_MS || dt > (WAKE_MS + 1U)) {
                pass = 0;
                break;
            }
        }
        (void)task_delete(&s_c2_low_task);
    }

    printf("%s (cntLow=%lu)\r\n", pass ? "PASS" : "FAIL", (unsigned long)s_c2_low_cnt);
    return pass;
}

/* ============================================================================
 * Case 3：严格优先级饿死测试（高优不让权，低优零执行）
 * ----------------------------------------------------------------------------
 * 测试目的：验证调度器严格遵守优先级压制，未退化为跨优先级时间片轮转。
 * 执行步骤：1. runner(prio 2) 拉起就绪的 low(prio 3)；
 *           2. runner 不调用 task_delay()，依靠 minios_get_tick() 原地忙等 100ms
 *             （期间经历约 100 次 SysTick，同时确保内核延时队列为空）；
 *           3. 检查 s_c3_low_cnt 增量后删除 low。
 * 通过标准：100 拍期间 low 得到的 CPU 次数严格为 0（delta == 0）。
 * ============================================================================ */

static volatile uint32_t s_c3_low_cnt = 0U;

static void c3_low_entry(void *arg)
{
    (void)arg;
    while (1) {
        s_c3_low_cnt++;
    }
}

static task_block_t s_c3_low_task = {
    .name     = "c3_low",
    .priority = PRIO_LOW,
    .func     = c3_low_entry,
    .buf      = s_worker_stack_a,
    .buf_size = sizeof(s_worker_stack_a)
};

static int case3_high_starves_low(void)
{
    minios_err_t err;
    uint32_t delta = 0U;
    int pass;

    printf("[Case 3] High priority starves low  ... ");
    s_c3_low_cnt = 0U;
    err = task_add(&s_c3_low_task);

    if (err == MINIOS_EOK) {
        uint32_t t_start = minios_get_tick();
        while ((uint32_t)(minios_get_tick() - t_start) < STARVE_MS) {
        }
        delta = s_c3_low_cnt;
        (void)task_delete(&s_c3_low_task);
    }

    pass = (err == MINIOS_EOK && delta == 0U) ? 1 : 0;
    printf("%s (low_delta=%lu)\r\n", pass ? "PASS" : "FAIL", (unsigned long)delta);
    return pass;
}

/* ============================================================================
 * Case 4：多节点延时链表唤醒抢占测试（队首到期、队尾仍未到期）
 * ----------------------------------------------------------------------------
 * 测试目的：验证 __delay_list 存在多个任务时，只要队首到期就必须立刻触发
 *           抢占唤醒，不能因后面还有未到期节点而丢弃唤醒或拖到长延时结束。
 * 执行步骤：1. runner(prio 2) 拉起 decoy(prio 1)，decoy 立即抢占并睡 500ms；
 *           2. runner 拉起忙等 low(prio 3)，随后自己睡 50ms
 *             （此时延时队列为 [runner(+50ms), decoy(+500ms)]）；
 *           3. runner 醒来记录耗时 dt，删除 decoy 与 low。
 * 通过标准：decoy 已启动，期间 low 正常计数，且 runner 在 50~52 ticks 准时醒来。
 * ============================================================================ */

static volatile uint32_t s_c4_decoy_ready = 0U;
static volatile uint32_t s_c4_low_cnt = 0U;

static void c4_decoy_entry(void *arg)
{
    (void)arg;
    s_c4_decoy_ready = 1U;
    while (1) {
        task_delay(DECOY_MS);
    }
}

static void c4_low_entry(void *arg)
{
    (void)arg;
    while (1) {
        s_c4_low_cnt++;
    }
}

static task_block_t s_c4_decoy_task = {
    .name     = "c4_decoy",
    .priority = PRIO_HIGH,
    .func     = c4_decoy_entry,
    .buf      = s_worker_stack_b,
    .buf_size = sizeof(s_worker_stack_b)
};

static task_block_t s_c4_low_task = {
    .name     = "c4_low",
    .priority = PRIO_LOW,
    .func     = c4_low_entry,
    .buf      = s_worker_stack_a,
    .buf_size = sizeof(s_worker_stack_a)
};

static int case4_multinode_delay_preempt(void)
{
    minios_err_t err_decoy;
    minios_err_t err_low;
    uint32_t t0;
    uint32_t dt;
    int pass;

    printf("[Case 4] Multi-node delay preempt   ... ");
    s_c4_decoy_ready = 0U;
    s_c4_low_cnt = 0U;

    err_decoy = task_add(&s_c4_decoy_task);
    err_low   = task_add(&s_c4_low_task);

    t0 = minios_get_tick();
    task_delay(WAKE_MS);
    dt = minios_get_tick() - t0;

    if (err_decoy == MINIOS_EOK) {
        (void)task_delete(&s_c4_decoy_task);
    }
    if (err_low == MINIOS_EOK) {
        (void)task_delete(&s_c4_low_task);
    }

    pass = (err_decoy == MINIOS_EOK && err_low == MINIOS_EOK &&
            s_c4_decoy_ready == 1U && s_c4_low_cnt > 0U &&
            dt >= WAKE_MS && dt <= (WAKE_MS + 2U)) ? 1 : 0;

    printf("%s (decoy=%lu low=%lu dt=%lu)\r\n",
           pass ? "PASS" : "FAIL",
           (unsigned long)s_c4_decoy_ready,
           (unsigned long)s_c4_low_cnt,
           (unsigned long)dt);
    return pass;
}

/* ============================================================================
 * 测试总控任务与对外入口
 * ============================================================================ */

static void runner_entry(void *arg)
{
    int passed = 0;

    (void)arg;
    printf("\r\n=== [sched/preempt] 4-Case Suite Start ===\r\n");

    passed += case1_add_immediate_preempt();
    passed += case2_systick_preempt_busy_low();
    passed += case3_high_starves_low();
    passed += case4_multinode_delay_preempt();

    printf("=== Summary: %d/4 PASS ===\r\n", passed);

    while (1) {
        task_delay(1000U);
    }
}

static task_block_t s_runner_task = {
    .name     = "preempt_runner",
    .priority = PRIO_RUNNER,
    .func     = runner_entry,
    .buf      = s_runner_stack,
    .buf_size = sizeof(s_runner_stack)
};

/**
 * @brief 启动调度器优先级抢占测试套件（含 4 个独立顺序用例）
 * @param 无参数
 * @return minios_err_t 执行结果
 *         - MINIOS_EOK: 正常情况下启动调度后永不返回
 *         - MINIOS_EBUSY: 调度器已处于运行态（__current_task != NULL），禁止重复启动
 *         - MINIOS_EINVAL: 测试总控任务参数校验失败
 *         - MINIOS_ERROR: 内核启动失败（调度器未启动）
 * @note 调用前须完成硬件基础初始化与 USART_Printf_Init()，且不要提前调用 minios_init()/minios_start()
 */
minios_err_t test_sched_preempt_run(void)
{
    minios_err_t err;

    if (__current_task != NULL) {
        return MINIOS_EBUSY;
    }

    err = minios_init();
    if (err != MINIOS_EOK) {
        return err;
    }

    err = task_add(&s_runner_task);
    if (err != MINIOS_EOK) {
        return err;
    }

    return minios_start();
}

#endif /* TEST_CASE_PREEMPT */
