{
  description = "Pinned development tools for independent Xenogears analysis and native C++ development";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          traceCore = pkgs.libretro.pcsx-rearmed.overrideAttrs (previous: {
            postPatch = (previous.postPatch or "") + ''
              cp ${./reference-trace.h} libpcsxcore/xem_reference_trace.h
              ${pkgs.python3}/bin/python3 ${./reference-trace-patch.py}
            '';
          });
          base = pkgs.mkShell {
            packages = with pkgs; [
              clang
              clang-tools
              cmake
              ninja
              python3
              ruff
              nixfmt
              git
              pkg-config
            ];
            hardeningDisable = [ "fortify" ];
            shellHook = ''
              export CC=clang
              export CXX=clang++
              export PYTHONDONTWRITEBYTECODE=1
              export SOURCE_DATE_EPOCH=0
            '';
          };
        in
        {
          default = base;
          analysis = base.overrideAttrs (previous: {
            nativeBuildInputs = previous.nativeBuildInputs ++ [ pkgs.mame-tools ];
          });
          disassembly = base.overrideAttrs (previous: {
            nativeBuildInputs =
              builtins.filter (package: package != pkgs.python3) previous.nativeBuildInputs
              ++ [
                (pkgs.python3.withPackages (python: [ python.capstone ]))
              ];
          });
          observation = base.overrideAttrs (_: {
            XEM_REFERENCE_CORE = "${pkgs.libretro.pcsx-rearmed}/lib/retroarch/cores/pcsx_rearmed_libretro.so";
          });
          observation-trace = base.overrideAttrs (_: {
            XEM_REFERENCE_CORE = "${traceCore}/lib/retroarch/cores/pcsx_rearmed_libretro.so";
          });
        }
      );
    };
}
