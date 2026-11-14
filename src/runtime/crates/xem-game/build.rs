//! Translate build/game/game.wasm (tools/game_module.py) with wasm2c and compile
//! it with generated import glue. Without a game module (it needs the user's
//! matched PS1 builds) the crate builds with `has_game_module` unset.

use std::env;
use std::fmt::Write as _;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use sha2::{Digest, Sha256};

/// The C files wasm2c writes the module's translation in.
const PARTS: usize = 16;

/// The calls the game may have under way, as wasm2c's own count allows
/// (`WASM_RT_MAX_CALL_STACK_DEPTH`): one more traps as an exhausted stack.
const MAX_CALL_DEPTH: u32 = 500;

fn main() {
    println!("cargo::rustc-check-cfg=cfg(has_game_module)");
    println!("cargo::rerun-if-env-changed=XEM_GAME_WASM");
    println!("cargo::rerun-if-env-changed=XEM_WASM2C_RUNTIME");
    // XEM_GAME_OPT=0 compiles the translated module unoptimised, for
    // backtraces that name every game function.
    println!("cargo::rerun-if-env-changed=XEM_GAME_OPT");
    println!("cargo::rerun-if-env-changed=XEM_GAME_DEBUG");
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let wasm = env::var_os("XEM_GAME_WASM")
        .map(PathBuf::from)
        .unwrap_or_else(|| manifest.join("../../../../build/game/game.wasm"));
    println!("cargo::rerun-if-changed={}", wasm.display());
    let (Ok(runtime), true) = (env::var("XEM_WASM2C_RUNTIME"), wasm.exists()) else {
        println!("cargo::warning=no game module at {} (or XEM_WASM2C_RUNTIME unset): building without it", wasm.display());
        return;
    };
    let include = env::var("XEM_WASM2C_INCLUDE").expect("XEM_WASM2C_INCLUDE");
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let digest = Sha256::digest(fs::read(&wasm).expect("game module"));
    let module_id: String = digest.iter().map(|b| format!("{b:02x}")).collect();
    println!("cargo::rustc-env=XEM_MODULE_ID={module_id}");
    // tools/game_module.py writes the module's schema beside it: the session's
    // introspection, embedded so every host has it.
    let schema = wasm.with_file_name("schema.json");
    println!("cargo::rerun-if-changed={}", schema.display());
    fs::copy(&schema, out.join("schema.json")).expect("build/game/schema.json (tools/game_module.py writes it)");
    // The translation in parts (game_0.c...) that compile in parallel (cc's
    // `parallel` feature, under cargo's jobs): as one file it took GCC
    // nearly four minutes, in 16 parts 41 seconds on 16 threads, the part
    // with the largest game functions (the menu's and the battle's mode
    // loops) the longest.
    let status = Command::new("wasm2c")
        .args(["-n", "game", "--num-outputs", &PARTS.to_string(), "-o"])
        .arg(out.join("game.c"))
        .arg(&wasm)
        .status()
        .expect("wasm2c (enter nix/runtime)");
    assert!(status.success(), "wasm2c failed");
    count_calls_in_the_instance(&out);

    // Where a 64-bit host can tell the game's faults from its own (Linux and
    // Android: the glue's signal handler), game memory is its 2 GiB at the
    // start of 8 GiB reserved, which every address the translation can form
    // (32 bits plus a 32-bit offset) lies in: an access beyond the memory
    // faults and traps, and none is checked. Elsewhere each access is
    // compared with the memory's size first. (The checks took 8% of a game
    // frame's time: 1.22 ms against 1.12 on a field, 3.13 against 2.89 in a
    // battle.)
    let os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let guarded = matches!(os.as_str(), "linux" | "android") && env::var("CARGO_CFG_TARGET_POINTER_WIDTH").unwrap() == "64";

    let header = fs::read_to_string(out.join("game.h")).unwrap();
    let (glue, names) = glue(&header);
    fs::write(out.join("glue.c"), glue).unwrap();
    let mut rust = String::from("pub const IMPORT_NAMES: &[&str] = &[\n");
    for name in &names {
        writeln!(rust, "    {name:?},").unwrap();
    }
    rust.push_str("];\n");
    fs::write(out.join("imports.rs"), rust).unwrap();

    let mut build = cc::Build::new();
    build
        .files((0..PARTS).map(|part| out.join(format!("game_{part}.c"))))
        .file(out.join("glue.c"))
        .file(format!("{runtime}/wasm-rt-impl.c"))
        .file(format!("{runtime}/wasm-rt-mem-impl.c"))
        .file(format!("{runtime}/wasm-rt-exceptions-impl.c"))
        .include(&out)
        .include(&include)
        .include(&runtime)
        .define(if guarded { "WASM_RT_MEMCHECK_GUARD_PAGES" } else { "WASM_RT_MEMCHECK_BOUNDS_CHECK" }, "1")
        // wasm2c's own handler takes every fault of the process for the
        // game's; the glue installs one that knows them apart.
        .define("WASM_RT_SKIP_SIGNAL_RECOVERY", "1")
        // The calls are counted in the instance (count_calls_in_the_instance).
        .define("WASM_RT_NONCONFORMING_UNCHECKED_STACK_EXHAUSTION", "1")
        .define("XEM_MAX_CALL_DEPTH", MAX_CALL_DEPTH.to_string().as_str())
        .define("XEM_GUARDED_MEMORY", if guarded { "1" } else { "0" })
        .opt_level(env::var("XEM_GAME_OPT").ok().and_then(|v| v.parse().ok()).unwrap_or(2))
        // Debug information for the translated module is large and makes GCC's
        // variable tracking fail on it; the game's debugging goes through the
        // schema and commands instead.
        .debug(false)
        .warnings(false)
        // The parts' functions are global, and position-independent code
        // would otherwise keep each call open to interposition: no inlining
        // even within a part.
        .flag_if_supported("-fno-semantic-interposition")
        // Each floating-point operation rounded on its own, as wasm's are:
        // GCC otherwise fuses a product into the sum after it where the
        // processor has a fused multiply-add (for aarch64, two fmadd and
        // three fmsub in xem_present_face_visible), and the port's tests of
        // which culled faces a host reveals would round as no browser does.
        .flag_if_supported("-ffp-contract=off");
    // XEM_GAME_DEBUG=1: line tables (no variable tracking), so backtraces
    // through the translated module name its lines.
    if env::var_os("XEM_GAME_DEBUG").is_some() {
        build.flag("-g1").flag("-fno-var-tracking");
    }
    build
        .compile("xem_game");
    println!("cargo::rustc-cfg=has_game_module");
    // Dependents (crates with `xem-game` as a dependency) see DEP_XEM_GAME_MODULE.
    println!("cargo::metadata=module=1");
}

