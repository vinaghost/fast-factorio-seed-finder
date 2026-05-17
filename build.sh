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

sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100
sudo update-alternatives --auto g++

echo "Configuring project..."
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G "$GENERATOR"

echo "Building project..."
cmake --build "$BUILD_DIR" --config "$CONFIG" -j

echo "Build succeeded."

#./build/seed_finders/solo_any/solo_any_seed_finder -o output.csv --threads 2 --last-seed 1000000