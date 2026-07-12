# AXIR — Axium Intermediate Representation

## Formal Specification (v1.0, final revised)

AXIR is the low-level, slot-based, integer-typeless but numerically aware
intermediate representation of the Axium Open Structure (AOS) toolchain.

AXIR is designed for:
- SSA-friendly optimization (SSA not enforced at IR level)
- pre–machine-code linking
- direct machine-code emission (no external assembler or linker)

AXIR separates:
- integer/pointer slots: I[n], n ∈ ℕ
- floating-point slots: F[n], n ∈ ℕ

All high-level semantics (types, structs, signedness at language level)
are defined in AxiumHL/Axium-CT, not in AXIR.

## Core Slot Model

I[n] : integer/pointer slots (raw machine words)
F[n] : floating-point slots (raw FP values)

AXIR uses a 3-address form where possible:

    I[dst] ← op(I[a], I[b])
    F[dst] ← op(F[a], F[b])

Multiple assignments to the same slot are allowed. SSA may be constructed
internally by the optimizer but is not required by the AXIR spec.

## Memory Model

Memory is raw byte-addressable storage. Pointers are represented in I[n].
AXIR provides explicit load/store instructions with defined granularities and
explicit signed/unsigned behavior for sub-word loads.

## Integer Load/Store (Zero- and Sign-Extension)

All sub-word loads exist in signed and unsigned variants.

Unsigned loads (zero-extension):

```text
load8u  I[dst], I[addr]
    Load 8-bit value from memory at address I[addr] into I[dst],
    zero-extended to machine word size.

load16u I[dst], I[addr]
    Load 16-bit value, zero-extended.

load32u I[dst], I[addr]
    Load 32-bit value, zero-extended.

load64  I[dst], I[addr]
    Load native 64-bit value (no extension needed).
```

Signed loads (sign-extension):

```text
load8s  I[dst], I[addr]
    Load 8-bit value from memory at address I[addr] into I[dst],
    sign-extended to machine word size.

load16s I[dst], I[addr]
    Load 16-bit value, sign-extended.

load32s I[dst], I[addr]
    Load 32-bit value, sign-extended.
```

Stores:

```text
store8  I[addr], I[src]
    Store low 8 bits of I[src] into memory at address I[addr].

store16 I[addr], I[src]
    Store low 16 bits of I[src] into memory at address I[addr].

store32 I[addr], I[src]
    Store low 32 bits of I[src] into memory at address I[addr].

store64 I[addr], I[src]
    Store 64 bits of I[src] into memory at address I[addr].
```

## Floating-Point Load/Store

```text
loadf32 F[dst], I[addr]
    Load 32-bit floating-point value from memory at address I[addr] into F[dst].

loadf64 F[dst], I[addr]
    Load 64-bit floating-point value from memory at address I[addr] into F[dst].

storef32 I[addr], F[src]
    Store 32-bit floating-point value F[src] into memory at address I[addr].

storef64 I[addr], F[src]
    Store 64-bit floating-point value F[src] into memory at address I[addr].
```

## Address-of Instruction

```text
addr I[dst], data_label
    Load the address of a static data object identified by data_label into I[dst].
```

## Integer Arithmetic

```text
add I[dst], I[a], I[b]
add I[dst], I[a], imm
    I[dst] ← I[a] + I[b] or I[a] + imm.

sub I[dst], I[a], I[b]
sub I[dst], I[a], imm
    I[dst] ← I[a] - I[b] or I[a] - imm.

mul I[dst], I[a], I[b]
mul I[dst], I[a], imm
    I[dst] ← I[a] * I[b] or I[a] * imm.

divs I[dst], I[a], I[b]
divs I[dst], I[a], imm
    Signed division: I[dst] ← I[a] / I[b] (signed) or I[a] / imm (signed).

divu I[dst], I[a], I[b]
divu I[dst], I[a], imm
    Unsigned division: I[dst] ← I[a] / I[b] (unsigned) or I[a] / imm (unsigned).

mods I[dst], I[a], I[b]
mods I[dst], I[a], imm
    Signed modulus: I[dst] ← I[a] mod I[b] (signed) or I[a] mod imm (signed).

modu I[dst], I[a], I[b]
modu I[dst], I[a], imm
    Unsigned modulus: I[dst] ← I[a] mod I[b] (unsigned) or I[a] mod imm (unsigned).

mov I[dst], I[src]
    I[dst] ← I[src].
```

## Bitwise Integer Operations

