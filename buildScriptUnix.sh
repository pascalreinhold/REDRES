#!/bin/bash
mkdir -p build    # creates a build directory if there is none named build
cd build  # change directory to new build directory
cmake -S ../ -B . #  From the CMakeLists.txt in the parent directory generate a cmake project in the current directory
cmake --build . -j 6 # compiles the code and generates an executable
cd .. # leave build directory
