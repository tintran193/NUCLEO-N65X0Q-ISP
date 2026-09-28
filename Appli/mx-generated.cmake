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
	TX_INCLUDE_USER_DEFINE_FILE
	TX_SINGLE_MODE_SECURE=1
	UX_INCLUDE_USER_DEFINE_FILE
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/ports/generic/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/inc
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

# ThreadX/USBX + UVC applicative sources (added to bring up a USB Video
# Class MJPEG stream on top of the existing camera capture -- see
# WORKLOG.md/Camera_README.md for the full explanation). app_threadx.c is a
# NEW file written for this project (not a copy of Camera_N6_AI_Test's,
# which re-does camera bring-up with a different, deliberately-unused color
# path); the rest are ported from Camera_N6_AI_Test essentially as-is.
set(App_UVC_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/tx_initialize_low_level.S
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_azure_rtos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_threadx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_usbx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_usbx_device.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ux_device_descriptors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ux_device_cdc_acm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ux_device_video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_jpg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_cvt.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pcd_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_ll_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_jpeg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim_ex.c
)

set(usbx_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_callback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_destroy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_stall.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_endpoint_status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_frame_number_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_function.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_initialize_complete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_interrupt_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_transfer_request.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_uninitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_transfer_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_alternate_setting_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_alternate_setting_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_class_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_class_unregister.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_clear_feature.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_configuration_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_configuration_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_control_request_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_descriptor_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_disconnect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_endpoint_stall.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_get_status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_host_wakeup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_interface_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_interface_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_interface_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_interface_start.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_microsoft_extension_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_set_feature.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_transfer_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_transfer_all_request_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_transfer_request.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_device_stack_uninitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_debug_callback_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_debug_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_delay_ms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_descriptor_pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_descriptor_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_error_callback_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_long_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_long_get_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_long_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_long_put_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate_add_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate_mulc_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate_mulv_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_compare.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_copy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_byte_pool_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_memory_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_off.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_on.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_pci_class_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_pci_read.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_pci_write.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_physical_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_set_interrupt_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_short_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_short_get_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_short_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_short_put_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_string_length_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_string_length_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_string_to_unicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_identify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_schedule_other.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_unicode_to_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_utility_virtual_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_system_error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_system_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/core/src/ux_system_uninitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_control_request.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_ioctl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_max_payload_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_read_payload_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_read_payload_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_read_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_reception_start.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_stream_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_transmission_start.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_uninitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_write_payload_commit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_write_payload_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_video_write_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_bulkin_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_bulkout_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_control_request.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_ioctl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_read.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_unitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_write.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/usbx/common/usbx_device_classes/src/ux_device_class_cdc_acm_write_with_callback.c
)
set(threadx_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_secure_stack_initialize.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_secure_stack_allocate.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_secure_stack_free.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_context_restore.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_context_save.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_interrupt_control.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_interrupt_disable.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_interrupt_restore.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_schedule.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_stack_build.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_system_return.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_timer_interrupt.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_initialize_high_level.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_initialize_kernel_enter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_initialize_kernel_setup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/tx_thread_secure_stack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/ports/cortex_M55/gnu/src/txe_thread_secure_stack_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_stack_error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_stack_error_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_system_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_pool_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_pool_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_block_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_pool_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_byte_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_event_flags_set_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_mutex_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_flush.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_front_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_queue_send_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_ceiling_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_semaphore_put_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_entry_exit_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_identify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_preemption_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_shell_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_stack_analyze.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_system_preempt_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_system_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_terminate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_time_slice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_time_slice_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_timeout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_thread_wait_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_time_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_time_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_block_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_block_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_block_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_block_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_block_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_block_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_byte_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_byte_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_byte_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_byte_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_byte_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_event_flags_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_event_flags_set_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_mutex_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_mutex_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_mutex_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_mutex_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_flush.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_front_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_queue_send_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_ceiling_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_semaphore_put_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_entry_exit_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_preemption_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_terminate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_time_slice_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_thread_wait_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_expiration_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_system_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_system_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/tx_timer_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_timer_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_timer_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_timer_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/threadx/common/src/txe_timer_info_get.c
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
    usbx
    threadx
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

# Create usbx static library
add_library(usbx OBJECT)
target_sources(usbx PRIVATE ${usbx_Src})
target_link_libraries(usbx PUBLIC stm32cubemx)

# Create threadx static library
add_library(threadx OBJECT)
target_sources(threadx PRIVATE ${threadx_Src})
target_link_libraries(threadx PUBLIC stm32cubemx)

# Add STM32CubeMX generated application + camera/ISP + UVC/ThreadX/USBX sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src} ${App_ISP_Camera_Src} ${App_UVC_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)
