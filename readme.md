# `Brainf*ck` x86-64 ELF Compiler

This is a minimal `Brainf*ck` to x86-64 ELF compiler written in C. It transforms `Brainf*ck` source code into a standalone Linux x86-64 ELF executable that runs natively without interpretation.

## Features

- **Direct compilation** of `Brainf*ck` commands (`> < + - . , [ ]`) into raw x86-64 machine code.
- Emits a fully valid **ELF64 executable** runnable on Linux.
- Implements `Brainf*ck` **loops** with jump offset patching.
- Uses Linux **syscalls** for input/output and program exit (no libc dependency).
- Allocates a fixed size (30,000 bytes) **tape** as the `Brainf*ck` data memory in the `.bss` segment.
- Uses the `r12` register as the **data pointer**.

## How it works

- The compiler reads the `Brainf*ck` source one character at a time, ignoring any non command characters.
- Each `Brainf*ck` command maps to one or more machine instructions appended to a code buffer.
- The ELF executable starts by initializing the `r12` register to point to the tape memory region located at a fixed virtual address.
- `>` and `<` increment/decrement the `r12` pointer.
- `+` and `-` increment/decrement the byte at `[r12]` using `inc`/`dec` instructions targeting memory.
- `.` triggers a syscall to write one byte from `[r12]` to standard output.
- `,` triggers a syscall to read one byte from standard input into `[r12]`.
- Loops `[` and `]` emit conditional jumps: the compiler tracks loop start locations on a stack, and patches jump addresses after parsing the matching bracket.
- At the end of the code, an exit syscall is emitted to cleanly terminate the program.
- The ELF headers and program headers are written out to produce a legitimate executable file.
- The tape is not stored inside the code segment but rather allocated in the `.bss` segment at a fixed address (`0x600000`).
- Syscalls are invoked directly through the `syscall` instruction to avoid runtime dependencies.

## Important design choices

- **Tape size** is fixed at 30000 bytes (this can be changed in the source).
- This implementation uses minimal instruction sequences to keep the binary small and fast.
- No external libraries or runtime dependencies are required.
- The resulting ELF is a statically linked, self contained native program.

## Potential improvements

- Tape size configurable at compile time or runtime.
- Additional optimizations like command merging (`+++` -> `add`) or loop unrolling.
- Support for more advanced ELF features like relocation or symbol tables.
- Target other architectures or OS'es.

## Code structure

- `Buf` and `Code` structures manage dynamic binary code emission buffers.
- Functions like `emit_inc_r12()`, `emit_syscall_write()` generate specific instruction sequences.
- Loops are managed by a manual stack that tracks jump offsets and patches them after code emission.
- ELF headers and program headers are manually written as byte sequences.
- Main function reads Brainfuck source, invokes compile process, and writes ELF output.

## License

This code is provided under the GNU GPLv3 license. Feel free to use, modify, and share under compatible terms.

---

This compiler is a compact demonstration of how to turn a minimalist esoteric language into a fully native executable, combining low level assembly understanding, ELF file format knowledge, and compiler basics in a single neat project.
