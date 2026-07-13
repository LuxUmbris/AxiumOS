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
./build/axirc emit tests/emit_write.axir --target linux_x86_64 -o "$tmpdir/write"
set +e
output=$("$tmpdir/write")
status=$?
set -e
test "$status" -eq 23
test "$output" = 'direct byte syscall'
./build/axirc emit tests/emit_link_data.axir tests/emit_link_code.axir --target linux_x86_64 -o "$tmpdir/linked"
set +e
output=$("$tmpdir/linked")
status=$?
set -e
test "$status" -eq 29
test "$output" = 'linked AXIR'
./build/axirc emit tests/emit_loop.axir --target linux_x86_64 -o "$tmpdir/loop"
set +e
"$tmpdir/loop"
status=$?
set -e
test "$status" -eq 41
./build/axirc emit tests/emit_jump.axir --target linux_x86_64 -o "$tmpdir/jump"
set +e
"$tmpdir/jump"
status=$?
set -e
test "$status" -eq 43
./build/axirc emit tests/emit_syscall_result.axir --target linux_x86_64 -o "$tmpdir/syscall-result"
set +e
output=$("$tmpdir/syscall-result")
status=$?
set -e
test "$status" -eq 12
test "$output" = 'result-slot'
./build/axirc emit tests/emit_virtual_slots.axir --target linux_x86_64 -o "$tmpdir/virtual-slots"
set +e
output=$("$tmpdir/virtual-slots")
status=$?
set -e
test "$status" -eq 14
test "$output" = 'virtual slots'
./build/axirc emit tests/emit_integer_binary.axir --target linux_x86_64 -o "$tmpdir/integer-binary"
set +e
"$tmpdir/integer-binary"
status=$?
set -e
test "$status" -eq 42
./build/axirc emit tests/emit_comparisons.axir --target linux_x86_64 -o "$tmpdir/comparisons"
set +e
"$tmpdir/comparisons"
status=$?
set -e
test "$status" -eq 3
./build/axirc emit tests/emit_shifts.axir --target linux_x86_64 -o "$tmpdir/shifts"
set +e
"$tmpdir/shifts"
status=$?
set -e
test "$status" -eq 3
./build/axirc emit tests/emit_division.axir --target linux_x86_64 -o "$tmpdir/division"
set +e
"$tmpdir/division"
status=$?
set -e
test "$status" -eq 12
printf 'AXIR tests passed\n'
