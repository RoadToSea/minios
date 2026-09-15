/*
 * ************************************************
 *
 *        Shared mode switch for the debug probe demo
 *
 *  RTOS_TEST_ENABLED selects the execution mode. Change it
 *  here and rebuild - main.c, debug_probe.c and rtos_test.c
 *  all read it from this single place.
 *
 *    0 = bare metal: main() polling loop runs the probes,
 *        thread mode uses MSP (CONTROL.SPSEL = 0)
 *    1 = CMSIS-RTOS2 (RTX): test threads run the probes,
 *        thread mode uses PSP (CONTROL.SPSEL = 1), the SVC /
 *        PendSV / SysTick vectors belong to the RTX kernel
 *
 * ************************************************
 */

#ifndef PROBE_CONFIG_H
#define PROBE_CONFIG_H

#define RTOS_TEST_ENABLED 0

#endif /* PROBE_CONFIG_H */
