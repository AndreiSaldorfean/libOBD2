#!/bin/zsh

path=" -I/home/rudy/tools/arm-gnu-toolchain/arm-none-eabi/include/\","

sed --help
# sed "4s/..$/${path}/" ../build/compile_commands.json > tmp && mv tmp ../build/compile_commands.json
