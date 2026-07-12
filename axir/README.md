# AXIR v1.0 reference front end

`axirc` is a first executable implementation of the AXIR v1.0 textual format. It provides a tokenizer, parser, strict semantic validator, static-data layout, canonical text dump, and a deterministic in-process reference interpreter for the backend-independent instruction subset.

## Build

```sh
make
# or
./build.sh
```

Both entry points build `build/axirc` with a C++17 compiler.

## Design rules

AXIR uses plain data structs and free functions rather than classes or inheritance. Structs describe tokens, operands, instructions, data objects, functions, programs, and runtime configuration; parsing, validation, dumping, and execution are separate functions.

The implementation follows clean-code boundaries: each public function has one responsibility, names describe domain intent, dependencies flow through explicit parameters, and the command-line driver remains separate from tokenization, parsing, validation, and execution.

## CLI

```text
axirc check program.axir
axirc run program.axir [--memory bytes] [--max-steps count]
axirc dump program.axir
```

Passing only `program.axir` is equivalent to `check`.

`check` accepts the complete v1.0 vocabulary, including load/store variants, integer and floating arithmetic, comparisons, conversions, labels, functions, static and indirect calls, returns, process control, symbolic syscalls, static data, and `addr` references. It rejects undefined labels/data, malformed slots, invalid operand classes, duplicate labels/data, and malformed literals.

`run` implements integer/FP arithmetic, memory, `addr`, labels, branches, `ret`, and `hlt`. Memory is byte-addressable and little-endian in this reference runtime. The un-suffixed `load8`, `load16`, and `load32` forms are accepted as zero-extending compatibility aliases; the revised `load8u/s`, `load16u/s`, and `load32u/s` forms are also supported. Shift counts are masked to 0–63. `itof` treats its source as signed 64-bit; `ftoi` truncates finite signed-64-bit-range values.

Calls and syscalls validate but deliberately do not execute: AXIR leaves their ABI, target address model, and syscall convention to a backend mapping file. Direct machine-code emission, linking/optimization, and target-mapping parsing are therefore the next implementation layer, not simulated by the reference interpreter.

## Tests

```sh
sh tests/test_axir.sh
```

The test set builds the compiler, validates a fixture covering every v1.0 opcode, runs integer/FP/memory/control-flow integration coverage, verifies canonical dumping, and asserts rejection of an invalid slot-class program.
