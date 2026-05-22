/**
 * @file PredictOS.h
 * @author Yi Ren
 * @brief Header file of Predict OS
 */

#ifndef SRC_PREDICTOS_H_
#define SRC_PREDICTOS_H_

// place in ITCM. applies to functions
#define PDOS_ITCM __attribute__((section(".itcm")))
// place in DTCM. applies to variables
#define PDOS_DTCM __attribute__((section(".dtcm")))

// use memory space preserved in linker script
#define PDOS_CORE1_MAIN_MEM_BASE    (0x2401C000U)
#define PDOS_CORE1_MAIN_MEM_SIZE    (0x2000U)

#define PDOS_CORE2_MAIN_MEM_BASE    (0x2401E000U)
#define PDOS_CORE2_MAIN_MEM_SIZE    (0x2000U)

#define PDOS_ARB_SHM_BASE           (0x2403C000U)

// error code
typedef enum
{
    PDOS_OK             = 0U,
    PDOS_ERROR          = 1U,
    PDOS_INVALID_PARAM  = 2U
} PdOSErrCode;

// maximum task number. should not exceed 32
#define PDOS_MAX_TASK_NUM           (32U)

// systick frequency
#define PDOS_TICKS_PER_SECOND       (1000U)

// memory pool size (in bytes)
#define PDOS_MEM_POOL_SIZE          (4096U)

// maximum size of core-local memory buffer (in bytes)
#define PDOS_LOCAL_MEM_SIZE         (1000U)

#define PDOS_ENABLE_SCHED_LOG       (1)
// maximum number of log entries. note: each entry requires 8 bytes
#define PDOS_MAX_SCHED_LOG_NUM      (256U)

// 1: limited preemptive / 0: fully non-preemptive
#define PDOS_ENABLE_PREEMPTION      (1)

// dummy cycles per millisecond
#define PDOS_DUMMY_CYC_PER_MS       (33500U)

// number of dummy cycles before retry when arbitration lose
#define PDOS_ARBITER_BACKOFF_ITER   (3350U)    // approx. 0.1ms

// number of dummy cycles for waiting another core to register
#define PDOS_ARBITER_WAIT_WINDOW    (376U)

// halt the kernel at designated time
#define PDOS_USE_STOP_TIME          (1)
#define PDOS_STOP_TIME              (2000U)

// enable evaluations
#define PDOS_ENABLE_EVAL            (0)
// maximun number of switch time record
#define PDOS_MAX_EVAL_LOG_NUM       (200U)

// task function. should take no arguments and return void
typedef void (*PdOSTaskFunction)(void);

/**
 * @brief Initialize OS.
 * 
 * @retval PDOS_OK success
 * @retval PDOS_ERROR fail
 */
PdOSErrCode PdOS_init(void);

/**
 * @brief Create a new task.
 * 
 * @param taskFunction pointer to task function
 * @param prio task priority (1~32, higher value for higher priority)
 * @param threshold preemption threshold (>= priority)
 * @param stkSto pointer to task stack
 * @param stkSize stack size (in bytes)
 * @param memSize maximum core-local memory usage (in bytes)
 * @retval PDOS_OK success
 * @retval PDOS_ERROR fail
 * @retval PDOS_INVALID_PARAM invalid parameters
 * 
 * @note stack size needs to be at least 64 bytes
 */
PdOSErrCode PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, uint8_t threshold, void *stkSto, uint32_t stkSize, uint32_t memSize);

/**
 * @brief Set the OS to run.
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
 * @param prevWkUpTime previous wake-up time. should be initialized by user
 * @param period task period
 * 
 * @note period should not exceed 65535 for log recording.
 *         period greater than 65535 still works, but log will fail
 */
void PdOS_delayUntil(uint32_t *prevWkUpTime, uint32_t period);

/**
 * @brief Copy data from main memory to core-local memory.
 * 
 * @param extraDelayTime extra delay time
 * @return void* start address of available core-local memory for tasks
 * 
 * @note only call this function at the beginning of the task
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

/**
 * @brief Init arbiter for shared memory access.
 * 
 * @note should be called by the main core before starting the other core
 */
void PdOS_arbiter_init(void);

#endif /* SRC_PREDICTOS_H_ */
