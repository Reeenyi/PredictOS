## Makefile
### Disable FPU
Add parameter `CONFIG_FPU=no` to disable the floating-point unit.
This is to simplify the stack usage of ARMv7-M.

**Before:**
```makefile
$(AT)$(MAKE) CONFIG_TARGET_CORE=core1 CONFIG_TARGET_MEMORY=nvm default-all
```

**After:**
```makefile
$(AT)$(MAKE) CONFIG_TARGET_CORE=core1 CONFIG_TARGET_MEMORY=nvm default-all CONFIG_FPU=no
```

### Use custom linker script

We use a custom linker scripy `PredictOS-linker-script-gcc-m7.ld.E` to handle the usage of ITCM and DTCM.
Modify the makefile to use it.

**Add:**
```makefile
LINKER_SCRIPT_SOURCE := PredictOS-linker-script-gcc-m7.ld.E
```


## Linker script
We copy the linker file from `SDKS/StellarESDK-1.7.0/Projects/SDKTests/CommonBuild/rsc/application-gcc-m7.ld.E` and modify it.


### Make SYSTICK and PredictOS run in Instruction TCM
`Systick_Handler` is frequently used in the RTOS kernel and needs to run in ITCM to avoid contention for shared memory.
However, `Systick_Handler` is implemented in the `SYSTICK` driver of the SDK, so we need to place the driver into ITCM, together with `PredictOS`.

We move the .itcm section before .text to ensure SYSTICK and PredictOS are placed in ITCM instead of the default .text section.

**Before:**
around line 85
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

**After:**
around line 47
```ld
.itcm ORIGIN(EMBED_ITCM) : ALIGN(16)
{
    __itcm__ = . ;
    *systick*.o(.text*)
    *PredictOS*.o(.text*)
} > EMBED_ITCM AT> EMBED_NVM
__itcm_start__ = ORIGIN(EMBED_ITCM);
__itcm_len__   = LENGTH(EMBED_ITCM);
__itcm_end__   = __itcm_start__ + __itcm_len__;
__itcm_size__  = SIZEOF(.itcm);
__itcm_load__  = LOADADDR(.itcm);
```


### Set MSP to Data TCM

RTOS kernel utilizes interrupts to operate. Set MSP to DTCM to make interrupt stacks stay in core-local memory.

**Before:**
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

**After:**
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
