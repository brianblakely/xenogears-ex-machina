#!/usr/bin/env bash
# Build the fiber stand-in module (program.c with port/fiber.c and
# port/arena_task.c) like build/game/game.wasm and run the runtime's test of
# it through wasm2c (runtime/crates/xem-game/tests/fiber_module.rs).
# Run from the repository root inside `nix develop path:./nix/runtime`.
set -euo pipefail
out=build/fiber_module
mkdir -p "$out"
flags=(--target=wasm32-unknown-unknown -O2 -std=gnu89 -nostdinc -ffreestanding -fno-builtin -funsigned-char
       -fwrapv -fno-strict-aliasing -w -include tests/fiber_module/imports.h -Iport/include -Idecomp/include)
objects=()
for source in tests/fiber_module/program.c port/fiber.c port/arena_task.c; do
    object="$out/$(basename "$source" .c).o"
    "$XEM_CLANG" "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
"$XEM_WASM_LD" --no-entry --initial-memory=$((0x80200000)) --max-memory=$((0x80200000)) \
    -z stack-size=$((0x100000)) --stack-first --export=__stack_pointer \
    --export=xem_run --export=xem_task_run --export=xem_call --export=xem_interrupt --export=xem_unwind_area \
    "${objects[@]}" -o "$out/raw.wasm"
wasm-opt -O2 --asyncify --pass-arg=asyncify-imports@xem.yield,xem.restart,xem.task_switch \
    --enable-bulk-memory --enable-sign-ext --enable-mutable-globals "$out/raw.wasm" -o "$out/fiber.wasm"
cd runtime
XEM_GAME_WASM="$PWD/../$out/fiber.wasm" XEM_FIBER_MODULE=1 \
    cargo test -p xem-game --test fiber_module --target-dir "../$out/target" -- --nocapture
