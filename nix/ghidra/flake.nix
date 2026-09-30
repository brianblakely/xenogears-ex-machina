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
      rabbitizer = pkgs.python3Packages.buildPythonPackage {
        pname = "rabbitizer";
        version = "1.16.2";
        src = pkgs.fetchPypi {
          pname = "rabbitizer";
          version = "1.16.2";
          hash = "sha256-KbYkVzu1fzKH60qI8Y5dr1YW8isZIw9sdaeuMoPN0Eg=";
        };
        pyproject = true;
        build-system = with pkgs.python3Packages; [
          setuptools
          wheel
        ];
        pythonImportsCheck = [ "rabbitizer" ];
        meta = {
          description = "MIPS instruction decoder used by the optional disassembler";
          homepage = "https://github.com/Decompollaborate/rabbitizer";
          license = pkgs.lib.licenses.mit;
          platforms = [ system ];
        };
      };
      spimdisasm = pkgs.python3Packages.buildPythonApplication {
        pname = "spimdisasm";
        version = "1.42.4";
        src = pkgs.fetchPypi {
          pname = "spimdisasm";
          version = "1.42.4";
          hash = "sha256-CiyNtUYVImKIt/bIbtzIRKZU35YyFXL6i441Zn4bkD8=";
        };
        pyproject = true;
        build-system = with pkgs.python3Packages; [
          setuptools
          wheel
          twine
        ];
        dependencies = [ rabbitizer ];
        pythonImportsCheck = [ "spimdisasm" ];
        meta = {
          description = "Optional original MIPS assembly preparation for m2c";
          homepage = "https://github.com/Decompollaborate/spimdisasm";
          license = pkgs.lib.licenses.mit;
          mainProgram = "spimdisasm";
          platforms = [ system ];
        };
      };
      psxBinutils =
        let
          cross = pkgs.pkgsCross.mipsel-linux-gnu;
          tools = cross.buildPackages.binutils-unwrapped;
          prefix = "${cross.stdenv.hostPlatform.config}-";
        in
        pkgs.runCommand "xem-psx-binutils" { } ''
          mkdir -p "$out/bin"
          for tool in as ld objcopy objdump readelf nm size; do
            test -x "${tools}/bin/${prefix}$tool"
            ln -s "${tools}/bin/${prefix}$tool" "$out/bin/psx-$tool"
          done
        '';
      # decompals/old-gcc 0.17 static i386 cpp/cc1 builds (GCC sources with the
      # PSX target patch). Each occurs in the original program, per translation
      # unit: 2.7.2 fills the epilogue's jr delay slot with the stack adjustment,
      # 2.6.3 never does, and the later battle modules come from the Cygnus CDK
      # build of 2.7.2 (cdk-gcc b18), which keeps a symbol's %hi in a register
      # (docs/matching.md). Commands are psx-cpp-/psx-cc1-<version>[-cdk].
      oldGcc =
        version: variant: hash:
        let
          name = if variant == "psx" then version else "${version}-${variant}";
        in
        pkgs.stdenvNoCC.mkDerivation {
          pname = "psx-gcc";
          version = "${version}-${variant}-old-gcc-0.17";
          src = pkgs.fetchurl {
            url = "https://github.com/decompals/old-gcc/releases/download/0.17/gcc-${version}-${variant}.tar.gz";
            inherit hash;
          };
          sourceRoot = ".";
          dontBuild = true;
          installPhase = ''
            mkdir -p "$out/lib/psx-gcc-${name}" "$out/bin"
            install -m755 cpp cc1 "$out/lib/psx-gcc-${name}/"
            ln -s "$out/lib/psx-gcc-${name}/cpp" "$out/bin/psx-cpp-${name}"
            ln -s "$out/lib/psx-gcc-${name}/cc1" "$out/bin/psx-cc1-${name}"
          '';
          meta = {
            description = "GCC ${name} cpp/cc1 for the PlayStation R3000 target";
            homepage = "https://github.com/decompals/old-gcc";
            license = pkgs.lib.licenses.gpl2Plus;
            platforms = [ system ];
          };
        };
      psxGcc272 = oldGcc "2.7.2" "psx" "sha256-UApFmzSF6IWo0wLKwjwqRjLzkA4DoJFT9hkGmf1yNXE=";
      psxGcc263 = oldGcc "2.6.3" "psx" "sha256-AeboxJM0FOo/jY47x2ah9fr9T8ARDgt10faRvXkZibE=";
      psxGcc272cdk = oldGcc "2.7.2" "cdk" "sha256-QrsN+W2xGptdLiPXi9yWJ5H0AEYoDT02Dak/5e728Ls=";
      maspsx = pkgs.stdenvNoCC.mkDerivation {
        pname = "maspsx";
        version = "2026-7686f845";
        src = pkgs.fetchzip {
          extension = "tar.gz";
          url = "https://codeload.github.com/mkst/maspsx/tar.gz/7686f845a181700534c83c0419183e38aeb3e49c";
          hash = "sha256-Q6NDNesXDj79mBeM24h+RhSZQG5fo4JuVPexQVSqnIQ=";
        };
        dontBuild = true;
        installPhase = ''
          mkdir -p "$out/lib/maspsx" "$out/bin"
          cp -R maspsx maspsx.py "$out/lib/maspsx/"
          printf '#!%s\nexec %s %s "$@"\n' "${pkgs.runtimeShell}" "${pkgs.python3}/bin/python3" \
            "$out/lib/maspsx/maspsx.py" > "$out/bin/maspsx"
          chmod +x "$out/bin/maspsx"
        '';
        meta = {
          description = "ASPSX-compatible preprocessing of GCC assembly for GNU as";
          homepage = "https://github.com/mkst/maspsx";
          license = pkgs.lib.licenses.mit;
          platforms = [ system ];
        };
      };
      pylibyaml = pkgs.python3Packages.buildPythonPackage {
        pname = "pylibyaml";
        version = "0.1.0";
        src = pkgs.fetchPypi {
          pname = "pylibyaml";
          version = "0.1.0";
          hash = "sha256-O1jeoGGQPARonjX6tj7BSffPXoLwgIvTQl+zqzlQYj4=";
        };
        pyproject = true;
        build-system = [ pkgs.python3Packages.setuptools ];
        dependencies = [ pkgs.python3Packages.pyyaml ];
        doCheck = false;
      };
      n64img = pkgs.python3Packages.buildPythonPackage {
        pname = "n64img";
        version = "0.3.3";
        src = pkgs.fetchPypi {
          pname = "n64img";
          version = "0.3.3";
          hash = "sha256-SIs3kW60qUMjHq9JzpBS99b5b3yQcS7zrqqu9J47vxI=";
        };
        pyproject = true;
        build-system = [ pkgs.python3Packages.setuptools ];
        dependencies = [ pkgs.python3Packages.pypng ];
        doCheck = false;
      };
      pygfxd = pkgs.python3Packages.buildPythonPackage {
        pname = "pygfxd";
        version = "1.0.5";
        src = pkgs.fetchPypi {
          pname = "pygfxd";
          version = "1.0.5";
          hash = "sha256-sx9fhi3MwuD6pgXMFs1joi/rMkTAeiBjA3Qh/NP2cUU=";
        };
        pyproject = true;
        build-system = [ pkgs.python3Packages.setuptools ];
        doCheck = false;
      };
      # splat imports its N64 codecs unconditionally; this is the upstream
      # manylinux abi3 wheel (the sdist needs a Rust/maturin build).
      crunch64 = pkgs.python3Packages.buildPythonPackage {
        pname = "crunch64";
        version = "0.6.2";
        format = "wheel";
        src = pkgs.fetchurl {
          url = "https://files.pythonhosted.org/packages/7a/e7/9788e5a4a1b86e905378a09534dc7a5d4a4ba8ad38b1aedfa3a645470086/crunch64-0.6.2-cp37-abi3-manylinux_2_17_x86_64.manylinux2014_x86_64.whl";
          hash = "sha256-t2TMUAyxyrm8xtVoPhrbzk64V6vzEhjTEa7X1Tv8lu4=";
        };
        nativeBuildInputs = [ pkgs.autoPatchelfHook ];
        buildInputs = [ pkgs.stdenv.cc.cc.lib ];
      };
      splat = pkgs.python3Packages.buildPythonApplication {
        pname = "splat64";
        version = "0.50.0";
        src = pkgs.fetchPypi {
          pname = "splat64";
          version = "0.50.0";
          hash = "sha256-9TvDo/7NG3oBNnUJvs51ScWOjLmEmASr2eDzD34Vywo=";
        };
        pyproject = true;
        build-system = [ pkgs.python3Packages.hatchling ];
        dependencies = with pkgs.python3Packages; [
          colorama
          intervaltree
          crunch64
          n64img
          pygfxd
          pylibyaml
          pyyaml
          tqdm
          (toPythonModule spimdisasm)
          rabbitizer
        ];
        pythonRelaxDeps = true;
        doCheck = false;
        pythonImportsCheck = [ "splat" ];
        meta = {
          description = "Binary splitting into linkable assembly/data/source segments";
          homepage = "https://github.com/ethteck/splat";
          license = pkgs.lib.licenses.mit;
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
        inherit
          ghidra
          m2c
          spimdisasm
          psxBinutils
          ;
        psx-loader = psxLoader;
      };
      devShells.${system} = {
        default = base;
        matching = pkgs.mkShell {
          packages = [
            m2c
            spimdisasm
            psxBinutils
            psxGcc272
            psxGcc263
            psxGcc272cdk
            maspsx
            splat
            pkgs.gnumake
            pkgs.diffutils
            pkgs.git
            pkgs.python3
          ];
          shellHook = ''
            export PYTHONDONTWRITEBYTECODE=1
            export SOURCE_DATE_EPOCH=0
          '';
        };
      };
    };
}
