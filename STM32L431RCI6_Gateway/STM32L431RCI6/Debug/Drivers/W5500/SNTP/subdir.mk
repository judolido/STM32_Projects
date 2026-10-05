################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/W5500/SNTP/sntp.c 

OBJS += \
./Drivers/W5500/SNTP/sntp.o 

C_DEPS += \
./Drivers/W5500/SNTP/sntp.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/W5500/SNTP/%.o Drivers/W5500/SNTP/%.su Drivers/W5500/SNTP/%.cyclo: ../Drivers/W5500/SNTP/%.c Drivers/W5500/SNTP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L431xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Core" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500/W5500" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-W5500-2f-SNTP

clean-Drivers-2f-W5500-2f-SNTP:
	-$(RM) ./Drivers/W5500/SNTP/sntp.cyclo ./Drivers/W5500/SNTP/sntp.d ./Drivers/W5500/SNTP/sntp.o ./Drivers/W5500/SNTP/sntp.su

.PHONY: clean-Drivers-2f-W5500-2f-SNTP

