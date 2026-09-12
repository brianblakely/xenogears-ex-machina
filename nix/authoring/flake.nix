{
  description = "Separate build-only TypeScript geometry dependency qualification";

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
    in
    {
      devShells = nixpkgs.lib.genAttrs systems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              nodejs_24
              python3
              ruff
              nixfmt
            ];
            shellHook = ''
              export PYTHONDONTWRITEBYTECODE=1
              export SOURCE_DATE_EPOCH=0
            '';
          };
        }
      );
    };
}
