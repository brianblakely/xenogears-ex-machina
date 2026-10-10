//! Translate build/game/game.wasm (tools/game_module.py) with wasm2c and compile
//! it with generated import glue. Without a game module (it needs the user's
//! matched PS1 builds) the crate builds with `has_game_module` unset.

use std::env;
use std::fmt::Write as _;
use std::fs;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    println!("cargo::rustc-check-cfg=cfg(has_game_module)");
    println!("cargo::rerun-if-env-changed=XEM_GAME_WASM");
    println!("cargo::rerun-if-env-changed=XEM_WASM2C_RUNTIME");
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let wasm = env::var_os("XEM_GAME_WASM")
        .map(PathBuf::from)
        .unwrap_or_else(|| manifest.join("../../../build/game/game.wasm"));
    println!("cargo::rerun-if-changed={}", wasm.display());
    let (Ok(runtime), true) = (env::var("XEM_WASM2C_RUNTIME"), wasm.exists()) else {
        println!("cargo::warning=no game module at {} (or XEM_WASM2C_RUNTIME unset): building without it", wasm.display());
        return;
    };
    let include = env::var("XEM_WASM2C_INCLUDE").expect("XEM_WASM2C_INCLUDE");
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let status = Command::new("wasm2c")
        .args(["-n", "game", "-o"])
        .arg(out.join("game.c"))
        .arg(&wasm)
        .status()
        .expect("wasm2c (enter nix/runtime)");
    assert!(status.success(), "wasm2c failed");

    let header = fs::read_to_string(out.join("game.h")).unwrap();
    let (glue, names) = glue(&header);
    fs::write(out.join("glue.c"), glue).unwrap();
    let mut rust = String::from("pub const IMPORT_NAMES: &[&str] = &[\n");
    for name in &names {
        writeln!(rust, "    {name:?},").unwrap();
    }
    rust.push_str("];\n");
    fs::write(out.join("imports.rs"), rust).unwrap();

    cc::Build::new()
        .files([out.join("game.c"), out.join("glue.c")])
        .file(format!("{runtime}/wasm-rt-impl.c"))
        .file(format!("{runtime}/wasm-rt-mem-impl.c"))
        .file(format!("{runtime}/wasm-rt-exceptions-impl.c"))
        .include(&out)
        .include(&include)
        .include(&runtime)
        .define("WASM_RT_MEMCHECK_BOUNDS_CHECK", "1")
        .define("WASM_RT_SKIP_SIGNAL_RECOVERY", "1")
        .opt_level(2)
        .warnings(false)
        .compile("xem_game");
    println!("cargo::rustc-cfg=has_game_module");
    // Dependents (crates with `xem-game` as a dependency) see DEP_XEM_GAME_MODULE.
    println!("cargo::metadata=module=1");
}

/// The import glue: every `xem` import forwards its arguments to one Rust
/// callback, which answers with a value, an unwind or a trap. An import the
/// game resumes from (asyncify rewinding) stops the rewind and returns the
/// resume value.
fn glue(header: &str) -> (String, Vec<String>) {
    let mut c = String::from(
        r#"#include "game.h"
#include "wasm-rt.h"

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
        let args: Vec<String> = (0..types.len()).map(|i| format!("u32 a{i}")).collect();
        let values: Vec<String> = (0..types.len()).map(|i| format!("a{i}")).collect();
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
/* Run an export under wasm2c's trap handler: 0, or the trap code. */
int xem_game_run(w2c_game *instance, u32 kind, u32 arg) {
    wasm_rt_trap_t code = wasm_rt_impl_try();
    if (code != 0) return (int)code;
    w2c_game_xem_run(instance, kind, arg);
    return 0;
}

int xem_game_call(w2c_game *instance, u32 address) {
    wasm_rt_trap_t code = wasm_rt_impl_try();
    if (code != 0) return (int)code;
    w2c_game_xem_call(instance, address);
    return 0;
}

const char *xem_game_trap_description(int code) {
    return wasm_rt_strerror((wasm_rt_trap_t)code);
}

w2c_game *xem_game_new(struct w2c_xem *env) {
    static int initialized;
    w2c_game *instance;
    if (!initialized) {
        wasm_rt_init();
        initialized = 1;
    }
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
u32 xem_game_data_end(w2c_game *instance) { return *w2c_game_0x5F_heap_base(instance); }
u32 xem_game_async_state(w2c_game *instance) { return w2c_game_asyncify_get_state(instance); }
void xem_game_stop_unwind(w2c_game *instance) { w2c_game_asyncify_stop_unwind(instance); }
void xem_game_start_rewind(w2c_game *instance) { w2c_game_asyncify_start_rewind(instance, instance->w2c_xem_instance->area); }
"#,
    );
    let c = c.replace("#include \"wasm-rt.h\"\n", "#include \"wasm-rt.h\"\n#include \"wasm-rt-exceptions.h\"\n#include \"wasm-rt-impl.h\"\n#include <stdlib.h>\n");
    (c, names)
}