```text
and I[dst], I[a], I[b]
and I[dst], I[a], imm
    I[dst] ← I[a] & I[b] or I[a] & imm.

or I[dst], I[a], I[b]
or I[dst], I[a], imm
    I[dst] ← I[a] | I[b] or I[a] | imm.

xor I[dst], I[a], I[b]
xor I[dst], I[a], imm
    I[dst] ← I[a] ^ I[b] or I[a] ^ imm.

shl I[dst], I[a], I[b]
shl I[dst], I[a], imm
    I[dst] ← I[a] << I[b] or I[a] << imm.

shrl I[dst], I[a], I[b]
shrl I[dst], I[a], imm
    Logical right shift: zero-fill.

shra I[dst], I[a], I[b]
shra I[dst], I[a], imm
    Arithmetic right shift: sign-extend.
```

## Floating-Point Arithmetic

```text
addf F[dst], F[a], F[b]
    F[dst] ← F[a] + F[b].

subf F[dst], F[a], F[b]
    F[dst] ← F[a] - F[b].

mulf F[dst], F[a], F[b]
    F[dst] ← F[a] * F[b].

divf F[dst], F[a], F[b]
    F[dst] ← F[a] / F[b].

movf F[dst], F[src]
    F[dst] ← F[src].
```

## Integer/FP Conversion

```text
itof F[dst], I[src]
    Convert integer/pointer I[src] to floating-point F[dst].

ftoi I[dst], F[src]
    Convert floating-point F[src] to integer I[dst].
```

## Integer Comparisons (Signed/Unsigned)

```text
cmp_eq I[dst], I[a], I[b]
cmp_eq I[dst], I[a], imm
    I[dst] ← (I[a] == I[b]) ? 1 : 0 or (I[a] == imm) ? 1 : 0.

cmp_ne I[dst], I[a], I[b]
cmp_ne I[dst], I[a], imm
    I[dst] ← (I[a] != I[b]) ? 1 : 0 or (I[a] != imm) ? 1 : 0.
```

Signed:

```text
cmp_lts I[dst], I[a], I[b]
cmp_lts I[dst], I[a], imm
    I[dst] ← (I[a] < I[b]) ? 1 : 0 or (I[a] < imm) ? 1 : 0.

cmp_gts I[dst], I[a], I[b]
cmp_gts I[dst], I[a], imm
    I[dst] ← (I[a] > I[b]) ? 1 : 0 or (I[a] > imm) ? 1 : 0.

cmp_les I[dst], I[a], I[b]
cmp_les I[dst], I[a], imm
    I[dst] ← (I[a] <= I[b]) ? 1 : 0 or (I[a] <= imm) ? 1 : 0.

cmp_ges I[dst], I[a], I[b]
cmp_ges I[dst], I[a], imm
    I[dst] ← (I[a] >= I[b]) ? 1 : 0 or (I[a] >= imm) ? 1 : 0.
```

Unsigned:

```text
cmp_ltu I[dst], I[a], I[b]
cmp_ltu I[dst], I[a], imm
    I[dst] ← (I[a] < I[b]) ? 1 : 0 or (I[a] < imm) ? 1 : 0.

cmp_gtu I[dst], I[a], I[b]
cmp_gtu I[dst], I[a], imm
    I[dst] ← (I[a] > I[b]) ? 1 : 0 or (I[a] > imm) ? 1 : 0.

cmp_leu I[dst], I[a], I[b]
cmp_leu I[dst], I[a], imm
    I[dst] ← (I[a] <= I[b]) ? 1 : 0 or (I[a] <= imm) ? 1 : 0.

cmp_geu I[dst], I[a], I[b]
cmp_geu I[dst], I[a], imm
    I[dst] ← (I[a] >= I[b]) ? 1 : 0 or (I[a] >= imm) ? 1 : 0.
```

## Floating-Point Comparisons

```text
cmp_eqf I[dst], F[a], F[b]
    I[dst] ← (F[a] == F[b]) ? 1 : 0.

cmp_nef I[dst], F[a], F[b]
    I[dst] ← (F[a] != F[b]) ? 1 : 0.

cmp_ltf I[dst], F[a], F[b]
    I[dst] ← (F[a] < F[b]) ? 1 : 0.

cmp_gtf I[dst], F[a], F[b]
    I[dst] ← (F[a] > F[b]) ? 1 : 0.

cmp_lef I[dst], F[a], F[b]
    I[dst] ← (F[a] <= F[b]) ? 1 : 0.

cmp_gef I[dst], F[a], F[b]
    I[dst] ← (F[a] >= F[b]) ? 1 : 0.
```

