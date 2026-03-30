/*
 * PredictOS.h
 */

#ifndef SRC_PREDICTOS_H_
#define SRC_PREDICTOS_H_

// Place in ITCM. Applies to functions.
#define PDOS_ITCM __attribute__((section(".itcm")))
// Place in DTCM. Applies to variables.
#define PDOS_DTCM __attribute__((section(".dtcm")))

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

// fully preemptive / fully non-preemptive
#define ALLOW_PREEMPTION (1U)

// task function. should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

// task handle
typedef struct PdOSTaskControlBlock *PdOSTaskHandle;

void PdOS_on_idle(void);  // weak function to overwrite
// stack size needs to be at least 64 bytes (16 words)
PdOSErrCode PdOS_init(void *idleStkSto, uint32_t idleStkSize);
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize);
void PdOS_run(void);
uint32_t PdOS_get_systime(void);
void PdOS_delay(uint32_t ticks);
void PdOS_delayUntil(uint32_t *prevWkUpTime, uint32_t period);

#endif /* SRC_PREDICTOS_H_ */
