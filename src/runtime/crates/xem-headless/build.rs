fn main() {
    println!("cargo::rustc-check-cfg=cfg(has_game_module)");
    // xem-game's build script sets DEP_XEM_GAME_* only when it compiled the module.
    if std::env::var_os("DEP_XEM_GAME_MODULE").is_some() {
        println!("cargo::rustc-cfg=has_game_module");
    }
}
