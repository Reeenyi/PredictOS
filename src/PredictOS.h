/*
 * PredictOS.h
 */

#ifndef SRC_PREDICTOS_H_
#define SRC_PREDICTOS_H_

typedef enum
{
	PDOS_OK = 0U,
	PDOS_ERROR = 1U,
	PDOS_INVALID_PARAM = 2U
} PdOSErrCode;

// maximum task number. should be no more than 32
#define MAX_TASK_NUM (32U)

// systick frequency
#define TICKS_PER_SECOND (1000U)

// task function. should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

// task handle
typedef struct PdOSTaskControlBlock *PdOSTaskHandle;

void PdOS_on_idle(void);  // weak function to overwrite
// stack size needs to be at least 64 bytes (16 words)
PdOSErrCode PdOS_init(void *idleStkSto, uint32_t idleStkSize);
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize);
void PdOS_run(void);
void PdOS_delay(uint32_t ticks);

#endif /* SRC_PREDICTOS_H_ */
