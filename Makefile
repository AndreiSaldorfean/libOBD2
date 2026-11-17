# ==============================================================================
# This makefile is used for generating the build directory and compiling targets
# ==============================================================================

build_type=Debug
# build_type=Release
memory=flash
# memory=ram

# app=example
app=tests
# app=library

ifneq ($(app),library)
dir=$(app)_$(memory)_$(build_type)
else
dir=library
endif

# Coverage build of the tests (host_virtual only), kept apart from the normal build
cc_dir=$(dir)_cc
# Single-header build (dist/libobd2.h), independent of build_config.json
amal_dir=builds/amalgamation

.PHONY: all b r cc clean amalgamate amalgamate_check

all: clean
	python scripts/gen.py --app $(app)
b:
	cmake --build builds/$(dir) -j12
r:
	./builds/$(dir)/test
# Code coverage: configure + build with gcov, run the tests, print line/branch coverage
cc:
	python scripts/gen.py --app $(app) --coverage
	cmake --build builds/$(cc_dir) -j12
	python scripts/coverage.py --reset builds/$(cc_dir)
	./builds/$(cc_dir)/test
	python scripts/coverage.py builds/$(cc_dir)
# Single header: generate $(amal_dir)/dist/libobd2.h
amalgamate:
	cmake -S . -B $(amal_dir) -DSPT_FREERTOS=OFF
	cmake --build $(amal_dir) --target amalgamate
# Single header: generate it, then compile + link the smoke test with gcc and arm-none-eabi-gcc
amalgamate_check: amalgamate
	./tests/amalgamation/check.sh $(amal_dir)/dist
clean:
	rm -rf builds/$(dir) builds/$(cc_dir) $(amal_dir)

