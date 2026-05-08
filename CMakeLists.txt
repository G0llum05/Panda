cmake_minimum_required(VERSION 3.25)
project(MultiPandOS LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Look up RISCV GCC compiler path
find_program(RISCV_GCC riscv64-unknown-elf-gcc
  PATHS /usr/local/bin /usr/bin
  NO_DEFAULT_PATH
)
if(NOT RISCV_GCC)
  find_program(RISCV_GCC riscv64-elf-gcc
    PATHS /usr/local/bin /usr/bin
    NO_DEFAULT_PATH
  )
endif()

if(RISCV_GCC MATCHES "riscv64-unknown-elf-gcc")
  set(XT_PRG_PREFIX "riscv64-unknown-elf-")
elseif(RISCV_GCC MATCHES "riscv64-elf-gcc")
  set(XT_PRG_PREFIX "riscv64-elf-")
else()
  message(FATAL_ERROR "RISCV GCC not found")
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

set(CMAKE_C_COMPILER ${XT_PRG_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${XT_PRG_PREFIX}gcc)
set(CMAKE_C_LINKER ${XT_PRG_PREFIX}ld)

add_link_options(-nostdlib -T ${URISCV_SRC}/uriscvcore.ldscript -march=rv32imfd)
add_compile_options(-ffreestanding -static -nostartfiles -nostdlib -I${URISCV_INC} -ggdb -Wall -O0 -std=gnu99 -march=rv32imafd -mabi=ilp32d)

file(GLOB PHASE1_SRC "phase1/*.c")
file(GLOB PHASE2_SRC "phase2/*.c")
file(GLOB PHASE3_SRC "phase3/*.c")
file(GLOB PHASE3_TEST_SRC "testers/*.c")
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

file(GLOB TESTERS_SRC "testers/*.c")

foreach(TEST_SRC ${TESTERS_SRC})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    if((NOT TEST_NAME STREQUAL "print") AND (NOT TEST_NAME STREQUAL "string"))
        add_executable(${TEST_NAME}
           ${TEST_SRC}
           testers/string.c
           testers/print.c
           ${URISCV_SRC}/crtso.S
           ${URISCV_SRC}/liburiscv.S
        )
        # Apply same compile/link flags as before
        target_compile_options(${TEST_NAME} PRIVATE
            -ffreestanding -static -nostdlib -I${URISCV_INC} -ggdb -Wall -O0 -std=gnu99 -march=rv32imafd -mabi=ilp32d
        )
        target_link_options(${TEST_NAME} PRIVATE
            -nostdlib -T ${URISCV_SRC}/uriscvcore.ldscript -march=rv32imfd
        )

        set_target_properties(${TEST_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/testers"
        )

        # Generate the .uriscv file in testers/
        add_custom_command(
            TARGET ${TEST_NAME}
            POST_BUILD
            COMMAND uriscv-mkdev -f "${PROJECT_SOURCE_DIR}/testers/${TEST_NAME}.uriscv" "${PROJECT_SOURCE_DIR}/testers/${TEST_NAME}"
            BYPRODUCTS "${PROJECT_SOURCE_DIR}/testers/${TEST_NAME}.uriscv"
            COMMENT "Generating ${TEST_NAME}.uriscv"
        )
    endif()
endforeach()
