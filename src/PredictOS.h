/*
 * PredictOS.h
 */

#ifndef SRC_PREDICTOS_H_
#define SRC_PREDICTOS_H_

// Place in ITCM. Applies to functions.
#define PDOS_ITCM __attribute__((section(".itcm")))
// Place in DTCM. Applies to variables.
#define PDOS_DTCM __attribute__((section(".dtcm")))

// error code
typedef enum
{
	PDOS_OK = 0U,
	PDOS_ERROR = 1U,
	PDOS_INVALID_PARAM = 2U
} PdOSErrCode;

// log event
typedef enum _PdOSLogEventType
{
	PDOS_ENTER_READ = 0U,
	PDOS_EXIT_READ = 1U,
	PDOS_ENTER_EXECUTE = 2U,
	PDOS_EXIT_EXECUTE = 3U,
	PDOS_ENTER_WRITE = 4U,
	PDOS_EXIT_WRITE = 5U
}PdOSLogEventType;

// maximum task number. should be no more than 32
#define MAX_TASK_NUM (32U)

// systick frequency
#define TICKS_PER_SECOND (1000U)

// memory pool size in bytes
#define MEM_POOL_SIZE (4096U)

// maximum size of core-local memory buffer
#define MAX_LOCAL_MEM_SIZE (1024U)

// maximum number of logs
#define MAX_LOG_NUM (100U)

// fully preemptive / fully non-preemptive
#define ALLOW_PREEMPTION (0U)

// task function. should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

// task handle
typedef struct PdOSTaskControlBlock *PdOSTaskHandle;

void PdOS_on_idle(void);  // weak function to overwrite
// stack size needs to be at least 64 bytes (16 words)
PdOSErrCode PdOS_init(void *idleStkSto, uint32_t idleStkSize);
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize, uint32_t memSize);
void PdOS_run(void);
uint32_t PdOS_get_systime(void);
void PdOS_busy_wait(uint32_t ticks);
void PdOS_delay(uint32_t ticks);
void PdOS_delayUntil(uint32_t *prevWkUpTime, uint32_t period);
void *PdOS_get_local_mem_addr(void);
void PdOS_read(void);
void PdOS_write(void);
void PdOS_add_log(PdOSLogEventType logEvent);

#endif /* SRC_PREDICTOS_H_ */
