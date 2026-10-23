cmake_minimum_required(VERSION 3.22)

#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    # The unoptimized Debug image exceeds the AT32F425's 64 KiB Flash.
    # Use the size-optimized Release configuration for a default build.
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME DAPLink_AT32F425)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Select the SWD transport with -DDAP_SWD_SPI=0 (GPIO) or =1 (SPI-assisted).
set(DAP_SWD_SPI 0 CACHE STRING "Select SWD transport: 0=GPIO, 1=SPI")
set_property(CACHE DAP_SWD_SPI PROPERTY STRINGS 0 1)
if(NOT "${DAP_SWD_SPI}" STREQUAL "0" AND NOT "${DAP_SWD_SPI}" STREQUAL "1")
    message(FATAL_ERROR "DAP_SWD_SPI must be 0 (GPIO) or 1 (SPI)")
endif()

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add at32_workbench generated sources
add_subdirectory(cmake/at32_workbench)

set(CHERRYDAP_ROOT ${CMAKE_SOURCE_DIR}/third_part/CherryDAP)
set(CHERRYUSB_ROOT ${CHERRYDAP_ROOT}/CherryUSB)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    project/src/cdc_usb2uart.c
    project/src/swo_usart_driver.c
    project/port/dap_main.c
    project/port/dap_swd_spi.c

    ${CHERRYUSB_ROOT}/core/usbd_core.c
    ${CHERRYUSB_ROOT}/class/cdc/usbd_cdc_acm.c
    ${CHERRYUSB_ROOT}/port/dwc2/usb_dc_dwc2.c
    project/port/usb_glue_at.c

    ${CHERRYDAP_ROOT}/CherryRB/chry_ringbuffer.c
    ${CHERRYDAP_ROOT}/DAP/Source/DAP.c
    ${CHERRYDAP_ROOT}/DAP/Source/DAP_vendor.c
    project/port/SW_DP.c
    ${CHERRYDAP_ROOT}/DAP/Source/SWO.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/libraries/cmsis/Driver/Include
    ${CMAKE_SOURCE_DIR}/project/port
    ${CHERRYUSB_ROOT}/common
    ${CHERRYUSB_ROOT}/core
    ${CHERRYUSB_ROOT}/class/cdc
    ${CHERRYUSB_ROOT}/class/hid
    ${CHERRYUSB_ROOT}/class/msc
    ${CHERRYUSB_ROOT}/port/dwc2
    ${CHERRYDAP_ROOT}/CherryRB
    ${CHERRYDAP_ROOT}/DAP/Include
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    CONFIG_CHERRYDAP_USE_CUSTOM_HID=0
    CONFIG_CHERRYDAP_USE_MSC=0
    DAP_SWD_SPI=${DAP_SWD_SPI}
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    at32_workbench

    # Add user defined libraries
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
)
