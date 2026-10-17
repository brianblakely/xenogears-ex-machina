# Only the staged executables and notices enter the store. Never copy the
# repository root: it can contain the user's discs and saves.
{ binaryDirectory, version }:
let
  tools = builtins.getFlake ("path:" + toString ../nix/runtime);
  system = builtins.currentSystem;
  pkgs = import tools.inputs.nixpkgs { inherit system; };
  libraries = with pkgs; [
    glibc
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
        executable=xem-desktop
        if [ "''${1:-}" = --headless ]; then executable=xem-headless; shift; fi
        if [ "''${1:-}" = --licenses ]; then cat @out@/share/xem/THIRD_PARTY_NOTICES.txt; exit; fi
        export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath libraries}:''${LD_LIBRARY_PATH:-}:/usr/lib64:/usr/lib:/usr/lib/x86_64-linux-gnu:/usr/lib/aarch64-linux-gnu:/run/opengl-driver/lib
        exec @out@/libexec/$executable "$@"
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
  runtimeComponents = map toString [
    application
    tools.inputs.appimage.packages.${system}.appimage-runtimes.appimage-type2-runtime
    tools.inputs.appimage.packages.${system}.appimage-appruns.userns-chroot
  ];
  sourceRoots = [
    tools.inputs.nixpkgs.outPath
    tools.inputs.appimage.outPath
  ];
}