## Control Flow

```text
jmp label
    Unconditional jump to label.

cjump I[cond], label
    Conditional jump. If I[cond] != 0, jump to label.
```

## Labels

```text
label:
    Defines a jump target.
```

## Functions and Arguments

Function definition (conceptual, provided by AxiumHL/Axium-CT):

```text
func my_function(I[a], I[b], F[c])
```

Static call:

```text
call my_function, I[x], I[y], F[z]
```

Indirect call (function pointer):

```text
call_ptr I[target_addr], I[x], I[y], F[z]
```

Semantics:
- Static call: target is a label; backend maps to direct call.
- Indirect call: target is an address in I[target_addr]; backend maps to
  indirect call/jump through register.
- Argument mapping (slots → registers/stack) is backend-defined.

## Return Instructions

```text
ret I[a]
    Return integer/pointer value I[a] from the current function.

ret imm
    Return immediate value from the current function.

ret_void
    Return from the current function with no value.
```

## Process Control

```text
hlt I[a]
hlt imm
    Terminate the program with the given exit code.
```

## Syscall Instructions

```text
syscall name
    Perform a symbolic system call. AXIR does not define syscall numbers or
    calling conventions. The selected target configuration maps each symbolic
    name to platform-specific byte sequences and a mandatory slot signature.
```

For every required `syscall name`, the target configuration MUST contain a
`syscall <name>` section with: the input `I[n]` slot for every argument, the
result `I[n]` slot when there is one, exact syscall-number and invoke bytes,
and the target's error and register-clobber convention. The caller writes
argument values into the documented slots before the instruction and reads the
documented result slot afterward. Linux x86-64 uses a non-negative result or a
negative errno value; its composite `read_file` and `write_file` entries expand
only to the primitive syscall sections. These sequences never call libc or any
other external runtime.

Required symbolic syscall names:

```text
read
write
read_file
write_file
open
close
seek
stat
remove
mkdir
rmdir
time
sleep
exit
```

Additional syscall names may be defined by the backend.

## Static Data Sections

AXIR supports static data declarations for global data and strings.

Data declaration:

```text
data my_string: "Hello World", 0
    Defines a null-terminated byte sequence labeled my_string.

data my_global_array: reserve 32
    Reserves 32 bytes of uninitialized storage labeled my_global_array.
```

Access:

```text
addr I[dst], my_string
    Load the address of my_string into I[dst].

addr I[dst], my_global_array
    Load the address of my_global_array into I[dst].
```

## Program Structure

An AXIR program consists of:
- one or more function bodies (instruction streams with labels)
- zero or more static data sections (data declarations)

Functions are defined by labels and terminated by ret or hlt. AXIR has no
explicit function prologue or epilogue; these are backend-generated.

## Linking Model

AXIR performs pre–machine-code linking:

- merge AXIR modules (functions + data)
- resolve labels and function calls (static and indirect)
- resolve data labels and addr references
- perform dead-code elimination
- perform constant propagation
- perform inlining
- perform slot renumbering
- perform SSA-friendly dataflow optimizations

SSA is not enforced at the IR level. The optimizer may construct SSA internally
(e.g., with phi-nodes or block arguments), but AXIR remains a practical,
3-address, multi-assignment IR.

## Backend Mapping

All AXIR instructions are mapped to machine instructions through target mapping
files (e.g., target.yml). These files define:

- instruction byte encodings
- syscall byte sequences
- integer and floating-point register classes
- register allocation rules
- calling conventions
- machine word size
- endianness
- jump offset encoding
- immediate encoding rules
- function prologue/epilogue sequences
- static data layout and section placement

## Guarantees

- Explicit integer vs. floating-point slot spaces.
- Explicit load/store with sub-word granularity and defined extension behavior.
- Signed and unsigned comparisons, division, and modulus.
- Support for static data and string storage via data sections.
- Support for static and indirect function calls.
- SSA-friendly 3-address form without enforcing SSA.
- Deterministic instruction semantics.
- Infinite virtual slot space.
- No types, no registers, no ABI in AXIR itself.
- Fully linked before machine-code emission.
- Fully portable across architectures.
- Direct machine-code emission (no external linker/assembler).
- Safe and aggressive optimization.

End of AXIR Specification v1.0 (final revised)
