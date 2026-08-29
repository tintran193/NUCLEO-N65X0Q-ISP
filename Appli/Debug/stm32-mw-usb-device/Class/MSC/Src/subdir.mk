################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../stm32-mw-usb-device/Class/MSC/Src/usbd_msc.c \
../stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.c \
../stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.c \
../stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.c \
../stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.c 

OBJS += \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc.o \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.o \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.o \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.o \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.o 

C_DEPS += \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc.d \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.d \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.d \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.d \
./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.d 


# Each subdirectory must supply rules for building sources it contributes
stm32-mw-usb-device/Class/MSC/Src/%.o stm32-mw-usb-device/Class/MSC/Src/%.su stm32-mw-usb-device/Class/MSC/Src/%.cyclo: ../stm32-mw-usb-device/Class/MSC/Src/%.c stm32-mw-usb-device/Class/MSC/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DUSE_NUCLEO_64 -c -I../ISP_MW/isp/USB_Device/Inc -I../stm32-mw-usb-device/Core/Inc -I../stm32-mw-usb-device/Class/CDC/Inc -I../ISP_MW/evision/Inc -I../ISP_MW/isp/Inc -I../Core/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Drivers/BSP/STM32N6xx_Nucleo -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-stm32-2d-mw-2d-usb-2d-device-2f-Class-2f-MSC-2f-Src

clean-stm32-2d-mw-2d-usb-2d-device-2f-Class-2f-MSC-2f-Src:
	-$(RM) ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc.cyclo ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc.d ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc.o ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc.su ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.cyclo ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.d ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.o ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_bot.su ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.cyclo ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.d ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.o ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_data.su ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.cyclo ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.d ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.o ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_scsi.su ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.cyclo ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.d ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.o ./stm32-mw-usb-device/Class/MSC/Src/usbd_msc_storage_template.su

.PHONY: clean-stm32-2d-mw-2d-usb-2d-device-2f-Class-2f-MSC-2f-Src

