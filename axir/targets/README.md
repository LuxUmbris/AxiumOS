# AXIR targets

The compiler resolves a target by name relative to the executable:

```text
./targets/<target_name>.yml
```

For example, the first configured target is selected as `linux_x86_64` and is stored at `./targets/linux_x86_64.yml`.

Target mappings define the host ABI, executable format, machine word size, byte order, register classes, immediate and relocation rules, **machine-code byte templates**, and symbolic syscall mappings. The Linux mapping gives every required AXIR syscall a dedicated `syscall <name>` section with the exact syscall-number load bytes, `syscall` instruction bytes, and its `I[n]` argument/result-slot convention. Composite file calls expand into those primitive sections; they never use libc or another runtime. The internal backend copies and completes the templates while emitting an executable image directly: it must never generate assembly text or invoke an external assembler or linker. A target mapping is a required input to direct machine-code emission; it must be consumed by both the bootstrap and the self-hosted compiler.
