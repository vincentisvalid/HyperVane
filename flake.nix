{
  description = "HyperVane — emulation frontend";

  inputs = {
    nixpkgs.url      = "github:NixOS/nixpkgs/nixos-unstable";
    rust-overlay.url = "github:oxalica/rust-overlay";
    flake-utils.url  = "github:numtide/flake-utils";

    rust-overlay.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, rust-overlay, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [ (import rust-overlay) ];
        pkgs     = import nixpkgs { inherit system overlays; };

        rustToolchain = pkgs.rust-bin.stable."1.75.0".default.override {
          extensions = [ "rust-src" "rust-analyzer" ];
        };

        pythonEnv = pkgs.python312.withPackages (ps: with ps; [
          yt-dlp
          av
          aiohttp
          aiofiles
          protobuf
        ]);

        # ── Installable package ──────────────────────────────────────────────
        hypervane = pkgs.qt6.mkDerivation {
          pname   = "hypervane";
          version = "0.1.0";

          src = self;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            protobuf
            qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qtmultimedia
            qt6.qtsvg
            qt6.qtwayland
            protobuf
            abseil-cpp
          ];

          # CMakeLists.txt lives in src/ui/
          cmakeDir = "../src/ui";

          cmakeFlags = [
            "-DHYPERVANE_ASSETS_DIR=${placeholder "out"}/share/hypervane/assets"
          ];

          postInstall = ''
            # Assets
            mkdir -p $out/share/hypervane
            cp -r $src/assets $out/share/hypervane/assets

            # Desktop entry
            mkdir -p $out/share/applications
            cp $src/assets/hypervane.desktop $out/share/applications/hypervane.desktop
            substituteInPlace $out/share/applications/hypervane.desktop \
              --replace "Exec=hypervane" "Exec=$out/bin/hypervane-ui"

            # Icon
            mkdir -p $out/share/icons/hicolor/scalable/apps
            cp $src/assets/hypervane.svg \
               $out/share/icons/hicolor/scalable/apps/hypervane.svg
          '';

          meta = with pkgs.lib; {
            description = "Intelligent multi-system emulation frontend";
            homepage    = "https://github.com/vincentisvalid/HyperVane";
            license     = licenses.mit;
            platforms   = platforms.linux;
            mainProgram = "hypervane-ui";
          };
        };

      in {
        # ── `nix build` / `nix profile install` ─────────────────────────────
        packages.default  = hypervane;
        packages.hypervane = hypervane;

        # ── `nix run` ────────────────────────────────────────────────────────
        apps.default = {
          type    = "app";
          program = "${hypervane}/bin/hypervane-ui";
        };

        # ── Dev shell (`nix develop`) ─────────────────────────────────────────
        devShells.default = pkgs.mkShell {
          name = "hypervane";

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            protobuf
            go
            protoc-gen-go
            pkgs.qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            pkgs.qt6.qtbase
            pkgs.qt6.qtmultimedia
            pkgs.qt6.qtsvg
            pkgs.qt6.qtwayland
            protobuf
            abseil-cpp
            pythonEnv
            ffmpeg
            rustToolchain
            dejavu_fonts
            fontconfig
            libGL
            libxkbcommon
            wayland
            libx11
            libxext
          ];

          shellHook = ''
            export CMAKE_PREFIX_PATH="${pkgs.qt6.qtbase.dev}:${pkgs.qt6.qtmultimedia.dev}:${pkgs.qt6.qtsvg.dev}:$CMAKE_PREFIX_PATH"
            export CARGO_TARGET_DIR="$(pwd)/src/db_engine/target"
            export GOPATH="$(pwd)/.gopath"
            export PATH="$GOPATH/bin:$PATH"
            export QT_QPA_PLATFORM=''${QT_QPA_PLATFORM:-xcb}
            echo "HyperVane dev shell ready."
            echo "  Run: bash scripts/dev_setup.sh   (first time only)"
            echo "  Run: bash scripts/build_all.sh"
          '';
        };
      }
    );
}
