//! Compiles the vendored rcheevos (rcheevos/README.md): its client, runtime,
//! server API and disc hashing, as the hosts use them. Every rcheevos call
//! is made on the host's thread (the server's answers come back to it), so
//! the library is built without its locks.

fn main() {
    let root = std::path::Path::new("rcheevos");
    let files = [
        "src/rc_client.c", "src/rc_compat.c", "src/rc_util.c", "src/rc_version.c",
        "src/rapi/rc_api_common.c", "src/rapi/rc_api_info.c", "src/rapi/rc_api_runtime.c", "src/rapi/rc_api_user.c",
        "src/rcheevos/alloc.c", "src/rcheevos/condition.c", "src/rcheevos/condset.c", "src/rcheevos/consoleinfo.c",
        "src/rcheevos/format.c", "src/rcheevos/lboard.c", "src/rcheevos/memref.c", "src/rcheevos/operand.c",
        "src/rcheevos/rc_validate.c", "src/rcheevos/richpresence.c", "src/rcheevos/runtime.c",
        "src/rcheevos/runtime_progress.c", "src/rcheevos/trigger.c", "src/rcheevos/value.c",
        "src/rhash/cdreader.c", "src/rhash/hash.c", "src/rhash/hash_disc.c", "src/rhash/md5.c",
    ];
    let mut build = cc::Build::new();
    // Each compiler's default C (GNU C on GCC and Clang): rcheevos uses
    // POSIX's strdup and strcasecmp, which strict ISO C modes hide.
    build
        .include(root.join("include"))
        .define("RC_NO_THREADS", None)
        // A PlayStation disc's hash only: no cartridge, archive or
        // encrypted image formats.
        .define("RC_HASH_NO_ROM", None)
        .define("RC_HASH_NO_ZIP", None)
        .define("RC_HASH_NO_ENCRYPTED", None)
        // Its own sources' warnings are upstream's.
        .warnings(false)
        .file("src/glue.c");
    for file in files {
        build.file(root.join(file));
    }
    build.compile("rcheevos");
    println!("cargo:rerun-if-changed=rcheevos");
    println!("cargo:rerun-if-changed=src/glue.c");
}
