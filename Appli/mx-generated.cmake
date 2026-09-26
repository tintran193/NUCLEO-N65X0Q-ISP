# Source/include/define lists reconstructed from the original STM32CubeIDE
# Eclipse/Makefile project (Appli/Debug/**/subdir.mk), mirroring the format
# STM32CubeMX generates (see ../../STM32N6_Face_Detection/Appli/mx-generated.cmake).
#
# NOTE on the stm32-mw-usb-device middleware and ISP_SRC/stm32-mw-isp:
# every subdir.mk in this checkout lists them (with an -I.../USB_Device/Inc
# include and ~60 usbd_*.c sources), but neither directory actually exists
# under this project -- they were CubeIDE "linked resource" folders that
# pointed at the original author's machine (D:/STM32N6_WS/..., D:/stm32-mw-isp/...)
# and were never vendored into this repo. They are left out below; the app
# never calls MX_USB_DEVICE_Init() or anything from usbd_core.c, so nothing
# is missing functionally.
#
# ISP_MW/isp/Src/isp_tool_com.c *is* present locally, but it #includes
# "usbd_cdc_if.h"/"usb_device.h" which -- for the same reason -- don't
# exist in this checkout. Its calls are all guarded by
# `#ifdef ISP_MW_TUNING_TOOL_SUPPORT` in isp_core.c, which is never defined
# here, so the USB tuning-tool channel is dead code in this build; the file
# is excluded from compilation instead of stubbing out two missing headers.

set(MX_Defines_Syms
	USE_HAL_DRIVER
	STM32N657xx
	USE_NUCLEO_64
    $<$<CONFIG:Debug>:DEBUG>
)

set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/isp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/evision/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Secure_nsclib
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6xx_Nucleo
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/secure_nsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/startup_stm32n657x0hxq.s
)

# Camera driver + ISP middleware sources
set(App_ISP_Camera_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/imx219.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/imx219_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/imx219_reg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/isp/Src/isp_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/isp/Src/isp_cmd_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/isp/Src/isp_conf_template.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/isp/Src/isp_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/isp/Src/isp_services.c
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32n6xx_s.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dcmipp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rif.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6xx_Nucleo/stm32n6xx_nucleo.c
)

# Link directories setup
set(MX_LINK_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/ISP_MW/evision/Lib
)
# Project libraries
# (only the GCC-built eVision archives are linked; the _iar/_keil ones the
# original .cproject also listed are for other toolchains and irrelevant here)
set (MX_LINK_LIBS
    STM32_Drivers
    n6-evision-awb_gcc
    n6-evision-st-ae_gcc
    ${TOOLCHAIN_LINK_LIBRARIES}
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)

# Add STM32CubeMX generated application + camera/ISP sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src} ${App_ISP_Camera_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)
