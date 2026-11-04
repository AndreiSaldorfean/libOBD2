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

# Coverage build of the tests (host only), kept apart from the normal build
cc_dir=$(dir)_cc

.PHONY: all b r cc clean

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
clean:
	rm -rf builds/$(dir) builds/$(cc_dir)

