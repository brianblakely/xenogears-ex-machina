# Only the staged executables and notices enter the store. Never copy the
# repository root: it can contain the user's discs and saves.
{ binaryDirectory, version }:
let
  tools = builtins.getFlake ("path:" + toString ../nix/runtime);
  system = builtins.currentSystem;
  pkgs = import tools.inputs.nixpkgs { inherit system; };
  imageRuntime = tools.inputs.appimage.packages.${system}.appimage-runtimes.appimage-type2-runtime;
  imageLauncher = tools.inputs.appimage.packages.${system}.appimage-appruns.userns-chroot;
  fonts = pkgs.makeFontsConf { fontDirectories = [ pkgs.dejavu_fonts ]; };
  libraries = with pkgs; [
    glibc
    mesa
    sdl3
    vulkan-loader
    libGL
    wayland
    libxkbcommon
    libx11
    libxext
    libxcursor
    libxi
    libxrandr
    fontconfig
    freetype
    openxr-loader
    stdenv.cc.cc.lib
  ];
  staged = builtins.path {
    path = builtins.toPath binaryDirectory;
    name = "xem-release-input";
  };
  application =
    pkgs.runCommand "xem-${version}"
      {
        nativeBuildInputs = [
          pkgs.makeWrapper
          pkgs.patchelf
          pkgs.binutils
        ];
        buildInputs = libraries;
      }
      ''
        mkdir -p $out/libexec $out/bin $out/share/xem $out/share/applications
        cp ${staged}/xem-desktop ${staged}/xem-headless $out/libexec/
        chmod u+w $out/libexec/*
        for binary in $out/libexec/*; do
          strip --strip-debug "$binary"
          patchelf --set-interpreter ${pkgs.stdenv.cc.bintools.dynamicLinker} "$binary"
          patchelf --set-rpath ${pkgs.lib.makeLibraryPath libraries} "$binary"
        done
        cp ${staged}/*.txt ${staged}/LICENSE $out/share/xem/
        cat >$out/bin/xem <<'SCRIPT'
        #!${pkgs.runtimeShell}
        set -euo pipefail
        executable=xem-desktop
        if [ "''${1:-}" = --headless ]; then executable=xem-headless; shift; fi
        if [ "''${1:-}" = --licenses ]; then ${pkgs.coreutils}/bin/cat @out@/share/xem/THIRD_PARTY_NOTICES.txt; exit; fi
        # Mixing the host's ALSA/Wayland/etc. into a bundled glibc process can
        # require newer GLIBC symbols. Mesa stays with this library closure.
        # Only proprietary NVIDIA driver libraries come from the host (the
        # same isolation used by scripts/xem-gpu-host during development).
        shopt -s nullglob
        drivers=$(${pkgs.coreutils}/bin/mktemp -d "''${TMPDIR:-/tmp}/xem-drivers.XXXXXXXX")
        trap '${pkgs.coreutils}/bin/rm -rf "$drivers"' EXIT
        for directory in /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /run/opengl-driver/lib; do
          for library in "$directory"/libGLX_nvidia.so* "$directory"/libEGL_nvidia.so* "$directory"/libnvidia-*.so*; do
            ${pkgs.coreutils}/bin/ln -sf "$library" "$drivers/"
          done
        done
        export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath libraries}:$drivers''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
        icds=(${pkgs.mesa}/share/vulkan/icd.d/*.json)
        export VK_ADD_DRIVER_FILES=$(IFS=:; echo "''${icds[*]}")''${VK_ADD_DRIVER_FILES:+:$VK_ADD_DRIVER_FILES}
        export __EGL_VENDOR_LIBRARY_DIRS=${pkgs.mesa}/share/glvnd/egl_vendor.d:''${__EGL_VENDOR_LIBRARY_DIRS:-/usr/share/glvnd/egl_vendor.d}
        export FONTCONFIG_FILE=${fonts}
        @out@/libexec/$executable "$@"
        SCRIPT
        substituteInPlace $out/bin/xem --replace-fail '@out@' "$out"
        chmod +x $out/bin/xem
        cat >$out/share/applications/xem.desktop <<EOF
        [Desktop Entry]
        Type=Application
        Name=Xenogears Ex Machina
        Exec=xem
        Categories=Game;
        Terminal=false
        EOF
      '';
  image = tools.inputs.appimage.lib.${system}.mkAppImage {
    program = "${application}/bin/xem";
    name = "xenogears-ex-machina-${version}-linux-${pkgs.stdenv.hostPlatform.uname.processor}.AppImage";
    # Nix already supplies SOURCE_DATE_EPOCH; explicit time flags conflict with it.
  };
in
image
// {
  # Static launcher dependencies do not appear in the ELF runtime closure.
  runtimeComponents = map toString (
    [
      application
      imageRuntime
      imageLauncher
      pkgs.pkgsStatic.stdenv.cc.libc
      pkgs.pkgsStatic.stdenv.cc.cc.lib
    ]
    ++ imageRuntime.buildInputs
  );
  sourceRoots = [
    tools.inputs.nixpkgs.outPath
    tools.inputs.appimage.outPath
  ];
}
