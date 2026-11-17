#!/usr/bin/env bash
# Builds Morpheus (Release) into build/Release.  Needs cmake, libsdl2-dev, a C++17 compiler.
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
echo "Built: build/Morpheus"
