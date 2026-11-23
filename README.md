# Operating System Project

## Requirements

`nasm`, `qemu-system-x86_64`, and a 32-bit x86 GCC/binutils toolchain.

## Tests

| Test          | Description                       |
|---------------|-----------------------------------|
| `frame-pool`  | Contiguous frame pool allocator   |
| `paging`      | Page table and page fault handler |
| `vm-pool`     | Virtual memory pools              |
| `scheduler`   | Kernel threads and scheduler      |
| `disk`        | Non-blocking disk with threads    |
| `file-system` | Simple file system (default)      |

## Usage

```
make TEST=<test>
make TEST=<test> run
make clean
```

May need to override toolchain name on systems like Arch: `make LD=ld`.

Shorthands: `make <test>`, `make run-<test>`, `make debug-<test>`.
