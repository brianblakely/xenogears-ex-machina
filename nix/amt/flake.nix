{
  description = "Pinned Octave and the Auditory Modeling Toolbox (AMT 1.6.0, GPL-3.0+) with the data its localization models need, for testing the Binaural sound mode with computational listeners; a local test tool only: nothing in the runtime uses it";

  # The runtime shell's nixpkgs (nix/runtime/flake.lock).
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/dc5d91f840324650bac8c379428c7037a416959a";

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      site = "https://www.amtoolbox.org";
      # The baumgartner2014 listener pool: 23 listeners' own directional
      # transfer functions (ARI, SOFA), as data_baumgartner2014 loads them.
      pool = {
        NH12 = "sha256-mXKyxzRvR3Ge8tJlPF9xsLJRKzDLOoA7gBzMbTyHE6k=";
        NH14 = "sha256-N1FGG0WIvoGQnqlI4k0ld3gTcphecG//3iNGw+o9Cyg=";
        NH15 = "sha256-ThIUs4+gjwKXpVmvhxlav7y9ObdHnsFQZoJUddspDnw=";
        NH16 = "sha256-K46NzbbjjXBZGTKyY60DqwwqKNOCrcX8uyIhbha4dag=";
        NH17 = "sha256-NTOEIUujTvQKink0pFDpxEFnnLiVXcoj0fXNLh7wpNA=";
        NH18 = "sha256-6NZCmTjLWar9SSw7Tx99zRjrFtg8Cm4OYYM4Uuz+zlE=";
        NH21 = "sha256-5u+kcoSgVodx2yxxSSprzb74MbIAnksZs3cq8E55708=";
        NH22 = "sha256-2/K6UHXRb1khR/jF6i0lFvYzfk+DqfsnlaGdilpTS5c=";
        NH33 = "sha256-4VO5adUxLbgtIOka4bKE7IFpDjG55kY1uzindkpQrRg=";
        NH39 = "sha256-l26jFRBDKEQwZiYN4QyzpdFdYFe788DjvVSbEocyi9c=";
        NH41 = "sha256-E/zcHeDKJdI9RSCNMP76aS9PJ8Qw98OWX1Tsm6iUJ40=";
        NH42 = "sha256-69OumYU2v+v1tWHxUWa2PKDYwnnnsuWvfE+CjhnJl5c=";
        NH43 = "sha256-yOInv7ReZayuxUD9p0aitVZzLyrK3a8DHYH/7lVW/n4=";
        NH46 = "sha256-WglBiiq9EMhjT1xTXt8fsaiFwn2GnhUWQPaxTevbVlA=";
        NH53 = "sha256-9b5tY7IeRmWtVbuhiLsud/nA7lULPYqrUHBoM3M2o28=";
        NH55 = "sha256-GXCu8hIwlmW2dcNc6bmm9I1tMHPdqEwoVumcBwteuG4=";
        NH57 = "sha256-XRy7RdgHYWGheoKKqnrf8uAISb3mks0NctEIM8RSDVU=";
        NH58 = "sha256-ZbEVe7b83liT2VIcr6aMzbds6HTtcaxE/XLDS4EW0mI=";
        NH62 = "sha256-QXTKLEIPu+mG89Cx3LP6CwPdu+Inzk0OCn0VN9H97bY=";
        NH64 = "sha256-/vOKf8Wln2A/WOGzHvfM6qg+5EbjibSkM1tWn0bsxIc=";
        NH68 = "sha256-N+OoR7XQI4TxrWBmLFIK2advL4PDkpkVZC6nrrQCcWA=";
        NH71 = "sha256-2KQCOALTKKWtzo18IQhVUozM2yJNrTGlezhMzFUcl9M=";
        NH72 = "sha256-kRaKu/B814F0+TSyln+VT6EaiufoAvEoNE4D6JDSTk8=";
      };
      # The pool's localization experiments (its actual responses, from which
      # data_baumgartner2014('baseline') takes the published errors).
      experiments = {
        goupell2010 = "sha256-a/5J0ieRle7Jem6oSssPPDayVWtC9lDaU/D8uD2k1RM=";
        majdak2010 = "sha256-098r2EtWW35Vgz3UDSHHCQtwcURzv1TpIYbIs4CCzuY=";
        majdak2013 = "sha256-i5pVMVhNGj8p6yV3iL4A+lciZ29egVFY7d97/2aZ6z8=";
        majdak2013ctc = "sha256-2FyzIG2x/W3kVuhGsckDcAPXUawbwkrJGGckSxctJSQ=";
      };
    in
    {
      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          lib = pkgs.lib;
          octave = pkgs.octave.withPackages (
            packages: with packages; [
              io
              netcdf
              signal
              statistics
            ]
          );
          # What AMT's amt_load and amt_cache would download on first use,
          # from the first release folder holding each file, placed where
          # they look so the models run offline: the auxiliary data and the
          # pool's fitted sensitivities (data_baumgartner2014's cached
          # calibration, gamma 6, motor scatter 17, gradient order 1).
          inputs =
            lib.mapAttrsToList (id: hash: {
              path = "auxdata/baumgartner2014/ARI_${id}_hrtf_M_dtf 256.sofa";
              file = pkgs.fetchurl {
                name = "amt-baumgartner2014-ARI_${id}_hrtf_M_dtf_256.sofa";
                url = "${site}/amt-1.4.0/auxdata/baumgartner2014/ARI_${id}_hrtf_M_dtf%20256.sofa";
                inherit hash;
              };
            }) pool
            ++ lib.mapAttrsToList (model: hash: {
              path = "auxdata/${model}/data.mat";
              file = pkgs.fetchurl {
                name = "amt-${model}-data.mat";
                url = "${site}/amt-1.0.0/auxdata/${model}/data.mat";
                inherit hash;
              };
            }) experiments
            ++ [
              {
                path = "cache/data%2Fdata_baumgartner2014.m/calibration_g6_mrs17_do1.mat";
                file = pkgs.fetchurl {
                  name = "amt-cache-data_baumgartner2014-calibration_g6_mrs17_do1.mat";
                  url = "${site}/amt-1.4.0/cache/data%252Fdata_baumgartner2014.m/calibration_g6_mrs17_do1.mat";
                  hash = "sha256-LVbnfbyLllT0P5szNL+qZdLnPOdAedxQlGCaFPKtQbw=";
                };
              }
            ];
          # The full package: AMT with the third-party toolboxes it starts
          # (LTFAT 2.6.0, the SOFA Toolbox 2.2.0, SFS 2.5.0, Circular
          # Statistics, BinauralSH, BADS).
          amt = pkgs.stdenvNoCC.mkDerivation {
            pname = "amtoolbox-full";
            version = "1.6.0";
            src = pkgs.fetchurl {
              url = "mirror://sourceforge/amtoolbox/AMT%201.x/amtoolbox-full-1.6.0.zip";
              hash = "sha256-fTULRC+Y9Pb6Y0n9OPjlMSP3cDnYlWXK7friHOJqMTU=";
            };
            nativeBuildInputs = [ pkgs.unzip ];
            sourceRoot = "amtoolbox-1.6.0";
            # The SOFA Toolbox compiles a convention again unless its compiled
            # files are newer than its table; the package ships them
            # compiled, and the store gives every file the same time, so
            # the same time counts as compiled (nothing is written there).
            postPatch = ''
              substituteInPlace thirdparty/sofa/SOFAtoolbox/SOFAcompileConventions.m \
                --replace-fail "flagFile(1).datenum>file.datenum" "flagFile(1).datenum>=file.datenum"
            '';
            installPhase = ''
              mkdir -p $out/share
              cp -r . $out/share/amtoolbox
              ${lib.concatMapStrings (input: ''
                install -Dm644 ${input.file} "$out/share/amtoolbox/${input.path}"
              '') inputs}
              # amt_start deletes SFS's rms.m under Octave, whose own it
              # shadows; the store is read-only, so it goes here.
              rm $out/share/amtoolbox/thirdparty/sfs/sfs-matlab-2.5.0/SFS_octave/rms.m
            '';
            meta.license = lib.licenses.gpl3Plus;
          };
        in
        {
          default = pkgs.mkShell {
            packages = [ octave ];
            shellHook = ''
              export XEM_AMT=${amt}/share/amtoolbox
            '';
          };
        }
      );
    };
}
