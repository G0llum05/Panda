# PandOSsh 2025/2026

Implementation up to phase2 of the PandOSsh project of the Operating Systems course at Bologna University.

## How to Build & Run

Make sure you have the GCC-RISCV toolchain, `cmake`, `make` and the uriscv emulator installed locally on your system.

```bash
cmake -S . -B build
cmake --build build
uriscv
```

A GUI will pop up and you'll need to upload the config_machine.json specific to your machine.

## Implementation choices

We compartmentalized the code in order to properly define the project structure and separate logical components into different modules. To achieve this we used static functions throughout the project.

It was decided to only include non-blocking exceptions execution time in the running process accumulated CPU time, ultimately excluding blocking system calls (i.e. PASSEREN, DOIO, YIELD, CLOCKWAIT) and interrupts.

A functionality in which great thought was put into has been the termination of processes and their progeny. We opted for a deque-like data structure implemented using an array of MAXPROC size, as it had the great benefit of terminating processes in the most appropriate fashion (i.e. reverse order).