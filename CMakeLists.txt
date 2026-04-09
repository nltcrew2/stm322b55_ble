cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)

project(STM32PROJECT)

enable_language(C CXX ASM)

set(TARGET_NAME ${PROJECT_NAME})
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# =========================================================
# CPU / MCU
# =========================================================
set(CPU_PARAMETERS
    -mthumb
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(MCU_FAMILY STM32WBxx)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32WB55CGUX_FLASH.ld)

if(NOT EXISTS ${LINKER_SCRIPT})
    message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
endif()

# =========================================================
# Helper: collect all subdirectories recursively
# =========================================================
function(collect_subdirectories result base_dir)
    set(dir_list ${base_dir})

    file(GLOB_RECURSE children LIST_DIRECTORIES true "${base_dir}/*")

    foreach(child ${children})
        if(IS_DIRECTORY ${child})
            list(APPEND dir_list ${child})
        endif()
    endforeach()

    list(REMOVE_DUPLICATES dir_list)
    set(${result} ${dir_list} PARENT_SCOPE)
endfunction()

collect_subdirectories(LIB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/Lib)

# =========================================================
# Source files
# =========================================================
file(GLOB ROOT_C_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

file(GLOB_RECURSE CORE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/*.c
)

file(GLOB_RECURSE DRIVER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/*.c
)

file(GLOB_RECURSE MIDDLEWARE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/*.c
)

file(GLOB_RECURSE UTILITIES_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/*.c
)

file(GLOB_RECURSE APP_C_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/App/*.c
)

file(GLOB_RECURSE APP_CPP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/App/*.cpp
)

file(GLOB_RECURSE TASK_C_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Tasks/*.c
)

file(GLOB_RECURSE TASK_CPP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Tasks/*.cpp
)

file(GLOB_RECURSE LIB_C_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/*.c
)

file(GLOB_RECURSE LIB_CPP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/*.cpp
)

file(GLOB_RECURSE WPAN_APP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32_WPAN/App/*.c
)

file(GLOB_RECURSE WPAN_TARGET_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32_WPAN/Target/*.c
)

file(GLOB_RECURSE ASM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/*.s
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/*.S
)

set(SRC_PATHS
    ${ROOT_C_SRC}
    ${CORE_SRC}
    ${DRIVER_SRC}
    ${MIDDLEWARE_SRC}
    ${UTILITIES_SRC}
    ${APP_C_SRC}
    ${APP_CPP_SRC}
    ${TASK_C_SRC}
    ${TASK_CPP_SRC}
    ${LIB_C_SRC}
    ${LIB_CPP_SRC}
    ${WPAN_APP_SRC}
    ${WPAN_TARGET_SRC}
    ${ASM_SRC}
)

# =========================================================
# Include directories
# =========================================================
set(INC_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Tasks
    ${LIB_INCLUDE_DIRS}

    ${CMAKE_CURRENT_SOURCE_DIR}/STM32_WPAN/App
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32_WPAN/Target

    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/${MCU_FAMILY}_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/${MCU_FAMILY}_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/${MCU_FAMILY}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/RTOS2/Include

    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F

    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/template
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci

    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/conf
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/lpm/tiny_lpm
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/sequencer
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/timserv
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities/misc
)

# =========================================================
# Preprocessor definitions
# =========================================================
set(PREDEFINED_SYM
    _DEBUG
    USE_HAL_DRIVER
    STM32WB55xx
    ARM_MATH_CM4
)

# =========================================================
# Target
# =========================================================
add_executable(${TARGET_NAME} ${SRC_PATHS})

target_include_directories(${TARGET_NAME}
    PRIVATE ${INC_DIRS}
)

target_compile_definitions(${TARGET_NAME}
    PRIVATE ${PREDEFINED_SYM}
)

target_compile_options(${TARGET_NAME}
    PRIVATE
        ${CPU_PARAMETERS}
        -ffunction-sections
        -fdata-sections
        -fstack-usage
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Og
        -g3
        -ggdb
)

target_link_options(${TARGET_NAME}
    PRIVATE
        ${CPU_PARAMETERS}
        -T${LINKER_SCRIPT}
        -Wl,-Map=${TARGET_NAME}.map
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        --specs=nosys.specs
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        c
        m
        stdc++
        supc++
)

# =========================================================
# Post build
# =========================================================
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${TARGET_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.hex
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${TARGET_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.bin
)