#!/usr/bin/env sh
set -eu

cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
trap 'rm -f /tmp/axir-dump.txt /tmp/axir-invalid.out' EXIT
make clean
make
./build/axirc --help >/dev/null
./build/axirc check tests/valid.axir
./build/axirc run tests/run.axir
./build/axirc dump tests/run.axir >/tmp/axir-dump.txt
if ./build/axirc check tests/invalid.axir >/tmp/axir-invalid.out 2>&1; then
  echo "invalid AXIR unexpectedly passed validation" >&2
  exit 1
fi
printf 'AXIR tests passed\n'
