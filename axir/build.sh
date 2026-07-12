#!/usr/bin/env sh
set -eu

cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
mkdir -p build/targets
"${CXX:-c++}" src/*.cpp -Iinclude -o build/axirc -std=c++17 -Wall -Wextra -Wpedantic -O2
cp targets/*.yml build/targets/
