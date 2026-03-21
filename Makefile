all: clean
	cmake -S . -B builds/library -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/STM32F4.cmake -DMEMORY="flash"
b:
	cmake --build builds/library -j12
clean:
	rm -rf builds/library

################################ RAM BUILD TARGETS ################################
stm_r: stm_r
	cmake -S . -B builds/library -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/STM32F4.cmake -DMEMORY="ram"
	cmake --build builds/library -j12
	python3 utils/gen.py --app=demo --memory=ram

stm_rb:
	cmake --build ./builds/demo_ram -j12

stm_r_clean:
	rm -rf ./builds/demo_ram

# execute app for ram
stm_rx:
	@if $(MAKE) -q; then \
		./builds/demo_ram/demo.sh -r; \
	else \
		./builds/demo_ram/demo.sh -lr; \
	fi

# debug app for ram
stm_rd:
	@if $(MAKE) -q; then \
		./builds/demo_ram/tests.sh -d; \
	else \
		./builds/demo_ram/tests.sh -ld; \
	fi

# create build directory for tests for ram
test_r: test_r_clean
	cmake -S . -B builds/library -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/STM32F4.cmake -DMEMORY="ram"
	cmake --build builds/library -j12
	python3 utils/gen.py --app=tests --memory=ram

# build tests for ram
test_rb:
	cmake --build ./builds/tests_ram -j12

# execute tests for ram
test_rx:
	./builds/tests_ram/tests.sh -r
test_rlx:
	./builds/tests_ram/tests.sh -lr

# debug tests for ram
test_rd:
	@if $(MAKE) -q; then \
		./builds/tests_ram/tests.sh -d; \
	else \
		./builds/tests_ram/tests.sh -ld; \
	fi

test_r_clean:
	rm -rf ./builds/tests_ram
################################ FLASH BUILD TARGETS ################################

################################ FLASH BUILD TARGETS ################################
stm_f: stm_f_clean
	cmake -S . -B builds/library -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/STM32F4.cmake -DMEMORY="flash"
	cmake --build builds/library -j12
	python3 utils/gen.py --app=demo --memory=flash

stm_fb:
	cmake --build ./builds/demo_flash -j12

stm_f_clean:
	rm -rf ./builds/demo_flash

# execute app for flash
stm_fx:
	@if $(MAKE) -q; then \
		./builds/demo_flash/demo.sh -r; \
	else \
		./builds/demo_flash/demo.sh -lr; \
	fi

# debug app for flash
stm_fd:
	@if $(MAKE) -q; then \
		./builds/demo_flash/demo.sh -d; \
	else \
		./builds/demo_flash/demo.sh -ld; \
	fi

test_f: test_f_clean
	cmake -S . -B builds/library -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake/STM32F4.cmake -DMEMORY="flash"
	cmake --build builds/library -j12
	python3 utils/gen.py --app=tests --memory=flash

test_fb:
	cmake --build ./builds/tests_flash -j12

test_f_clean:
	rm -rf ./builds/tests_flash

# execute test for flash
test_fx:
	./builds/tests_flash/tests.sh -r

test_flx:
	./builds/tests_flash/tests.sh -lr

# debug test for flash
test_fd:
	@if $(MAKE) -q; then \
		./builds/tests_flash/tests.sh -d; \
	else \
		./builds/tests_flash/tests.sh -ld; \
	fi
################################ FLASH BUILD TARGETS ################################

