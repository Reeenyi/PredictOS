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

// maximum task number. should be no more than 32
#define MAX_TASK_NUM (32U)

// systick frequency
#define TICKS_PER_SECOND (1000U)

// memory pool size in bytes
#define MEM_POOL_SIZE (4096U)

// maximum size of core-local memory buffer
#define MAX_LOCAL_MEM_SIZE (1000U)

// maximum number of logs
#define MAX_LOG_NUM (100U)

// fully preemptive / fully non-preemptive
#define ALLOW_PREEMPTION (1U)

// 1 for using dummy cycles / 0 for waiting systick in busy wait
#define USE_BUSY_WAIT_DUMMY (1U)

// dummy cycles per second
#define DUMMY_CYC_PER_SEC (37594U)

// task function. should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

// task handle
typedef struct PdOSTaskControlBlock *PdOSTaskHandle;

/**
 * @brief Initialize OS.
 * 
 * @retval PDOS_OK success
 * @retval PDOS_ERROR fail
 */
PdOSErrCode PdOS_init(void);

/**
 * @brief Create a new task and return its handle.
 * 
 * @param taskFunction pointer to task function
 * @param prio task priority (1~32, higher value for higher priority)
 * @param threshold preemption threshold (no less than priority)
 * @param stkSto pointer to task stack
 * @param stkSize stack size
 * @param memSize maximum core-local memory usage
 * @return PdOSTaskHandle task handle. return NULL on failure
 * 
 * @note stack size needs to be at least 64 bytes (16 words)
 */
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, uint8_t threshold, void *stkSto, uint32_t stkSize, uint32_t memSize);

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
 * @brief Copy data from main memory to core-local memory.
 * 
 * @param extraDelayTime extra delay time
 * @return void* start address of available core-local memory for tasks
 */
void *PdOS_read(uint32_t extraDelayTime);

/**
 * @brief Copy data from core-local memory back to main memory.
 * 
 * @param extraDelayTime extra delay time
 * 
 * @note only call this function at the end of the task (before delayUntil)
 */
void PdOS_write(uint32_t extraDelayTime);

#endif /* SRC_PREDICTOS_H_ */
