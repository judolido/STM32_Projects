################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/W5500/SNMP/snmp.c \
../Drivers/W5500/SNMP/snmp_custom.c 

OBJS += \
./Drivers/W5500/SNMP/snmp.o \
./Drivers/W5500/SNMP/snmp_custom.o 

C_DEPS += \
./Drivers/W5500/SNMP/snmp.d \
./Drivers/W5500/SNMP/snmp_custom.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/W5500/SNMP/%.o Drivers/W5500/SNMP/%.su Drivers/W5500/SNMP/%.cyclo: ../Drivers/W5500/SNMP/%.c Drivers/W5500/SNMP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L431xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Core" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500/W5500" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-W5500-2f-SNMP

clean-Drivers-2f-W5500-2f-SNMP:
	-$(RM) ./Drivers/W5500/SNMP/snmp.cyclo ./Drivers/W5500/SNMP/snmp.d ./Drivers/W5500/SNMP/snmp.o ./Drivers/W5500/SNMP/snmp.su ./Drivers/W5500/SNMP/snmp_custom.cyclo ./Drivers/W5500/SNMP/snmp_custom.d ./Drivers/W5500/SNMP/snmp_custom.o ./Drivers/W5500/SNMP/snmp_custom.su

.PHONY: clean-Drivers-2f-W5500-2f-SNMP

