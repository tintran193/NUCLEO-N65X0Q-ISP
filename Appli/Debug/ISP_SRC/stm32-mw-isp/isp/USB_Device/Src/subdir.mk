################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
D:/stm32-mw-isp/stm32-mw-isp/isp/USB_Device/Src/usb_device.c \
D:/stm32-mw-isp/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.c \
D:/stm32-mw-isp/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.c 

OBJS += \
./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.o \
./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.o \
./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.o 

C_DEPS += \
./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.d \
./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.d \
./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.d 


# Each subdirectory must supply rules for building sources it contributes
ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.o: D:/stm32-mw-isp/stm32-mw-isp/isp/USB_Device/Src/usb_device.c ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Middlewares/ST/stm32-mw-isp/isp/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Drivers/BSP/STM32N6xx_Nucleo -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.o: D:/stm32-mw-isp/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.c ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Middlewares/ST/stm32-mw-isp/isp/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Drivers/BSP/STM32N6xx_Nucleo -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.o: D:/stm32-mw-isp/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.c ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Middlewares/ST/stm32-mw-isp/isp/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Drivers/BSP/STM32N6xx_Nucleo -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ISP_SRC-2f-stm32-2d-mw-2d-isp-2f-isp-2f-USB_Device-2f-Src

clean-ISP_SRC-2f-stm32-2d-mw-2d-isp-2f-isp-2f-USB_Device-2f-Src:
	-$(RM) ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.cyclo ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.d ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.o ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usb_device.su ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.cyclo ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.d ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.o ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_cdc_if.su ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.cyclo ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.d ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.o ./ISP_SRC/stm32-mw-isp/isp/USB_Device/Src/usbd_desc.su

.PHONY: clean-ISP_SRC-2f-stm32-2d-mw-2d-isp-2f-isp-2f-USB_Device-2f-Src

