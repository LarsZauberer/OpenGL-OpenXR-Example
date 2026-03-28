{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells.default = pkgs.mkShell {
          name = "devShell";

          buildInputs = [
            pkgs.clang-tools
            pkgs.clang
            pkgs.cmake

            pkgs.glfw
            pkgs.glib
            pkgs.openxr-loader
            pkgs.pkg-config
          ];

          shellHook = '''';
        };
      }
    );
}
