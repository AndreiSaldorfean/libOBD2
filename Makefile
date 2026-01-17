all: clean
	cmake -S . -B build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/ArmToolchain.cmake
b:
	cmake --build build
open:
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "init; reset halt"
ren:
	renode --console utils/stm_dev.resc
clean:
	rm -rf build

stm32f401ccu: stm32f401ccu_clean
	cmake -S ./examples/STM32F401CCU/ -B ./examples/STM32F401CCU/build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../../cmake/ArmToolchain.cmake

stm32f401ccu_b:
	cmake --build ./examples/STM32F401CCU/build

stm32f401ccu_clean:
	rm -rf ./examples/STM32F401CCU/build

unit_test: unit_test_clean
	cmake -S ./tests/ -B ./tests/build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../cmake/ArmToolchain.cmake

unit_test_b:
	cmake --build ./tests/build

unit_test_clean:
	rm -rf ./tests/build