/// Makes the translation count its calls in the instance. wasm2c counts
/// them in a thread's variable, which position-independent code reaches
/// through a call that every function then makes on entry and again on
/// return: a tenth of a game frame's time (3.13 ms in a battle, against 2.81
/// with no count at all). A word of the instance, which every function
/// holds, costs an increment and a comparison. The glue starts the count
/// again after a trap.
fn count_calls_in_the_instance(out: &Path) {
    let edit = |file: &str, edit: &dyn Fn(String) -> String| {
        let path = out.join(file);
        fs::write(&path, edit(fs::read_to_string(&path).unwrap())).unwrap();
    };
    edit("game.h", &|text| {
        assert_eq!(text.matches("} w2c_game;").count(), 1, "wasm2c's game.h does not end the instance as this build expects");
        text.replace("} w2c_game;", "  u32 xem_depth;\n} w2c_game;")
    });
    edit("game-impl.h", &|text| {
        // wasm2c's two definitions of a function's entry and return: with
        // its count of calls and without.
        let begin = text.find("#if WASM_RT_STACK_DEPTH_COUNT\n#define FUNC_PROLOGUE");
        let end = begin.and_then(|begin| text[begin..].find("#endif\n").map(|end| begin + end + "#endif\n".len()));
        let (Some(begin), Some(end)) = (begin, end) else { panic!("wasm2c's game-impl.h does not count calls as this build expects") };
        let counted = &text[begin..end];
        assert!(
            counted.matches("#define FUNC_PROLOGUE").count() == 2
                && counted.matches("#define FUNC_EPILOGUE").count() == 2
                && text.matches("wasm_rt_call_stack_depth").count() == counted.matches("wasm_rt_call_stack_depth").count(),
            "wasm2c's game-impl.h does not count calls as this build expects"
        );
        format!(
            "{}#define FUNC_PROLOGUE \\\n  if (++instance->xem_depth > XEM_MAX_CALL_DEPTH) \\\n    TRAP(EXHAUSTION);\n\n#define FUNC_EPILOGUE --instance->xem_depth\n{}",
            &text[..begin],
            &text[end..]
        )
    });
}

