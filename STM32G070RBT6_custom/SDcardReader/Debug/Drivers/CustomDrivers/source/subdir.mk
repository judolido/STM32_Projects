################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/CustomDrivers/source/SDdriver.c 

OBJS += \
./Drivers/CustomDrivers/source/SDdriver.o 

C_DEPS += \
./Drivers/CustomDrivers/source/SDdriver.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/CustomDrivers/source/%.o Drivers/CustomDrivers/source/%.su Drivers/CustomDrivers/source/%.cyclo: ../Drivers/CustomDrivers/source/%.c Drivers/CustomDrivers/source/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G070xx -c -I../Core/Inc -I"C:/Users/jelen/Desktop/STM/STM32Mini/Drivers/CustomDrivers/includes" -I"C:/Users/jelen/Desktop/projects/STM32_Projects/STM32G070RBT6_custom/SDcardReader/Drivers/CustomDrivers" -I../Drivers/CustomDrivers -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Drivers-2f-CustomDrivers-2f-source

clean-Drivers-2f-CustomDrivers-2f-source:
	-$(RM) ./Drivers/CustomDrivers/source/SDdriver.cyclo ./Drivers/CustomDrivers/source/SDdriver.d ./Drivers/CustomDrivers/source/SDdriver.o ./Drivers/CustomDrivers/source/SDdriver.su

.PHONY: clean-Drivers-2f-CustomDrivers-2f-source

