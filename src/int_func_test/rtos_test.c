/*
 * ************************************************
 *
 *        RTOS multi-thread test module
 *
 *  rtos_test.c - demonstrates function-call and
 *  interrupt-triggered call debugging under an RTOS
 *  with multiple threads.
 *
 *  Scenario:
 *    - two worker threads call DebugProbe_RunNormalCall
 *      concurrently (serialized by a mutex);
 *    - the main test thread polls probeRunRequest and
 *      services it, as main.c did in bare-metal mode.
 *
 *  What to compare in the debugger:
 *    - probeNormalRegs captured from different threads:
 *      SP alternates between the per-thread PSP stacks,
 *      CONTROL.SPSEL is 1 in every thread snapshot.
 *    - probeIrqRegs: always MSP, IPSR = 44 regardless of
 *      which thread was interrupted.
 *    - RTX kernel internals: step into osDelay() to see the
 *      SVC call (osRtxKernelLock / wait path) and watch the
 *      SVC_Handler / PendSV_Handler vectors from
 *      lib/CMSIS/RTOS2/RTX/Source/irq_armv7m.S take over.
 *
 *  Watch-view symbols are the probe* symbols declared
 *  in debug_probe.h.
 *
 * ************************************************
 */

#include "cmsis_os2.h"
#include "debug_probe.h"
#include "rtos_test.h"

static osMutexId_t probeMutex;

/* Worker thread: repeatedly exercises the normal-call probe
 * under the mutex so snapshots from the two threads do not
 * interleave. */
static void ProbeWorkerThread(void *argument)
{
    uint32_t base = (uint32_t)(uintptr_t)argument;

    for (;;) {
        if (osMutexAcquire(probeMutex, osWaitForever) == osOK) {
            probeNormalResult = DebugProbe_RunNormalCall(base, 0x0D000000U);
            osMutexRelease(probeMutex);
        }
        osDelay(300);
    }
}

/* Main test thread: bare-metal style polling loop, now
 * cooperative with the RTOS through osDelay. */
static void DebugTestThread(void *argument)
{
    (void)argument;

    for (;;) {
        if (probeRunRequest != 0U) {
            probeRunRequest = 0;
            if (osMutexAcquire(probeMutex, osWaitForever) == osOK) {
                probeNormalResult = DebugProbe_RunNormalCall(0x11111111U, 0x22222222U);
                osMutexRelease(probeMutex);
            }
        }

        /* Extra: periodic pure-IRQ probe without a normal call */
        DebugProbe_TriggerIrq();
        osDelay(1000);
    }
}

void RtosTest_Start(void)
{
    osThreadId_t workerA;
    osThreadId_t workerB;

    probeMutex = osMutexNew(NULL);

    workerA = osThreadNew(ProbeWorkerThread, (void *)(uintptr_t)0x0A000000U, NULL);
    workerB = osThreadNew(ProbeWorkerThread, (void *)(uintptr_t)0x0B000000U, NULL);

    (void)workerA;
    (void)workerB;

    osThreadNew(DebugTestThread, NULL, NULL);
}
