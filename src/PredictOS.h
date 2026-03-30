/**
 * @file PredictOS.h
 * @author Yi Ren
 * @brief Header file of Predict OS
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
	PDOS_ERROR = 1U
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

/**
 * @brief Default weak function on idle (to be overwritten).
 */
void PdOS_on_idle(void);

/**
 * @brief Initialize OS.
 * 
 * @param idleStkSto pointer to idle stack
 * @param idleStkSize idle stack size
 * @retval PDOS_OK success
 * @retval PDOS_ERROR fail
 * 
 * @note stack size needs to be at least 64 bytes (16 words)
 */
PdOSErrCode PdOS_init(void *idleStkSto, uint32_t idleStkSize);

/**
 * @brief Create a new task and return its handle.
 * 
 * @param taskFunction pointer to task function
 * @param prio task priority (1~32, higher value for higher priority)
 * @param stkSto pointer to task stack
 * @param stkSize stack size
 * @param memSize maximum core-local memory usage
 * @return PdOSTaskHandle task handle. return NULL on failure
 * 
 * @note stack size needs to be at least 64 bytes (16 words)
 */
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize, uint32_t memSize);

/**
 * @brief Set OS to run.
 */
void PdOS_run(void);

/**
 * @brief Get current system time.
 * @return uint32_t current system time
 */
uint32_t PdOS_get_systime(void);

/**
 * @brief Busy wait for a relative time (keep CPU).
 * 
 * @param ticks delay time in ticks
 */
void PdOS_busy_wait(uint32_t ticks);

/**
 * @brief Delay for a relative time (yield CPU).
 * 
 * @param ticks delay time in ticks
 */
void PdOS_delay(uint32_t ticks);

/**
 * @brief Delay until a specific absolute time (yield CPU).
 * 
 * @param prevWkUpTime previous wake up time. should be initialized by PdOS_get_systime
 * @param period task period
 */
void PdOS_delayUntil(uint32_t *prevWkUpTime, uint32_t period);

/**
 * @brief Get the start address of available core-local memory for tasks.
 * 
 * @return void* start address
 */
void *PdOS_get_local_mem_addr(void);

/**
 * @brief Copy data from main memory to core-local memory.
 */
void PdOS_read(void);

/**
 * @brief Copy data from core-local memory back to main memory.
 */
void PdOS_write(void);

/**
 * @brief Add an entry to log.
 * 
 * @param logEvent log event
 */
void PdOS_add_log(PdOSLogEventType logEvent);

#endif /* SRC_PREDICTOS_H_ */
