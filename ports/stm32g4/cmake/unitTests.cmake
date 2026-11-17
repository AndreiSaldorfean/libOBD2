set(TEST_EXECUTABLE_SUFFIX ".elf")
set(TEST_USE_LEGACY_SOURCES ON)
set(TEST_USE_ECU ON)
set(TEST_ADD_BIN_POST_BUILD ON)

set(TINYUSB_ROOT ${CMAKE_SOURCE_DIR}/../examples/libs/tinyusb)

file(GLOB TEST_PLATFORM_STARTUP_SOURCES
	"${CMAKE_SOURCE_DIR}/../ports/stm32g4/startup/*.c"
)

list(APPEND TEST_PLATFORM_SOURCES
	"${CMAKE_SOURCE_DIR}/../examples/libs/cmsis_device_g4/Source/Templates/system_stm32g4xx.c"
	${TEST_PLATFORM_STARTUP_SOURCES}
	${TINYUSB_ROOT}/src/tusb.c
	${TINYUSB_ROOT}/src/common/tusb_fifo.c
	${TINYUSB_ROOT}/src/device/usbd.c
	${TINYUSB_ROOT}/src/device/usbd_control.c
	${TINYUSB_ROOT}/src/class/cdc/cdc_device.c
	${TINYUSB_ROOT}/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
)

list(APPEND TEST_PLATFORM_INCLUDES
	../examples/libs/libopencm3/include/
	../examples/libs/FreeRTOS-Kernel/include/
	../examples/libs/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/
	../examples/libs/cmsis_device_g4/Include
	../examples/libs/CMSIS_5/CMSIS/Core/Include
	${TINYUSB_ROOT}/src
	${TINYUSB_ROOT}/src/common
	${TINYUSB_ROOT}/src/device
	${TINYUSB_ROOT}/src/class/cdc
	${TINYUSB_ROOT}/src/portable/st/stm32_fsdev
	${TINYUSB_ROOT}/hw
	../ports/stm32g4/startup/
	../ports/stm32g4/config/
)

list(APPEND TEST_PLATFORM_DEFINES
	STM32G4
	STM32G474xx
	UNITY_EXCLUDE_SETJMP_H
)

get_filename_component(OPENCM3_LIB
	"${CMAKE_CURRENT_LIST_DIR}/../../../examples/libs/libopencm3/lib/libopencm3_stm32g4.a"
	ABSOLUTE
)

list(APPEND TEST_PLATFORM_LINK_LIBS ${OPENCM3_LIB})
