UF2_FAMILY_ID = 0x6db66082
ST_FAMILY = h7
DEPS_SUBMODULES += lib/CMSIS_5 hw/mcu/st/cmsis_device_$(ST_FAMILY) hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

ST_CMSIS = hw/mcu/st/cmsis_device_$(ST_FAMILY)
ST_HAL_DRIVER = hw/mcu/st/stm32$(ST_FAMILY)xx_hal_driver

CFLAGS += -DSTM32H743xx -DHSE_VALUE=8000000

# Default is FulSpeed port
PORT ?= 0

SRC_S += $(ST_CMSIS)/Source/Templates/gcc/startup_stm32h743xx.s
LD_FILE = $(BOARD_PATH)/stm32h743xx_flash.ld

# For flash-jlink target
JLINK_DEVICE = stm32h743zi

# flash target using on-board stlink
flash: flash-stlink

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m7 \
  -mfloat-abi=hard \
  -mfpu=fpv5-d16 \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_STM32H7 \
	-DBOARD_DEVICE_RHPORT_NUM=$(PORT)

ifeq ($(PORT), 1)
  CFLAGS += -DBOARD_DEVICE_RHPORT_SPEED=OPT_MODE_HIGH_SPEED
  $(info "PORT1 High Speed")
else
  $(info "PORT0 Full Speed")
endif

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=maybe-uninitialized -Wno-error=cast-align

# All source paths should be relative to the top level.
SRC_C += \
	src/portable/st/synopsys/dcd_synopsys.c \
	$(ST_CMSIS)/Source/Templates/system_stm32$(ST_FAMILY)xx.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_dma.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_cortex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_rcc_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_gpio.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart_ex.c \
	$(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_pwr_ex.c \

INC += \
	$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(ST_CMSIS)/Include \
	$(TOP)/$(ST_HAL_DRIVER)/Inc

# For freeRTOS port source
FREERTOS_PORT = ARM_CM7/r0p1
