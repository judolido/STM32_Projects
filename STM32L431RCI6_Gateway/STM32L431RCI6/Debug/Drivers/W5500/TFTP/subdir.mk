################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/W5500/TFTP/netutil.c \
../Drivers/W5500/TFTP/tftp.c 

OBJS += \
./Drivers/W5500/TFTP/netutil.o \
./Drivers/W5500/TFTP/tftp.o 

C_DEPS += \
./Drivers/W5500/TFTP/netutil.d \
./Drivers/W5500/TFTP/tftp.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/W5500/TFTP/%.o Drivers/W5500/TFTP/%.su Drivers/W5500/TFTP/%.cyclo: ../Drivers/W5500/TFTP/%.c Drivers/W5500/TFTP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L431xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Core" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500/W5500" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-W5500-2f-TFTP

clean-Drivers-2f-W5500-2f-TFTP:
	-$(RM) ./Drivers/W5500/TFTP/netutil.cyclo ./Drivers/W5500/TFTP/netutil.d ./Drivers/W5500/TFTP/netutil.o ./Drivers/W5500/TFTP/netutil.su ./Drivers/W5500/TFTP/tftp.cyclo ./Drivers/W5500/TFTP/tftp.d ./Drivers/W5500/TFTP/tftp.o ./Drivers/W5500/TFTP/tftp.su

.PHONY: clean-Drivers-2f-W5500-2f-TFTP

