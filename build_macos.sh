#!/usr/bin/env bash
set -euo pipefail

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake not found. Please install CMake first."
    exit 1
fi

if [[ $# -ne 1 ]]; then
    echo "Usage: ./build_macos.sh config"
    echo ""
    echo "config:"
    echo "  debug   -   build with the debug configuration"
    echo "  release -   build with the release configuration"
    echo ""
    exit 1
fi


if [[ "$1" == "debug" ]]; then
    CONFIG="Debug"
elif [[ "$1" == "release" ]]; then
    CONFIG="Release"
else
    echo "The config \"$1\" is not supported!"
    echo ""
    echo "Configs:"
    echo "  debug   -   build with the debug configuration"
    echo "  release -   build with the release configuration"
    echo ""
    exit 1
fi

BUILD_DIR="build/macos-$1"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DHYBRID_BUILD_EDITOR=ON \
    -DHYBRID_BUILD_PLAYER=ON \
    -DBUILD_TESTING=ON

cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel
ctest --test-dir "${BUILD_DIR}" --build-config "${CONFIG}" --output-on-failure
