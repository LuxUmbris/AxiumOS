#!/usr/bin/env sh
set -eu

cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmpdir=$(mktemp -d /tmp/axir-test.XXXXXX)
trap 'rm -rf "$tmpdir" /tmp/axir-dump.txt /tmp/axir-invalid.out' EXIT
make clean
make
./build/axirc --help >/dev/null
test -f build/targets/linux_x86_64.yml
cmp -s targets/linux_x86_64.yml build/targets/linux_x86_64.yml
./build/axirc target linux_x86_64
./build/axirc check tests/valid.axir --target linux_x86_64
if ./build/axirc target ../linux_x86_64 >/tmp/axir-invalid.out 2>&1; then
  echo "unsafe target name unexpectedly passed validation" >&2
  exit 1
fi
./build/axirc check tests/valid.axir
./build/axirc run tests/run.axir
./build/axirc dump tests/run.axir >/tmp/axir-dump.txt
if ./build/axirc check tests/invalid.axir >/tmp/axir-invalid.out 2>&1; then
  echo "invalid AXIR unexpectedly passed validation" >&2
  exit 1
fi
./build/axirc emit tests/emit_exit.axir --target linux_x86_64 -o "$tmpdir/exit37"
test -x "$tmpdir/exit37"
if "$tmpdir/exit37"; then
  echo "direct ELF unexpectedly exited with status 0" >&2
  exit 1
else
  status=$?
  test "$status" -eq 37
fi
./build/axirc emit tests/emit_optimized_exit.axir --target linux_x86_64 -o "$tmpdir/exit19"
if "$tmpdir/exit19"; then
  echo "optimized direct ELF unexpectedly exited with status 0" >&2
  exit 1
else
  status=$?
  test "$status" -eq 19
fi
printf 'AXIR tests passed\n'
