# ==============================================================================
# This makefile is used for generating the build directory and compiling targets
# ==============================================================================

# build_type=Debug
build_type=Release
memory=flash
# memory=ram

app=example
# app=tests
# app=library

ifneq ($(app),library)
dir=$(app)_$(memory)_$(build_type)
else
dir=library
endif

all: clean
	python utils/gen.py --app $(app)
b:
	cmake --build builds/$(dir) -j12
clean:
	rm -rf builds/$(dir)

