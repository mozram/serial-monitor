#!/bin/bash
set -e
mkdir -p build_debug
cd build_debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
echo "Debug Build Successful!"
