{
  description = "Pinned Ghidra and PlayStation loader for independent original-source analysis";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/dc5d91f840324650bac8c379428c7037a416959a";

  outputs =
    { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      psxSource = pkgs.fetchzip {
        extension = "tar.gz";
        url = "https://codeload.github.com/lab313ru/ghidra_psx_ldr/tar.gz/6f6be18615d9b42b1ce07740780623c3cf6cbd2c";
        hash = "sha256-gTKxn8i143phvNzrRillrRvw49Odlnk+KSWYtjfY9Cw=";
      };
      psyqSource = pkgs.fetchzip {
        extension = "tar.gz";
        url = "https://codeload.github.com/lab313ru/psx_psyq_signatures/tar.gz/e9e46e7e133ef275a79bfce650924f98edb086bc";
        hash = "sha256-AG9H24r3xSC7R3DDO8OtMKoNhDisNL1JqLSUTxwadXY=";
      };
      extensionBuilder = pkgs.callPackage "${nixpkgs}/pkgs/tools/security/ghidra/build-extension.nix" {
        ghidra = pkgs.ghidra;
        jdk = pkgs.jdk21;
        gradle = pkgs.gradle_8;
      };
      psxLoader = extensionBuilder.buildGhidraExtension {
        pname = "ghidra_psx_ldr";
        version = "2026-09-03-6f6be186";
        src = psxSource;
        postPatch = ''
          mkdir -p data/psyq
          cp -R ${psyqSource}/. data/psyq/
          rm data/languages/mips32le.sla
          analyzer=src/main/java/ghidra/app/plugin/core/analysis
          mv "$analyzer/MipsPreAnalyzer.java" "$analyzer/PsxMipsPreAnalyzer.java"
          substituteInPlace "$analyzer/PsxMipsPreAnalyzer.java" \
            --replace-fail MipsPreAnalyzer PsxMipsPreAnalyzer
        '';
        preBuild = ''
          ${pkgs.ghidra}/lib/ghidra/support/sleigh data/languages/mips32le.slaspec
        '';
        meta = {
          description = "lab313ru PlayStation executable loader and PSX analysis extension";
          homepage = "https://github.com/lab313ru/ghidra_psx_ldr";
          platforms = [ system ];
        };
      };
      ghidra = pkgs.ghidra.withExtensions (_: [ psxLoader ]);
      m2cSource = pkgs.fetchzip {
        extension = "tar.gz";
        url = "https://codeload.github.com/matt-kempster/m2c/tar.gz/1d1c4454a445326541305f83f2b0cb680a9ecb2d";
        hash = "sha256-XsWi6CQQruRwbHpFaWkNiO9s4q0nuUIvbUjwBa1izAM=";
      };
      m2c = pkgs.python3Packages.buildPythonApplication {
        pname = "m2c";
        version = "0.1.0-1d1c4454";
        src = m2cSource;
        pyproject = true;
        build-system = [ pkgs.python3Packages.poetry-core ];
        dependencies = [ pkgs.python3Packages.graphviz ];
        # Nix's 0.21 includes Python 3.14 fixes. Verify m2c's upstream suite
        # against this pinned compatibility override instead of fetching at runtime.
        pythonRelaxDeps = [ "graphviz" ];
        nativeCheckInputs = [ pkgs.python3Packages.coverage ];
        checkPhase = ''
          runHook preCheck
          python run_tests.py -j 4
          runHook postCheck
        '';
        pythonImportsCheck = [ "m2c.main" ];
        meta = {
          description = "Optional matching-oriented MIPS reconstruction aid";
          homepage = "https://github.com/matt-kempster/m2c";
          license = pkgs.lib.licenses.gpl3Only;
          mainProgram = "m2c";
          platforms = [ system ];
        };
      };
      base = pkgs.mkShell {
        packages = [
          ghidra
          (pkgs.python3.withPackages (python: [ python.capstone ]))
          pkgs.git
        ];
        XEM_GHIDRA_INSTALL_DIR = "${pkgs.ghidra}/lib/ghidra";
        XEM_PSX_LOADER_DIR = "${psxLoader}/lib/ghidra/Ghidra/Extensions/ghidra_psx_ldr";
        shellHook = ''
          export PYTHONDONTWRITEBYTECODE=1
          export SOURCE_DATE_EPOCH=0
        '';
      };
    in
    {
      packages.${system} = {
        default = ghidra;
        inherit ghidra m2c;
        psx-loader = psxLoader;
      };
      devShells.${system} = {
        default = base;
        matching = base.overrideAttrs (previous: {
          nativeBuildInputs = previous.nativeBuildInputs ++ [ m2c ];
        });
      };
    };
}
