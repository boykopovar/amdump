# amdump

Dumps Android process memory into an ELF64 core file with `PT_LOAD` segments (process regions), loadable in a disassembler. Correctness of cross-region jump instructions is preserved.

## Build

CMake >= 3.16, C++17.

```
cmake -B build && cmake --build build
```

## Usage

```
amdump <pid|package> <outfile> [--max-gb <N>] [--only-show-regions] [--region-prefix <prefix> ...]
```

- `pid|package` - PID or package name (lookup via `/proc/*/cmdline`).
- `--max-gb` - dump size limit, default 10. Writing does not start if the estimated size exceeds this value.
- `--only-show-regions` - print regions (including so library names), without creating a dump.
- `--region-prefix` - filter regions by name prefix, applied to both dump writing and `--only-show-regions` output.
