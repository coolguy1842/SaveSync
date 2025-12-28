{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
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
                buildInputs = with pkgs; [
                    clang-tools
                    gcc
                    glibc
                    gnumake
                    ninja
                    cmake
                    act
                ];
            };

            packages.default = let    
                manifest = (pkgs.lib.importJSON ./manifest.json);  
            in pkgs.devkitNix.stdenvARM.mkDerivation {
                name = manifest.name;
                version = manifest.version;
                src = ./.;

                buildInputs = with pkgs; [ cmake ];
                configurePhase = ''
                    cmake -DCMAKE_BUILD_TYPE=Release -S $src -B ./build
                '';
                
                buildPhase = ''
                    cmake --build ./build
                '';

                installPhase = ''
                    mkdir $out
                    cp ./build/SaveSync.3dsx $out
                    [[ -e ./build/SaveSync.cia ]] && cp ./build/SaveSync.cia $out
                '';

                meta = with pkgs.lib; {
                    description = "Local Backup/Sync Homebrew Program";
                    homepage = "https://github.com/coolguy1842/SaveSync/";
                    license = licenses.gpl3;
                };
            };
        }
    );
}