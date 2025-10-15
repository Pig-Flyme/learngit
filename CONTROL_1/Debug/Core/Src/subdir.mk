################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc.c \
../Core/Src/dma.c \
../Core/Src/endgas.c \
../Core/Src/gpio.c \
../Core/Src/main.c \
../Core/Src/ntc_control.c \
../Core/Src/oxygen.c \
../Core/Src/ph.c \
../Core/Src/pid.c \
../Core/Src/pump.c \
../Core/Src/relay.c \
../Core/Src/stir.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32h7xx.c \
../Core/Src/temperature.c \
../Core/Src/tim.c \
../Core/Src/usart.c 

OBJS += \
./Core/Src/adc.o \
./Core/Src/dma.o \
./Core/Src/endgas.o \
./Core/Src/gpio.o \
./Core/Src/main.o \
./Core/Src/ntc_control.o \
./Core/Src/oxygen.o \
./Core/Src/ph.o \
./Core/Src/pid.o \
./Core/Src/pump.o \
./Core/Src/relay.o \
./Core/Src/stir.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32h7xx.o \
./Core/Src/temperature.o \
./Core/Src/tim.o \
./Core/Src/usart.o 

C_DEPS += \
./Core/Src/adc.d \
./Core/Src/dma.d \
./Core/Src/endgas.d \
./Core/Src/gpio.d \
./Core/Src/main.d \
./Core/Src/ntc_control.d \
./Core/Src/oxygen.d \
./Core/Src/ph.d \
./Core/Src/pid.d \
./Core/Src/pump.d \
./Core/Src/relay.d \
./Core/Src/stir.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32h7xx.d \
./Core/Src/temperature.d \
./Core/Src/tim.d \
./Core/Src/usart.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H750xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc.cyclo ./Core/Src/adc.d ./Core/Src/adc.o ./Core/Src/adc.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/endgas.cyclo ./Core/Src/endgas.d ./Core/Src/endgas.o ./Core/Src/endgas.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/ntc_control.cyclo ./Core/Src/ntc_control.d ./Core/Src/ntc_control.o ./Core/Src/ntc_control.su ./Core/Src/oxygen.cyclo ./Core/Src/oxygen.d ./Core/Src/oxygen.o ./Core/Src/oxygen.su ./Core/Src/ph.cyclo ./Core/Src/ph.d ./Core/Src/ph.o ./Core/Src/ph.su ./Core/Src/pid.cyclo ./Core/Src/pid.d ./Core/Src/pid.o ./Core/Src/pid.su ./Core/Src/pump.cyclo ./Core/Src/pump.d ./Core/Src/pump.o ./Core/Src/pump.su ./Core/Src/relay.cyclo ./Core/Src/relay.d ./Core/Src/relay.o ./Core/Src/relay.su ./Core/Src/stir.cyclo ./Core/Src/stir.d ./Core/Src/stir.o ./Core/Src/stir.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32h7xx.cyclo ./Core/Src/system_stm32h7xx.d ./Core/Src/system_stm32h7xx.o ./Core/Src/system_stm32h7xx.su ./Core/Src/temperature.cyclo ./Core/Src/temperature.d ./Core/Src/temperature.o ./Core/Src/temperature.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su

.PHONY: clean-Core-2f-Src

