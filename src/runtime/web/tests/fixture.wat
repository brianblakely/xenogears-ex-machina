;; A stand-in game module for the browser host's public tests: the same imports
;; and exports as build/game/game.wasm (tools/game_module.py), made suspendable
;; by `wasm-opt --asyncify` at xem.yield and xem.restart like the real one.
;;
;; Under the runtime's virtual clock every wait here lasts until the next
;; vertical blank, so each is one frame:
;; xem_run(0, _): write 0, 1, 2 to RAM word 0x80000000, waiting for VSync after
;;   each, then restart into xem_run(1, 100).
;; xem_run(1, n): write 0..n-1 to RAM word 0x80000004, polling after each, then
;;   call stub #0 (a function the port does not define).
;; With RAM word 0x80000100 nonzero before the first frame, xem_run(0, _)
;; instead saves a file on memory card 1 through the BIOS file calls the port
;; makes (create "bu00:XEMTEST", write the 128 bytes at 0x80000400, close),
;; leaves the descriptor and the write's result at 0x80000104 and 0x80000108,
;; and then waits for VSync forever.
(module
  (import "xem" "yield" (func $yield (param i32)))
  (import "xem" "restart" (func $restart (param i32 i32)))
  (import "xem" "missing" (func $missing (param i32)))
  (import "xem" "card_open" (func $card_open (param i32 i32) (result i32)))
  (import "xem" "card_write" (func $card_write (param i32 i32 i32) (result i32)))
  (import "xem" "card_close" (func $card_close (param i32) (result i32)))
  ;; RAM keeps its KSEG0 address, as in the real module.
  (memory (export "memory") 32800 32800)
  (global $sp (export "__stack_pointer") (mut i32) (i32.const 0x10000))
  (data (i32.const 0x80000300) "bu00:XEMTEST\00")
  (data (i32.const 0x80000400) "Saved by the browser host's card test.")

  ;; The asyncify save area follows the stack in the module's low memory.
  (func (export "xem_unwind_area") (result i32)
    (i32.store (i32.const 0x10000) (i32.const 0x10008))
    (i32.store (i32.const 0x10004) (i32.const 0x20000))
    (i32.const 0x10000))

  (func (export "xem_call") (param $address i32))

  (func (export "xem_task_run") (unreachable))

  (func (export "xem_interrupt") (param $irq i32) (param $detail i32))

  (func (export "xem_run") (param $kind i32) (param $arg i32)
    (local $i i32)
    (local $fd i32)
    (if (i32.and (i32.eqz (local.get $kind)) (i32.ne (i32.load (i32.const 0x80000100)) (i32.const 0)))
      (then
        ;; Create with one block (blocks << 16 | 0x200), opened for writing (2).
        (local.set $fd (call $card_open (i32.const 0x80000300) (i32.const 0x10202)))
        (i32.store (i32.const 0x80000104) (local.get $fd))
        (i32.store (i32.const 0x80000108) (call $card_write (local.get $fd) (i32.const 0x80000400) (i32.const 128)))
        (drop (call $card_close (local.get $fd)))
        (loop $wait
          (call $yield (i32.const 1))
          (br $wait))))
    (if (i32.eqz (local.get $kind))
      (then
        (loop $boot
          (i32.store (i32.const 0x80000000) (local.get $i))
          (call $yield (i32.const 1))
          (local.set $i (i32.add (local.get $i) (i32.const 1)))
          (br_if $boot (i32.lt_u (local.get $i) (i32.const 3))))
        (call $restart (i32.const 1) (i32.const 100))
        (unreachable)))
    (block $done
      (loop $poll
        (br_if $done (i32.ge_u (local.get $i) (local.get $arg)))
        (i32.store (i32.const 0x80000004) (local.get $i))
        (call $yield (i32.const 3))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $poll)))
    (call $missing (i32.const 0))
    (unreachable)))
