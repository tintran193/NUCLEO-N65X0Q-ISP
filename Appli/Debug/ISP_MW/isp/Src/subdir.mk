################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ISP_MW/isp/Src/isp_algo.c \
../ISP_MW/isp/Src/isp_cmd_parser.c \
../ISP_MW/isp/Src/isp_conf_template.c \
../ISP_MW/isp/Src/isp_core.c \
../ISP_MW/isp/Src/isp_services.c \
../ISP_MW/isp/Src/isp_tool_com.c 

OBJS += \
./ISP_MW/isp/Src/isp_algo.o \
./ISP_MW/isp/Src/isp_cmd_parser.o \
./ISP_MW/isp/Src/isp_conf_template.o \
./ISP_MW/isp/Src/isp_core.o \
./ISP_MW/isp/Src/isp_services.o \
./ISP_MW/isp/Src/isp_tool_com.o 

C_DEPS += \
./ISP_MW/isp/Src/isp_algo.d \
./ISP_MW/isp/Src/isp_cmd_parser.d \
./ISP_MW/isp/Src/isp_conf_template.d \
./ISP_MW/isp/Src/isp_core.d \
./ISP_MW/isp/Src/isp_services.d \
./ISP_MW/isp/Src/isp_tool_com.d 


# Each subdirectory must supply rules for building sources it contributes
ISP_MW/isp/Src/%.o ISP_MW/isp/Src/%.su ISP_MW/isp/Src/%.cyclo: ../ISP_MW/isp/Src/%.c ISP_MW/isp/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32N657xx -DUSE_NUCLEO_64 -c -I../ISP_MW/isp/USB_Device/Inc -I../stm32-mw-usb-device/Core/Inc -I../stm32-mw-usb-device/Class/CDC/Inc -I../ISP_MW/evision/Inc -I../ISP_MW/isp/Inc -I../Core/Inc -I../../Secure_nsclib -I../../Drivers/STM32N6xx_HAL_Driver/Inc -I../../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Include -I../../Drivers/BSP/STM32N6xx_Nucleo -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-ISP_MW-2f-isp-2f-Src

clean-ISP_MW-2f-isp-2f-Src:
	-$(RM) ./ISP_MW/isp/Src/isp_algo.cyclo ./ISP_MW/isp/Src/isp_algo.d ./ISP_MW/isp/Src/isp_algo.o ./ISP_MW/isp/Src/isp_algo.su ./ISP_MW/isp/Src/isp_cmd_parser.cyclo ./ISP_MW/isp/Src/isp_cmd_parser.d ./ISP_MW/isp/Src/isp_cmd_parser.o ./ISP_MW/isp/Src/isp_cmd_parser.su ./ISP_MW/isp/Src/isp_conf_template.cyclo ./ISP_MW/isp/Src/isp_conf_template.d ./ISP_MW/isp/Src/isp_conf_template.o ./ISP_MW/isp/Src/isp_conf_template.su ./ISP_MW/isp/Src/isp_core.cyclo ./ISP_MW/isp/Src/isp_core.d ./ISP_MW/isp/Src/isp_core.o ./ISP_MW/isp/Src/isp_core.su ./ISP_MW/isp/Src/isp_services.cyclo ./ISP_MW/isp/Src/isp_services.d ./ISP_MW/isp/Src/isp_services.o ./ISP_MW/isp/Src/isp_services.su ./ISP_MW/isp/Src/isp_tool_com.cyclo ./ISP_MW/isp/Src/isp_tool_com.d ./ISP_MW/isp/Src/isp_tool_com.o ./ISP_MW/isp/Src/isp_tool_com.su

.PHONY: clean-ISP_MW-2f-isp-2f-Src