/// The import glue: every `xem` import forwards its arguments to one Rust
/// callback, which answers with a value, an unwind or a trap. An import the
/// game resumes from (asyncify rewinding) stops the rewind and returns the
/// resume value.
fn glue(header: &str) -> (String, Vec<String>) {
    let mut c = String::from(
        r#"#include "game.h"
#include "wasm-rt.h"
#include "wasm-rt-exceptions.h"
#include "wasm-rt-impl.h"
#include <stdlib.h>

struct w2c_xem {
    void *host;
    int (*import)(void *host, unsigned index, const u32 *args, unsigned count, u32 *result);
    w2c_game *instance;
    u32 resume_value;
    u32 area;
};

static u32 xem_forward(struct w2c_xem *x, unsigned index, const u32 *args, unsigned count) {
    u32 result = 0;
    int action;
    if (w2c_game_asyncify_get_state(x->instance) == 2) {
        w2c_game_asyncify_stop_rewind(x->instance);
        return x->resume_value;
    }
    action = x->import(x->host, index, args, count, &result);
    if (action == 1) {
        x->area = w2c_game_xem_unwind_area(x->instance);
        w2c_game_asyncify_start_unwind(x->instance, x->area);
    } else if (action == 2) {
        wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);
    }
    return result;
}

"#,
    );
    let mut names = Vec::new();
    let mut lines = header.lines();
    while let Some(line) = lines.next() {
        let Some(rest) = line.strip_prefix("/* import: 'xem' '") else { continue };
        let name = rest.split('\'').next().unwrap().to_string();
        let proto = lines.next().unwrap();
        // e.g. `void w2c_xem_restart(struct w2c_xem*, u32, u32);`
        let ret = proto.split_whitespace().next().unwrap();
        let params = &proto[proto.find('(').unwrap() + 1..proto.rfind(')').unwrap()];
        let types: Vec<&str> = params.split(',').map(str::trim).skip(1).collect();
        assert!(types.iter().all(|t| *t == "u32") && (ret == "void" || ret == "u32"), "import {name}: {proto}");
        let args = Vec::from_fn(types.len(), |i| format!("u32 a{i}"));
        let values = Vec::from_fn(types.len(), |i| format!("a{i}"));
        let index = names.len();
        writeln!(
            c,
            "{ret} w2c_xem_{name}(struct w2c_xem *x{}{}) {{\n    u32 args[{}] = {{{}}};\n    {}xem_forward(x, {index}, args, {});\n}}\n",
            if args.is_empty() { "" } else { ", " },
            args.join(", "),
            values.len().max(1),
            if values.is_empty() { "0".into() } else { values.join(", ") },
            if ret == "void" { "" } else { "return " },
            values.len()
        )
        .unwrap();
        names.push(name);
    }
    c.push_str(
        r#"
#if XEM_GUARDED_MEMORY
#include <pthread.h>
#include <signal.h>
#include <string.h>

/* What wasm-rt-mem-impl.c reserves for a guarded memory. */
#define XEM_RESERVED_BYTES 0x200000000ull

/* The instance whose code this thread runs (none: NULL). */
static _Thread_local w2c_game *xem_running;
static struct sigaction xem_previous_action;
static pthread_once_t xem_handler_once = PTHREAD_ONCE_INIT;

/* A fault within the reserved range of the game this thread runs is the
 * game's access beyond its memory: it traps. Any other is someone else's:
 * the handler installed before this one's (Rust's, which tells a thread's
 * stack overflow; a Java runtime's), or the default action. */
static void xem_fault(int signal, siginfo_t *info, void *context) {
    w2c_game *instance = xem_running;
    if (instance != NULL) {
        uintptr_t address = (uintptr_t)info->si_addr;
        uintptr_t base = (uintptr_t)w2c_game_memory(instance)->data;
        if (address >= base && address - base < XEM_RESERVED_BYTES) {
            wasm_rt_trap(WASM_RT_TRAP_OOB);
        }
    }
    if (xem_previous_action.sa_flags & SA_SIGINFO) {
        xem_previous_action.sa_sigaction(signal, info, context);
    } else if (xem_previous_action.sa_handler == SIG_DFL || xem_previous_action.sa_handler == SIG_IGN) {
        /* The access is made again and takes the default action. */
        sigaction(signal, &xem_previous_action, NULL);
    } else {
        xem_previous_action.sa_handler(signal);
    }
}

static void xem_install_handler(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = xem_fault;
    /* On the thread's alternate stack where it has one, as the handler
     * before this one may need (a stack overflow leaves no other). */
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGSEGV, &action, &xem_previous_action) != 0) {
        abort();
    }
}

