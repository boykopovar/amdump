# amdump

Dumps Android process memory into an ELF64 core file with `PT_LOAD` segments (process regions), loadable in a disassembler. Correctness of cross-region jump instructions is preserved.

## Build

CMake >= 3.16, C++17.

```
cmake -B build && cmake --build build
```

No platform-specific calls, no bionic, no Android NDK. Strictly standard C++. Native reading of `/proc/`.

## Usage

```
amdump <pid|package> <outfile> [--max-gb <N>] [--only-show-regions] [--region-prefix <prefix> ...]
```

| Option              | Description                                                                                   |
|---------------------|-----------------------------------------------------------------------------------------------|
| pid \| package      | PID or package name (lookup via `/proc/*/cmdline`).                                           |
| --max-gb            | Dump size limit, default 10. Writing does not start if the estimated size exceeds this value. |
| --only-show-regions | Print regions (including so library names), without creating a dump.                          |
| --region-prefix     | Filter regions by name prefix, applied to both dump writing and `--only-show-regions` output. |
