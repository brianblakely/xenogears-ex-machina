//! sdl3-sys links `SDL3`; on Android the library is the one
//! `src/runtime/scripts/android-build.sh` builds from the desktop's SDL release
//! into `$XEM_ANDROID_SDL3_LIBS/<abi>/libSDL3.so`.

fn main() {
    println!("cargo::rustc-check-cfg=cfg(has_game_module)");
    // xem-game's build script sets DEP_XEM_GAME_* only when it compiled the module.
    if std::env::var_os("DEP_XEM_GAME_MODULE").is_some() {
        println!("cargo::rustc-cfg=has_game_module");
    }
    println!("cargo::rerun-if-env-changed=XEM_ANDROID_SDL3_LIBS");
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("android") {
        return;
    }
    let abi = match std::env::var("CARGO_CFG_TARGET_ARCH").unwrap().as_str() {
        "aarch64" => "arm64-v8a",
        "x86_64" => "x86_64",
        other => panic!("no Android ABI for {other}"),
    };
    let libs = std::env::var("XEM_ANDROID_SDL3_LIBS").expect(
        "XEM_ANDROID_SDL3_LIBS names the libSDL3.so build (src/runtime/scripts/android-build.sh)",
    );
    println!("cargo::rustc-link-search=native={libs}/{abi}");
}
