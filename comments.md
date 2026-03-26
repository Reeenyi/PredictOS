## Makefile
### Disable FPU
Add parameter `CONFIG_FPU=no` to disable the floating-point unit.
This is to simplify the stack usage of ARMv7-M.

*Before:*
```makefile
$(AT)$(MAKE) CONFIG_TARGET_CORE=core1 CONFIG_TARGET_MEMORY=nvm default-all
```

*After:*
```makefile
$(AT)$(MAKE) CONFIG_TARGET_CORE=core1 CONFIG_TARGET_MEMORY=nvm default-all CONFIG_FPU=no
```

### Use custom linker script

We use a custom linker scripy `PredictOS-linker-script-gcc-m7.ld.E` to handle the usage of ITCM and DTCM.
Modify the makefile to use it.

*Add:*
```makefile
LINKER_SCRIPT_SOURCE := PredictOS-linker-script-gcc-m7.ld.E
```


## Linker script
We copy the linker file from `SDKS/StellarESDK-1.7.0/Projects/SDKTests/CommonBuild/rsc/application-gcc-m7.ld.E` and modify it.

### Set MSP to Data TCM

RTOS kernel utilizes interrupts to operate. Set MSP to DTCM to make interrupt stacks stay in core-local memory.

*Before:*
```
.mstack (NOLOAD) :
{
    . = ALIGN(8);
    __main_stack_base__ = .;
    . += MAIN_STACK_SIZE;
    . = ALIGN(8);
    __main_stack_end__ = .;
} > EMBED_RAM
```

*After:*
```
.mstack (NOLOAD) :
{
    . = ALIGN(8);
    __main_stack_base__ = .;
    . += MAIN_STACK_SIZE;
    . = ALIGN(8);
    __main_stack_end__ = .;
} > EMBED_DTCM
```

### Modify memory layout

#### Place SYSTICK and PredictOS in Instruction TCM
`Systick_Handler` is frequently used in the RTOS kernel and needs to run in ITCM to avoid contention for shared memory.
However, `Systick_Handler` is implemented in the `SYSTICK` driver of the SDK, so we need to place the driver into ITCM, together with `PredictOS`.

We move them from the default `.text` section to `.ictm` to do so.

#### Place interrupt vector table in Instruction TCM

The interrupt vector is placed in main memory by default.
We need to place it in ICTM to avoid contention for shared memory when interrupts occur.

We delete `KEEP(*(.vectors))` in section `startup`, and place it in `.ictm` instead.
`KEEP(*(.reset_handler))` in section `startup` must not be replaced.


*Before:*
```
startup : ALIGN(16) SUBALIGN(1024)
{
    KEEP(*(.vectors))
    KEEP(*(.reset_handler))
} > EMBED_NVM
```

*After:*
```
startup : ALIGN(16) SUBALIGN(1024)
{
    KEEP(*(.reset_handler))
} > EMBED_NVM
```

*Before:*
```ld
.text : ALIGN(16) SUBALIGN(16)
{
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
    *(.glue_7t)
    *(.glue_7)
    *(.gcc*)
} > EMBED_NVM
```

*After:*
```ld
.text : ALIGN(16) SUBALIGN(16)
{
    *(EXCLUDE_FILE(*systick*.o *PredictOS*.o) .text)
    *(EXCLUDE_FILE(*systick*.o *PredictOS*.o) .text.*)
    *(.rodata)
    *(.rodata.*)
    *(.glue_7t)
    *(.glue_7)
    *(.gcc*)
} > EMBED_NVM
```

*Before:*
```ld
.itcm ORIGIN(EMBED_ITCM) : ALIGN(16)
{
    __itcm__ = . ;
} > EMBED_ITCM AT> EMBED_NVM
__itcm_start__ = ORIGIN(EMBED_ITCM);
__itcm_len__   = LENGTH(EMBED_ITCM);
__itcm_end__   = __itcm_start__ + __itcm_len__;
__itcm_size__  = SIZEOF(.itcm);
__itcm_load__  = LOADADDR(.itcm);
```

*After:*
```ld
.itcm ORIGIN(EMBED_ITCM) : ALIGN(16)
{
    __itcm__ = . ;
    KEEP(*(.vectors))
    *systick*.o(.text*)
    *PredictOS*.o(.text*)
} > EMBED_ITCM AT> EMBED_NVM
__itcm_start__ = ORIGIN(EMBED_ITCM);
__itcm_len__   = LENGTH(EMBED_ITCM);
__itcm_end__   = __itcm_start__ + __itcm_len__;
__itcm_size__  = SIZEOF(.itcm);
__itcm_load__  = LOADADDR(.itcm);
```


### SDK

It is very proud to say that we have avoided modifications on the SDK.
It was an alternative to modify `vector.S` and `SYSTICK` driver in the SDK, but we managed to fix the issue by customizing the linker file.