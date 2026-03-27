#!/usr/bin/env sh

if command -v gdb-multiarch >/dev/null 2>&1; then
    GDB_CMD="gdb-multiarch"
elif command -v riscv64-elf-gdb >/dev/null 2>&1; then
    GDB_CMD="riscv64-elf-gdb"
else
    echo "Error: Neither gdb-multiarch nor riscv64-elf-gdb found."
    exit 1
fi

"$GDB_CMD" "./build/MultiPandOS" \
    -ex "set architecture riscv:rv64"
