# AXIR self-hosting plan

## Branch roles

- `bootstrap` (`ab50c51`) contains the working C++ AXIR implementation. It is the trusted bootstrap only.
- `main` is reserved for the AXIR-written self-hosted implementation and its specifications.

The AXIR v1.0 specification is tracked on `main` at `axir/AXIR_SPEC_v1.0.md` so that the self-hosted source has a stable, branch-local source of truth.

## Definition of done

AXIR is self-hosted only when an AXIR-language implementation can accept AXIR source, validate and link it according to v1.0, emit a target artifact, and be rebuilt by the prior AXIR bootstrap artifact. The C++ implementation must not be part of that execution path except as the original bootstrap runner.

## Required stages

1. **Host target contract**
   - Add a concrete `target.yml` for one host architecture.
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

Stages 1–5 are not implemented. The v1.0 specification deliberately leaves syscall mappings, executable format, ABI, target instruction encodings, and target mapping files backend-defined. A real self-host implementation cannot be honestly produced until the first concrete target contract is selected.

## First implementation decision needed

Choose the first host target mapping (for example, Linux x86-64 ELF, Windows x86-64 PE, or another concrete target). The target must define the full data needed by the AXIR v1.0 `Backend Mapping` section before Stage 1 implementation begins.
