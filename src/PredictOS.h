/*
 * PredictOS.h
 */

#ifndef SRC_PREDICTOS_H_
#define SRC_PREDICTOS_H_

// maximum task number. should be no more than 32
#define MAX_TASK_NUM (32U)
// systick frequency
#define TICKS_PER_SECOND (1000U)

// task function. should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

// Task Control Block
typedef struct _PdOSTaskHandle
{
	void *psp;  // stack pointer
	uint32_t timeout;  // delay time down-counter
	uint8_t prio;  // priority
	uint8_t preserve[3];  // align by 4 bytes
} PdOSTaskHandle;

void PdOS_init(void *idleStkSto, uint32_t idleStkSize);
void PdOS_run(void);
void PdOS_create_task(PdOSTaskHandle *h, PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize);
void PdOS_sched(void);
__attribute__((weak)) void PdOS_on_idle(void);
void PdOS_delay(uint32_t ticks);

#endif /* SRC_PREDICTOS_H_ */
