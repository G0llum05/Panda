cmake_minimum_required(VERSION 3.25)
project(MultiPandOS LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Look up RISCV GCC compiler path
find_program(RISCV_GCC
  NAMES riscv64-unknown-elf-gcc riscv64-elf-gcc
  PATHS /usr/local/bin /usr/bin
  NO_DEFAULT_PATH
  REQUIRED
)

find_program(RISCV_LD
  NAMES riscv64-unknown-elf-ld riscv64-elf-ld
  PATHS /usr/local/bin /usr/bin
  NO_DEFAULT_PATH
  REQUIRED
)

if(RISCV_GCC MATCHES "riscv64-unknown-elf-gcc")
  set(XT_PRG_PREFIX "riscv64-unknown-elf-")
elseif(RISCV_GCC MATCHES "riscv64-elf-gcc")
  set(XT_PRG_PREFIX "riscv64-elf-")
endif()

# Find uriscv source and include directories
find_path(URISCV_SRC uriscvcore.ldscript
  PATHS /usr/local/share/uriscv /usr/share/uriscv
  REQUIRED
)

find_path(URISCV_INC uriscv/liburiscv.h # liburiscv.h as an identifier
  PATHS /usr/local/include /usr/include
  REQUIRED
)

find_path(URISCV_LIB crti.o
  PATHS /usr/local/lib/uriscv /usr/lib/uriscv
  REQUIRED
)

set(CMAKE_C_COMPILER ${RISCV_GCC})
set(CMAKE_ASM_COMPILER ${RISCV_GCC})
set(CMAKE_C_LINKER ${RISCV_LD})

add_link_options(-nostdlib -T ${URISCV_SRC}/uriscvcore.ldscript -march=rv32imfd)
add_compile_options(-ffreestanding -static -nostartfiles -nostdlib -I${URISCV_INC} -ggdb -Wall -O0 -std=gnu99 -march=rv32imafd -mabi=ilp32d)

file(GLOB PHASE1_SRC "phase1/*.c")
file(GLOB PHASE2_SRC "phase2/*.c")
file(GLOB PHASE3_SRC "phase3/*.c")

add_executable(MultiPandOS
    ${PHASE1_SRC}
    ${PHASE2_SRC}
    ${PHASE3_SRC}
    ${URISCV_SRC}/crtso.S
    ${URISCV_SRC}/liburiscv.S
)

add_custom_target(
	MultiPandOSuRISCV ALL
	COMMAND uriscv-elf2uriscv -k ${PROJECT_BINARY_DIR}/MultiPandOS
	BYPRODUCTS MultiPandOS.core.uriscv MultiPandOS.stab.uriscv
	DEPENDS MultiPandOS
)

set(GENERATED_URISCV_FILES
    testers/*.uriscv
)

add_custom_command(
  OUTPUT ${GENERATED_URISCV_FILES}
  COMMAND ${CMAKE_COMMAND} -E env
    URISCV_LIB=${URISCV_LIB}
    URISCV_INC=${URISCV_INC}
    URISCV_SRC=${URISCV_SRC}
    XT_PRG_PREFIX=${XT_PRG_PREFIX}
    make -f ${CMAKE_SOURCE_DIR}/testers/Makefile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/testers
)

add_custom_target(testers ALL
    DEPENDS ${GENERATED_URISCV_FILES}
)
