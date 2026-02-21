cmake_minimum_required(VERSION 3.25)
project(MultiPandOS LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Look up RISCV GCC compiler path
find_program(RISCV_GCC riscv64-unknown-elf-gcc
  PATHS /usr/local/bin /usr/bin
  NO_DEFAULT_PATH REQUIRED
)

set(XT_PRG_PREFIX "riscv64-unknown-elf-")  # Since we looked up gcc already

# Find uriscv source and include directories
find_path(URISCV_SRC uriscvcore.ldscript
  PATHS /usr/local/share/uriscv /usr/share/uriscv
  REQUIRED
)

find_path(URISCV_INC uriscv/liburiscv.h # liburiscv.h as an identifier
  PATHS /usr/local/include /usr/include
  REQUIRED
)

set(CMAKE_C_COMPILER ${XT_PRG_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${XT_PRG_PREFIX}gcc)
set(CMAKE_C_LINKER ${XT_PRG_PREFIX}ld)

add_link_options(-nostdlib -T ${URISCV_SRC}/uriscvcore.ldscript -march=rv32imfd)
add_compile_options(-ffreestanding -static -nostartfiles -nostdlib -I${URISCV_INC} -ggdb -Wall -O0 -std=gnu99 -march=rv32imafd -mabi=ilp32d)

file(GLOB PHASE1_SRC "phase1/*.c")
file(GLOB PHASE2_SRC "phase2/*.c")
add_executable(MultiPandOS
    ./klog.c
    ${PHASE1_SRC}
    ${PHASE2_SRC}
    ${URISCV_SRC}/crtso.S
    ${URISCV_SRC}/liburiscv.S
)

add_custom_target(
	MultiPandOSuRISCV ALL
	COMMAND uriscv-elf2uriscv -k ${PROJECT_BINARY_DIR}/MultiPandOS
	BYPRODUCTS MultiPandOS.core.uriscv MultiPandOS.stab.uriscv
	DEPENDS MultiPandOS
)
