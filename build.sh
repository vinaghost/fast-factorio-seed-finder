#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$SCRIPT_DIR"

CONFIG="${1:-Release}"
BUILD_DIR="${2:-$SCRIPT_DIR/build}"
GENERATOR="${3:-Ninja}"

if ! command -v cmake >/dev/null 2>&1; then
  echo "Error: cmake was not found in PATH."
  echo "Install CMake 3.30 or newer and try again."
  exit 1
fi

echo "Configuring project..."
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_CXX_COMPILER=g++-14

echo "Building project..."
cmake --build "$BUILD_DIR" --config "$CONFIG" -j

echo "Build succeeded."