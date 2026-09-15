/*
 * ************************************************
 *
 *        RTOS multi-thread test module
 *
 *  rtos_test.h - see rtos_test.c for the scenario.
 *
 * ************************************************
 */

#ifndef RTOS_TEST_H
#define RTOS_TEST_H

/* Creates the test threads (call once after osKernelInitialize,
 * before osKernelStart). */
void RtosTest_Start(void);

#endif /* RTOS_TEST_H */
