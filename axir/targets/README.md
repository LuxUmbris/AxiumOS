# AXIR targets

The compiler resolves a target by name relative to the executable:

```text
./targets/<target_name>.yml
```

For example, the first configured target is selected as `linux_x86_64` and is stored at `./targets/linux_x86_64.yml`.

Target mappings define the host ABI, executable format, machine word size, byte order, register classes, immediate and relocation rules, AXIR lowering strategies, and symbolic syscall mappings. A target mapping is a required input to direct machine-code emission; it must be consumed by both the bootstrap and the self-hosted compiler.
