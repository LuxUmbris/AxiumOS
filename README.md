# AxiumOS

Axium Open Structure (AOS).

## Axir compiler

`axir/` contains the initial cross-platform Axir compiler scaffold. It uses only portable C++ and a POSIX shell/GNU Make build interface; run it in a GCC-compatible environment such as Linux, WSL, macOS with GCC, or MSYS2.

```sh
cd axir
make cross-platform
./build/axirc
```

The equivalent direct build command is:

```sh
gcc src/*.cpp -Iinclude -o build/axirc -O3
```

`./build.sh` runs that command after creating `build/`.

### Layout

- `axir/src/main.cpp` — compiler entry point
- `axir/src/modules.cpp` — initial compiler module
- `axir/Makefile` — `build` and `cross-platform` Make targets
- `axir/build.sh` — portable shell build entry point
