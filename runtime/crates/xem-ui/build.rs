fn main() {
    // Debug info keeps element names in the generated code so tests and agents
    // can inspect the element tree.
    let config = slint_build::CompilerConfiguration::new()
        .with_style("fluent-dark".into())
        .with_debug_info(true);
    slint_build::compile_with_config("ui/settings.slint", config).expect("settings panel compiles");
}
