/*
 * PredictOS.h
 */

#ifndef SRC_PREDICTOS_H_
#define SRC_PREDICTOS_H_

// task function, should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

// Task Control Block
typedef struct _PdOSTaskHandle
{
	void *psp;  // stack pointer
} PdOSTaskHandle;

void PdOS_init(void);
void PdOS_run(void);
void PdOS_create_task(PdOSTaskHandle *h, PdOSTaskFunction taskFunction, void *stkSto, uint32_t stkSize);
void PdOS_sched(void);

#endif /* SRC_PREDICTOS_H_ */
