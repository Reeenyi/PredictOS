# PredictOS
This repository contains the complete source code of the thesis titled *An RTOS for the Limited-Preemptive Three-Phase Execution Model - Implementation and Evaluation on a COTS Multi-Core Platform*.

## Introduction

PredictOS is an RTOS implemented from scratch for the theoretical model on a dual-core Arm Cortex-M7 platform.

The idea of the three-phase execution model is to structure task execution into a read phase, an execution phase, and a write phase, thereby reducing contention for shared resources, which is the main memory in this project. Limited preemption in this project refers to escalating the priority of an executing task to its preemption threshold, and it cannot be preempted by tasks with lower priority.

PredictOS supports concurrent task execution by context switches and the blocking mechanism. Tasks are denoted by their unique priority and scheduled in a partitioned fixed-priority manner. The RTOS kernel can be configured to operate in either fully non-preemptive mode or limited-preemptive mode.

When in limited-preemptive mode, preemptions comply with the theoretical model. This means that preemptions are only allowed when:
- The currently executing task is not in its memory phase (read/write phase);
- The candidate task has a nominal priority greater than the preemption threshold of the currently executing task;
- Core-local memory has enough space for the candidate task's intermediate data.

Each task is assigned a dedicated memory slice for runtime data. On task creation, a memory slice in the memory pool in main memory is allocated to the task. When a task is scheduled to run, its memory slice is copied to the core-local memory (read phase), and it executes without accessing the main memory (execution phase). When execution finishes, the memory slice of the task is copied back to the main memory (write phase). If a task is preempted, its memory slice is kept in the core-local memory with the slice of the preempting task placed on top, similar to a stack structure.

Two cores execute in parallel, with each core having its fixed task set. At most one memory phase is allowed at a time. Arbitration between the two cores is based on the nominal priority of the currently executing task.

For more details about the implementation, please refer to Chapter 5 in the thesis.


## Repository structure

```text
PredictOS/
├── src/
│   ├── PredictOS.c
│   ├── PredictOS.h
│   ├── main_core1.c
│   └── main_core2.c
├── visualization_tool/
│   ├── parseSchedLog.py
│   └── timeEval.py
├── Makefile
├── PredictOS-linker-script-gcc-m7.ld.E
├── readme.md
└── thesis.pdf
```

### Source files

- `PredictOS.c`
Implementation of the RTOS kernel. Includes basic mechanisms for task execution, memory management, scheduler implementation, and arbitration of main memory access.

- `PredictOS.h`
Header file of the RTOS kernel. Includes configuration options of the RTOS kernel and public interfaces for the application layer.

- `main_core1.c`
Demo application code for core 1.

- `main_core2.c`
Demo application code for core 2.

### Visualization tools

- `parseSchedLog.py`
Visualization tool for parsing and visualizing scheduling event logs.

- `timeEval.py`
Tool for analyzing task switching times and drawing box plots.

### Build system

- `Makefile`
Build script of the project.

- `PredictOS-linker-script-gcc-m7.ld.E`
Linker script with customized memory layout.

### Full thesis

- `thesis.pdf`
The full thesis.


## Environment

- Target platform: STM Stellar SR5E1E7 with EVBE7000E extension board
- IDE: StellarStudio 7.0.0
- SDK: StellarESDK-1.7.0

## Build

1. Create a new StellarE SDK (1.7.0) project in StellarStudio.
2. Delete the auto-generated `src` directory and `Makefile` in the new project.
3. Copy the `src` directory from this repository to the new project.
4. Copy the `Makefile` and the linker script to the new project.
5. Build the project in StellarStudio.