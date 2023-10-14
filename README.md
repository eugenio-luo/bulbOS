# BulbOS

BulbOS is a toy operating system still in development targetting x86 32bit architecture runnable on the emulator QEMU

## How to compile and run

### requirements:
- `gcc` cross-compiler for i386 ([guide from osdev.org](https://wiki.osdev.org/GCC_Cross-Compiler))
- `QEMU` emulator
- `grub-mkrescue`
- `gdb` (optional but useful for debugging)

To compile and run, `i386-elf-gcc` and `i386-elf-ar` has to be in `$PATH` or change the variables`CC` and `AR` in `Makefile`, then:

```bash
make
```
To run with `gdb`:

```bash
make debug
```

## TODO

- complete rewrite of the project to structure it better
- write unit tests for every part
- loading process
- syscalls