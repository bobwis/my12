################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adcstream.c \
../Core/Src/crc32.c \
../Core/Src/eeprom.c \
../Core/Src/freertos.c \
../Core/Src/httpclient.c \
../Core/Src/httploader.c \
../Core/Src/lcd.c \
../Core/Src/main.c \
../Core/Src/miscutils.c \
../Core/Src/neo7m.c \
../Core/Src/nextionloader.c \
../Core/Src/splat1.c \
../Core/Src/stm32f7xx_hal_msp.c \
../Core/Src/stm32f7xx_hal_timebase_tim.c \
../Core/Src/stm32f7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f7xx.c \
../Core/Src/udpstream.c \
../Core/Src/www.c 

OBJS += \
./Core/Src/adcstream.o \
./Core/Src/crc32.o \
./Core/Src/eeprom.o \
./Core/Src/freertos.o \
./Core/Src/httpclient.o \
./Core/Src/httploader.o \
./Core/Src/lcd.o \
./Core/Src/main.o \
./Core/Src/miscutils.o \
./Core/Src/neo7m.o \
./Core/Src/nextionloader.o \
./Core/Src/splat1.o \
./Core/Src/stm32f7xx_hal_msp.o \
./Core/Src/stm32f7xx_hal_timebase_tim.o \
./Core/Src/stm32f7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f7xx.o \
./Core/Src/udpstream.o \
./Core/Src/www.o 

C_DEPS += \
./Core/Src/adcstream.d \
./Core/Src/crc32.d \
./Core/Src/eeprom.d \
./Core/Src/freertos.d \
./Core/Src/httpclient.d \
./Core/Src/httploader.d \
./Core/Src/lcd.d \
./Core/Src/main.d \
./Core/Src/miscutils.d \
./Core/Src/neo7m.d \
./Core/Src/nextionloader.d \
./Core/Src/splat1.d \
./Core/Src/stm32f7xx_hal_msp.d \
./Core/Src/stm32f7xx_hal_timebase_tim.d \
./Core/Src/stm32f7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f7xx.d \
./Core/Src/udpstream.d \
./Core/Src/www.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g -DUSE_HAL_DRIVER -DSTM32F767xx -DDEBUG -c -I../Core/Inc -I../FATFS/Target -I../FATFS/App -I../LWIP/App -I../LWIP/Target -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I../Middlewares/Third_Party/FatFs/src -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LwIP/src/apps/http -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Drivers/BSP/Components/lan8742 -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adcstream.cyclo ./Core/Src/adcstream.d ./Core/Src/adcstream.o ./Core/Src/adcstream.su ./Core/Src/crc32.cyclo ./Core/Src/crc32.d ./Core/Src/crc32.o ./Core/Src/crc32.su ./Core/Src/eeprom.cyclo ./Core/Src/eeprom.d ./Core/Src/eeprom.o ./Core/Src/eeprom.su ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/httpclient.cyclo ./Core/Src/httpclient.d ./Core/Src/httpclient.o ./Core/Src/httpclient.su ./Core/Src/httploader.cyclo ./Core/Src/httploader.d ./Core/Src/httploader.o ./Core/Src/httploader.su ./Core/Src/lcd.cyclo ./Core/Src/lcd.d ./Core/Src/lcd.o ./Core/Src/lcd.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/miscutils.cyclo ./Core/Src/miscutils.d ./Core/Src/miscutils.o ./Core/Src/miscutils.su ./Core/Src/neo7m.cyclo ./Core/Src/neo7m.d ./Core/Src/neo7m.o ./Core/Src/neo7m.su ./Core/Src/nextionloader.cyclo ./Core/Src/nextionloader.d ./Core/Src/nextionloader.o ./Core/Src/nextionloader.su ./Core/Src/splat1.cyclo ./Core/Src/splat1.d ./Core/Src/splat1.o ./Core/Src/splat1.su ./Core/Src/stm32f7xx_hal_msp.cyclo ./Core/Src/stm32f7xx_hal_msp.d ./Core/Src/stm32f7xx_hal_msp.o ./Core/Src/stm32f7xx_hal_msp.su ./Core/Src/stm32f7xx_hal_timebase_tim.cyclo ./Core/Src/stm32f7xx_hal_timebase_tim.d ./Core/Src/stm32f7xx_hal_timebase_tim.o ./Core/Src/stm32f7xx_hal_timebase_tim.su ./Core/Src/stm32f7xx_it.cyclo ./Core/Src/stm32f7xx_it.d ./Core/Src/stm32f7xx_it.o ./Core/Src/stm32f7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f7xx.cyclo ./Core/Src/system_stm32f7xx.d ./Core/Src/system_stm32f7xx.o ./Core/Src/system_stm32f7xx.su ./Core/Src/udpstream.cyclo ./Core/Src/udpstream.d ./Core/Src/udpstream.o ./Core/Src/udpstream.su ./Core/Src/www.cyclo ./Core/Src/www.d ./Core/Src/www.o ./Core/Src/www.su

.PHONY: clean-Core-2f-Src

