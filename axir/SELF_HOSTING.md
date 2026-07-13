# AXIR self-hosting plan

## Branch roles

- `bootstrap` contains the working C++ AXIR implementation. It is the trusted bootstrap only.
- `main` is reserved for the AXIR-written self-hosted implementation and its specifications.

The AXIR v1.0 specification is tracked on `main` at `axir/AXIR_SPEC_v1.0.md` so that the self-hosted source has a stable, branch-local source of truth.

## Definition of done

AXIR is self-hosted only when an AXIR-language implementation can accept AXIR source, validate and link it according to v1.0, emit a target artifact, and be rebuilt by the prior AXIR bootstrap artifact. The C++ implementation must not be part of that execution path except as the original bootstrap runner.

## Required stages

1. **Host target contract**
   - The first configuration is `axir/targets/linux_x86_64.yml`, selected as `linux_x86_64` through `./targets/<target_name>.yml`.
   - Define the integer/FP ABI, file/input mechanism, syscall encodings, data layout, executable format, and entry point.
   - Implement the same contract in the C++ bootstrap branch so it can execute the first AXIR self-host binary.

2. **AXIR runtime substrate**
   - Implement allocation-free input buffering, output buffering, string scanning, integer formatting, and error reporting in AXIR.
   - Verify those routines as AXIR programs through the bootstrap runner.

3. **AXIR frontend and linker**
   - Implement tokenizer, parser, validator, label/data resolution, and module merging in AXIR.
   - Verify valid and invalid fixtures against the v1.0 specification.

4. **AXIR backend**
   - Parse → validate → link all units → optimize the linked program → expand target byte templates → write the executable image directly.
   - Emit a standalone executable binary by writing ELF bytes internally. Never emit assembly text and never invoke an external assembler or linker.
   - Object, static-library, and dynamic-library output are optional later modes; executable binary emission is the standard mode.

5. **Fixed-point rebuild**
   - Build the AXIR compiler with the C++ bootstrap.
   - Use the resulting AXIR compiler to rebuild its own source.
   - Require byte-identical artifacts, or record and verify deterministic permitted metadata differences.

## Current status

Stage 0 is complete: the C++ bootstrap snapshot is committed on `bootstrap`.

Stage 1 has a concrete Linux x86-64 ELF target configuration with direct machine-code byte templates and a required `syscall <name>` section for every symbolic syscall. Each section records Linux syscall bytes plus named `I[n]` argument/result slots. The bootstrap parses and validates those mappings, rejects a target with a missing mapping, and binds primitive syscall results to the declared result slot. Generated code uses raw Linux syscalls and never libc. The bootstrap resolves the configuration only from `./targets/<target_name>.yml` beside its binary and does so without a YAML library or other runtime dependency.

The bootstrap has a verified direct-ELF proof: it parses modules, links them, validates the merged program, removes no-op moves, expands configured bytes, allocates a stack frame for arbitrary integer virtual slots, relocates static data and `rel32` branches, writes a standalone ELF64 image itself, and executes AXIR `write` plus `exit` sequences. It emits separate `R E` code and `RW` static-data segments, including directly lowered integer loads and stores, without invoking an assembler or linker. This is deliberately only a backend subset (`mov`, immediate/slot `add`/`sub`/`mul`/division/modulo, `and`/`or`/`xor`, `shl`/`shrl`/`shra`, integer comparisons, integer loads/stores, `addr`, `jmp`, `cjump`, primitive syscall, and `hlt`); it is not a complete v1.0 backend or a self-hosted compiler. Primitive Linux syscalls currently support up to three target-defined arguments, while complete multi-unit symbol semantics, all remaining operations, calls, floating point, and runtime execution support remain unimplemented. Stages 2–5 are not implemented.

## Next implementation milestone

Complete the Linux x86-64 direct backend in the bootstrap: multi-unit linking, all v1.0 instruction families, calls, branches, writable storage, and deterministic ELF relocations. Then use it to execute AXIR runtime-substrate programs. The AXIR-written compiler source can begin only after that complete bootstrap execution contract is verified.
