################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/tftp/tftdriver.c \
../Core/Src/tftp/tftploader.c 

OBJS += \
./Core/Src/tftp/tftdriver.o \
./Core/Src/tftp/tftploader.o 

C_DEPS += \
./Core/Src/tftp/tftdriver.d \
./Core/Src/tftp/tftploader.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/tftp/%.o Core/Src/tftp/%.su Core/Src/tftp/%.cyclo: ../Core/Src/tftp/%.c Core/Src/tftp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g -DUSE_HAL_DRIVER -DSTM32F767xx -DDEBUG -c -I../Core/Inc -I../FATFS/Target -I../FATFS/App -I../LWIP/App -I../LWIP/Target -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I../Middlewares/Third_Party/FatFs/src -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/LwIP/src/apps/http -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Drivers/BSP/Components/lan8742 -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-tftp

clean-Core-2f-Src-2f-tftp:
	-$(RM) ./Core/Src/tftp/tftdriver.cyclo ./Core/Src/tftp/tftdriver.d ./Core/Src/tftp/tftdriver.o ./Core/Src/tftp/tftdriver.su ./Core/Src/tftp/tftploader.cyclo ./Core/Src/tftp/tftploader.d ./Core/Src/tftp/tftploader.o ./Core/Src/tftp/tftploader.su

.PHONY: clean-Core-2f-Src-2f-tftp

