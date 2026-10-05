################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/W5500/DHCP6/dhcpv6.c 

OBJS += \
./Drivers/W5500/DHCP6/dhcpv6.o 

C_DEPS += \
./Drivers/W5500/DHCP6/dhcpv6.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/W5500/DHCP6/%.o Drivers/W5500/DHCP6/%.su Drivers/W5500/DHCP6/%.cyclo: ../Drivers/W5500/DHCP6/%.c Drivers/W5500/DHCP6/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L431xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Core" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500/W5500" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-W5500-2f-DHCP6

clean-Drivers-2f-W5500-2f-DHCP6:
	-$(RM) ./Drivers/W5500/DHCP6/dhcpv6.cyclo ./Drivers/W5500/DHCP6/dhcpv6.d ./Drivers/W5500/DHCP6/dhcpv6.o ./Drivers/W5500/DHCP6/dhcpv6.su

.PHONY: clean-Drivers-2f-W5500-2f-DHCP6

