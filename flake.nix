{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
        flake-utils.url = "github:numtide/flake-utils";
        devkitNix.url = "github:bandithedoge/devkitNix";
    };

    outputs = { self, nixpkgs, flake-utils, devkitNix, ... }: flake-utils.lib.eachDefaultSystem (system: let
        pkgs = import nixpkgs {
            inherit system;
            overlays = [ devkitNix.overlays.default ];
        };
        
        in {
            devShells.default = pkgs.mkShell.override {
                stdenv = pkgs.devkitNix.stdenvARM;
            } {
                nativeBuildInputs = with pkgs; [
                    clang
                    gcc
                    glibc
                    gnumake
                    ninja
                    cmake
                ];

                buildInputs = with pkgs; [
                    clang
                    gcc
                    glibc
                    gnumake
                    ninja
                    cmake
                ];
            };

            packages.default = pkgs.devkitNix.stdenvARM.mkDerivation {
                name = "devkitARM-debug";
                src = ./.;

                makeFlags = [ "TARGET=debug" ];
                installPhase = ''
                    mkdir $out
                    cp example.nds $out
                '';
            };
        }
    );
}