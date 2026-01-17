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
