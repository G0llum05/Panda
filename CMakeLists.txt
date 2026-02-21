cmake_minimum_required(VERSION 3.25)
project(MultiPandOS LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (EXISTS "/usr/local/bin/riscv64-unknown-elf-gcc")
  set(XT_PRG_PREFIX riscv64-unknown-elf-)
elseif (EXISTS "/usr/bin/riscv64-elf-gcc")
  set(XT_PRG_PREFIX riscv64-elf-)
else ()
  message(FATAL_ERROR "Missing riscv64-unknown-elf toolchain")
endif ()
if (EXISTS "/usr/local/share/uriscv")
  set(URISCV_SRC /usr/local/share/uriscv)
  set(URISCV_INC /usr/local/include)
elseif (EXISTS "/usr/share/uriscv")
  # TODO: Check for arch based systems!
  set(URISCV_SRC /usr/share/uriscv)
  set(URISCV_INC /usr/local/include)
else ()
  message(FATAL_ERROR "Missing uriscv lib")
endif ()


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
