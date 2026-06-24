set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_FLAGS
    "-mcpu=cortex-m4 \
    -mfpu=fpv4-sp-d16 \
    -mthumb \
    -std=c99 \
    -mfloat-abi=hard \
    -nostartfiles \
    -nodefaultlibs \
    -nostdlib \
    -nolibc \
    -Wall \
    -Wextra \
    -Wextra \
    -Werror \
    -g \
    -O0 \
    -Wa,-alh=output.lst")

set(CMAKE_EXE_LINKER_FLAGS
    "-lc \
    -T${CMAKE_CURRENT_LIST_DIR}/../linker/stm32g4_${MEMORY}.ld \
    -Wl,-Map=output.map \
    -lgcc \
    -Wl,--gc-sections")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
