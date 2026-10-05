################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.c \
../Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.c 

OBJS += \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.o \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.o 

C_DEPS += \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.d \
./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/W5500/MQTT/MQTTPacket/src/%.o Drivers/W5500/MQTT/MQTTPacket/src/%.su Drivers/W5500/MQTT/MQTTPacket/src/%.cyclo: ../Drivers/W5500/MQTT/MQTTPacket/src/%.c Drivers/W5500/MQTT/MQTTPacket/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L431xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Core" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500" -I"C:/Users/Vitek/Desktop/projects/STM32_Projects/STM32L431RCI6_Gateway/STM32L431RCI6/Drivers/W5500/W5500" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-W5500-2f-MQTT-2f-MQTTPacket-2f-src

clean-Drivers-2f-W5500-2f-MQTT-2f-MQTTPacket-2f-src:
	-$(RM) ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectClient.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTConnectServer.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTDeserializePublish.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTFormat.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTPacket.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSerializePublish.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeClient.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTSubscribeServer.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.su ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.cyclo ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.d ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.o ./Drivers/W5500/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.su

.PHONY: clean-Drivers-2f-W5500-2f-MQTT-2f-MQTTPacket-2f-src

