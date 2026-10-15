{
  description = "Pinned Phase 2 runtime tools: Rust, wasm32 game module, SDL3/wgpu/Slint, OpenXR, WebXR and Android";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.rust-overlay.url = "github:oxalica/rust-overlay";
  inputs.rust-overlay.inputs.nixpkgs.follows = "nixpkgs";

  outputs =
    { nixpkgs, rust-overlay, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs {
            inherit system;
            overlays = [ rust-overlay.overlays.default ];
          };
          # Only the android shell accepts the Android SDK license (unfree).
          androidPkgs = import nixpkgs {
            inherit system;
            config.allowUnfree = true;
            config.android_sdk.accept_license = true;
          };
          rust = pkgs.rust-bin.stable."1.97.1".minimal.override {
            extensions = [
              "clippy"
              "rustfmt"
              "rust-src"
            ];
            targets = [
              "wasm32-unknown-unknown"
              "aarch64-linux-android"
              "x86_64-linux-android"
            ];
          };
          llvm = pkgs.llvmPackages_21;
          # Libraries the SDL3, wgpu (Vulkan/GL), Slint and OpenXR hosts open at run time.
          runtimeLibraries = with pkgs; [
            sdl3
            vulkan-loader
            libGL
            wayland
            libxkbcommon
            libx11
            libxcursor
            libxi
            libxrandr
            fontconfig
            freetype
            openxr-loader
          ];
          # FreeSurround (Christian Kothe, GPL-2.0+) as vendored by Dolphin, wrapped in a
          # stereo -> 5.1 filter for testing the Wide sound mode. A test tool only:
          # nothing in the runtime links it.
          freesurround = pkgs.stdenv.mkDerivation {
            pname = "xem-freesurround";
            version = "0-dolphin-d2833fb";
            src = pkgs.fetchFromGitHub {
              owner = "dolphin-emu";
              repo = "dolphin";
              rev = "d2833fb78882715950ab0a2c26c3ca20e3f6e349";
              sparseCheckout = [ "Externals/FreeSurround" ];
              hash = "sha256-0A1dw/59JAtw6Pqlta59iNiGk7f0If6hV5r+rJkt5ks=";
            };
            buildPhase = ''
              $CXX -O2 -std=c++14 -w -IExternals/FreeSurround/include \
                Externals/FreeSurround/source/*.cpp ${./freesurround.cpp} \
                -o freesurround
            '';
            installPhase = "install -Dm755 freesurround $out/bin/freesurround";
            meta.license = pkgs.lib.licenses.gpl2Plus;
          };
          base = pkgs.mkShell {
            packages =
              (with pkgs; [
                rust
                llvm.clang-unwrapped
                llvm.lld
                llvm.llvm
                wabt
                binaryen
                wasm-bindgen-cli
                nodejs_24
                chromium
                (python3.withPackages (python: [ python.pyelftools ]))
                ruff
                nixfmt
                git
                gnumake
                cmake
                ninja
                pkg-config
                vulkan-tools
                # Sampling profiles of the native hosts and examples (user space:
                # perf record --all-user --call-graph dwarf).
                perf
                mesa
                monado
                openxr-loader
                xvfb-run
                weston
                freesurround
              ])
              ++ runtimeLibraries;
            shellHook = ''
              export PYTHONDONTWRITEBYTECODE=1
              export SOURCE_DATE_EPOCH=0
              export XEM_CLANG=${llvm.clang-unwrapped}/bin/clang
              export XEM_WASM_LD=${llvm.lld}/bin/wasm-ld
              export XEM_WASM2C_RUNTIME=${pkgs.wabt}/share/wabt/wasm2c
              export XEM_WASM2C_INCLUDE=${pkgs.wabt}/include
              export XEM_CHROMIUM=${pkgs.chromium}/bin/chromium
              # Software Vulkan (lavapipe) for tests; xem-gpu-host switches to the host driver.
              export XEM_LAVAPIPE_ICD=${pkgs.mesa}/share/vulkan/icd.d/lvp_icd.${pkgs.stdenv.hostPlatform.uname.processor}.json
              export XEM_MONADO_RUNTIME=${pkgs.monado}/share/openxr/1/openxr_monado.json
              export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath runtimeLibraries}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
              export PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1
            '';
          };
          android = androidPkgs.androidenv.composeAndroidPackages {
            platformVersions = [ "34" ];
            buildToolsVersions = [ "34.0.0" ];
            includeNDK = true;
            ndkVersions = [ "29.0.14206865" ];
            includeEmulator = true;
            includeSystemImages = true;
            systemImageTypes = [ "google_apis" ];
            abiVersions = [ "x86_64" ];
          };
        in
        {
          default = base;
          android = base.overrideAttrs (previous: {
            nativeBuildInputs = previous.nativeBuildInputs ++ [
              android.androidsdk
              androidPkgs.jdk17
              androidPkgs.gradle
              androidPkgs.cargo-ndk
              # qemu-aarch64 runs xem-headless's arm64 build (scripts/arm64-smoke.sh).
              androidPkgs.qemu-user
            ];
            shellHook = previous.shellHook + ''
              export ANDROID_HOME=${android.androidsdk}/libexec/android-sdk
              export ANDROID_SDK_ROOT=$ANDROID_HOME
              export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/29.0.14206865
              export JAVA_HOME=${androidPkgs.jdk17.home}
              export GRADLE_OPTS="-Dorg.gradle.project.android.aapt2FromMavenOverride=$ANDROID_HOME/build-tools/34.0.0/aapt2"
              # The desktop's SDL release: the APK builds libSDL3.so and SDLActivity from it.
              export XEM_SDL3_SRC=${pkgs.sdl3.src}
            '';
          });
        }
      );
    };
}
