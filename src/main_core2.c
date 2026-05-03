/**
 * @file main_core2.c
 * @author Yi Ren
 * @brief Application code for core 2
 */

#include <test_env.h>
#include "PredictOS.h"

#define TASK_LIST                                                                                     \
    /*ID, MAGIC, STK_SIZE, MEM_USAGE, PRIORITY, THERSHOLD, READ_TIME, EXEC_TIME, WRITE_TIME, PERIOD*/ \
    X(1 , 66U  , 128U    , 100U     , 5U      , 5U       , 2U       , 5U       , 2U        , 30U    ) \
    X(2 , 77U  , 128U    , 200U     , 3U      , 3U       , 6U       , 10U      , 5U        , 120U   )

#define DEFINE_TASK(ID, MAGIC_NUMBER, STACK_SIZE, MEMORY_USAGE, READ_TIME, EXECUTE_TIME, WRITE_TIME, PERIOD) \
                                                                \
PDOS_DTCM uint32_t task##ID##Stack[STACK_SIZE];                 \
                                                                \
PDOS_ITCM void task##ID##_func(void)                            \
{                                                               \
    uint32_t i;                                                 \
    uint32_t prevWkUpTime = 0U;                                 \
    uint8_t *localMem;                                          \
    uint8_t magicNum;                                           \
    uint8_t prevMagicNum = MAGIC_NUMBER;                        \
                                                                \
    /* first iteration */                                       \
    localMem = PdOS_read(READ_TIME);                            \
    for (i = 0U; i < MEMORY_USAGE; i++)                         \
    {                                                           \
        localMem[i] = prevMagicNum;                             \
    }                                                           \
    PdOS_busy_wait(EXECUTE_TIME);                               \
    PdOS_write(WRITE_TIME);                                     \
    PdOS_delayUntil(&prevWkUpTime, PERIOD);                     \
                                                                \
    while (1)                                                   \
    {                                                           \
        localMem = PdOS_read(READ_TIME);                        \
                                                                \
        magicNum = prevMagicNum + 1U;                           \
        for (i = 0U; i < MEMORY_USAGE; i++)                     \
        {                                                       \
            if (localMem[i] != prevMagicNum)                    \
            {                                                   \
                /* data unexpected */                           \
                while (1);                                      \
            }                                                   \
            localMem[i] = magicNum;                             \
        }                                                       \
        prevMagicNum = magicNum;                                \
        PdOS_busy_wait(EXECUTE_TIME);                           \
                                                                \
        PdOS_write(WRITE_TIME);                                 \
                                                                \
        PdOS_delayUntil(&prevWkUpTime, PERIOD);                 \
    }                                                           \
}

#define X(ID, MAGIC_NUMBER, STACK_SIZE, MEMORY_USAGE, PRIORITY, THRESHOLD, READ_TIME, EXECUTE_TIME, WRITE_TIME, PERIOD) \
    DEFINE_TASK(ID, MAGIC_NUMBER, STACK_SIZE, MEMORY_USAGE, READ_TIME, EXECUTE_TIME, WRITE_TIME, PERIOD)

TASK_LIST
#undef X

int main(void)
{
    test_env_init((TestInit_t)(TEST_INIT_IRQ));

    PdOS_init();

#define X(ID, MAGIC_NUMBER, STACK_SIZE, MEM_USAGE, PRIORITY, THRESHOLD, READ_TIME, EXECUTE_TIME, WRITE_TIME, PERIOD) \
    if (PdOS_create_task(&task##ID##_func, PRIORITY, THRESHOLD, task##ID##Stack, sizeof(task##ID##Stack), MEM_USAGE) != PDOS_OK) \
    {               \
        while (1);  \
    }

TASK_LIST
#undef X

    PdOS_run();
}
