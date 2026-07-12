# AXIR self-hosting plan

## Branch roles

- `bootstrap` (`ab50c51`) contains the working C++ AXIR implementation. It is the trusted bootstrap only.
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
   - Implement code emission for the concrete host `target.yml`.
   - Emit and run a standalone AXIR compiler artifact without an external assembler or linker.

5. **Fixed-point rebuild**
   - Build the AXIR compiler with the C++ bootstrap.
   - Use the resulting AXIR compiler to rebuild its own source.
   - Require byte-identical artifacts, or record and verify deterministic permitted metadata differences.

## Current status

Stage 0 is complete: the C++ bootstrap snapshot is committed on `bootstrap`.

Stage 1 has a concrete Linux x86-64 ELF target configuration, but its backend, ELF writer, and bootstrap execution support are not implemented yet. Stages 2–5 are not implemented. The target configuration is a contract, not a substitute for the required AXIR-written frontend, linker, backend, and fixed-point rebuild.

## Next implementation milestone

Implement the Linux x86-64 ELF backend described by `targets/linux_x86_64.yml` in the bootstrap, then use it to execute AXIR runtime-substrate programs. The self-hosted compiler source can begin only after that bootstrap execution contract is verified.