#define XEM_RUNNING(instance) (xem_running = (instance))
#else
#define XEM_RUNNING(instance) ((void)0)
#endif

/* Run an export under wasm2c's trap handler: 0, or the trap code (the calls
 * the trap left under way are counted no more). */
#define XEM_RUN(instance, call)                   \
    do {                                          \
        wasm_rt_trap_t code = wasm_rt_impl_try(); \
        if (code != 0) {                          \
            XEM_RUNNING(NULL);                    \
            (instance)->xem_depth = 0;            \
            return (int)code;                     \
        }                                         \
        XEM_RUNNING(instance);                    \
        call;                                     \
        XEM_RUNNING(NULL);                        \
        return 0;                                 \
    } while (0)

int xem_game_run(w2c_game *instance, u32 kind, u32 arg) { XEM_RUN(instance, w2c_game_xem_run(instance, kind, arg)); }

int xem_game_run_task(w2c_game *instance) { XEM_RUN(instance, w2c_game_xem_task_run(instance)); }

int xem_game_call(w2c_game *instance, u32 address) { XEM_RUN(instance, w2c_game_xem_call(instance, address)); }

int xem_game_interrupt(w2c_game *instance, u32 irq, u32 detail) { XEM_RUN(instance, w2c_game_xem_interrupt(instance, irq, detail)); }

const char *xem_game_trap_description(int code) {
    return wasm_rt_strerror((wasm_rt_trap_t)code);
}

w2c_game *xem_game_new(struct w2c_xem *env) {
    w2c_game *instance;
    wasm_rt_init();
#if XEM_GUARDED_MEMORY
    pthread_once(&xem_handler_once, xem_install_handler);
#endif
    instance = (w2c_game *)calloc(1, sizeof(w2c_game));
    env->instance = instance;
    wasm2c_game_instantiate(instance, env);
    return instance;
}

void xem_game_free(w2c_game *instance) {
    wasm2c_game_free(instance);
    free(instance);
}

u8 *xem_game_memory(w2c_game *instance, u64 *size) {
    wasm_rt_memory_t *memory = w2c_game_memory(instance);
    *size = memory->size;
    return memory->data;
}

u32 *xem_game_stack_pointer(w2c_game *instance) { return w2c_game_0x5F_stack_pointer(instance); }
u32 xem_game_async_state(w2c_game *instance) { return w2c_game_asyncify_get_state(instance); }
void xem_game_stop_unwind(w2c_game *instance) { w2c_game_asyncify_stop_unwind(instance); }
void xem_game_start_rewind(w2c_game *instance, u32 area) { w2c_game_asyncify_start_rewind(instance, area); }
"#,
    );
    (c, names)
}
