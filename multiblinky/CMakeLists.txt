cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

set(CMAKE_FILES ${CMAKE_SOURCE_DIR}/../cmake)
set(CMAKE_TOOLCHAIN_FILE    ${CMAKE_FILES}/toolchain-gcc-arm-embedded.cmake)

project(Multiblinky)

include(${CMAKE_FILES}/CPM_setup.cmake)


#-----------------------------------------------------------------------
# Build settings
#-----------------------------------------------------------------------

set(EXE_NAME                Multiblinky)
set(FLASH_ADDR              0x1A000000)
set(FLASH_CFG               lpc4337_swd)
set(DEBUG_BREAKPOINT_LIMIT  6)
set(DEBUG_WATCHPOINT_LIMIT  4)


# default settings
set(OPTIMIZE s)
set(BLACKMAGIC_DEV /dev/ttyBmpGdb)
set(POWER_TARGET "no")

# Include custom settings
# (if this file does not exist, copy it manually from config.cmake.example)
include(${CMAKE_SOURCE_DIR}/config.cmake)

message(STATUS "Config OPTIMIZE: ${OPTIMIZE}")
message(STATUS "Config BLACKMAGIC_DEV: ${BLACKMAGIC_DEV}")
message(STATUS "Config POWER_TARGET: ${POWER_TARGET}")

set(SYSTEM_LIBRARIES    m c gcc)

# M4 core has hardware floating point support
add_definitions(-D__FPU_PRESENT)
set(FLOAT_FLAGS "-mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(FLAGS_M4 "-mcpu=cortex-m4 ${FLOAT_FLAGS}")

set(C_FLAGS "-O${OPTIMIZE} -g3 -c -fmessage-length=80 -fno-builtin   \
    -ffunction-sections -fdata-sections -std=gnu99 -mthumb      \
    -fdiagnostics-color=auto")
set(C_FLAGS_WARN "-Wall -Wextra -Wno-unused-parameter           \
    -Wshadow -Wpointer-arith -Winit-self -Wstrict-overflow=5")

set(L_FLAGS "-fmessage-length=80 -nostdlib -specs=nano.specs \
    -mthumb -Wl,--gc-sections")

set(MCU_PLATFORM    43xx_m4)

add_definitions("${FLAGS_M4} ${C_FLAGS} ${C_FLAGS_WARN}")
add_definitions(-DCORE_M4 -DMCU_PLATFORM_${MCU_PLATFORM})


set(ELF_PATH            "${CMAKE_CURRENT_BINARY_DIR}/${EXE_NAME}")
set(EXE_PATH            "${ELF_PATH}.bin")
set(FLASH_FILE          ${PROJECT_BINARY_DIR}/flash.cfg)

#------------------------------------------------------------------------------
# CPM Modules
#------------------------------------------------------------------------------

CPM_AddModule("startup_lpc43xx_m4"
    GIT_REPOSITORY "https://github.com/JitterCompany/startup_lpc43xx_m4.git"
    GIT_TAG "1.2")

CPM_AddModule("lpc_tools"
    GIT_REPOSITORY "https://github.com/JitterCompany/lpc_tools.git"
    GIT_TAG "2.6.2")

CPM_AddModule("chip_lpc43xx_m4"
    GIT_REPOSITORY "https://github.com/JitterCompany/chip_lpc43xx_m4.git"
    GIT_TAG "3.3.0")

CPM_AddModule("c_utils"
    GIT_REPOSITORY "https://github.com/JitterCompany/c_utils.git"
    GIT_TAG "1.4.5")

CPM_AddModule("mcu_debug"
    GIT_REPOSITORY "https://github.com/JitterCompany/mcu_debug.git"
    GIT_TAG "2.1")

CPM_Finish()


get_property(startup_linker GLOBAL PROPERTY startup_linker)
message(STATUS "blinky_m4: startup_linker: ${startup_linker}")

set(LINKER_FILES "-L .. -T ${startup_linker}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${L_FLAGS} \
${LINKER_FILES} ${FLAGS_M4}")


#-----------------------------------------------------------------------
# Setup source
#-----------------------------------------------------------------------


file(GLOB SOURCES
"src/*.c"
)

set(CMAKE_SYSTEM_NAME Generic)

#-----------------------------------------------------------------------
# Setup executable
#-----------------------------------------------------------------------


add_executable(${EXE_NAME} ${SOURCES})
target_link_libraries(${EXE_NAME} ${CPM_LIBRARIES})
target_link_libraries(${EXE_NAME} ${SYSTEM_LIBRARIES})

add_custom_target(bin ALL

    # empty flash file
    COMMAND > "${FLASH_FILE}"

    DEPENDS ${EXE_NAME}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXE_NAME} ${EXE_NAME}.bin

    # append flash file
    COMMAND echo "${PROJECT_BINARY_DIR}/${EXE_NAME}.bin ${FLASH_ADDR} ${FLASH_CFG}" >> "${PROJECT_BINARY_DIR}/flash.cfg"
    )

add_dependencies(flash bin)
add_dependencies(debug bin)
