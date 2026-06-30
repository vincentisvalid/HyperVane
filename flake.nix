{
  description = "HyperVane — emulation frontend dev environment";

  inputs = {
    nixpkgs.url     = "github:NixOS/nixpkgs/nixos-unstable";
    rust-overlay.url = "github:oxalica/rust-overlay";
    flake-utils.url  = "github:numtide/flake-utils";

    rust-overlay.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, rust-overlay, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [ (import rust-overlay) ];
        pkgs     = import nixpkgs { inherit system overlays; };

        # Pin to the project's MSRV
        rustToolchain = pkgs.rust-bin.stable."1.75.0".default.override {
          extensions = [ "rust-src" "rust-analyzer" ];
        };

        pythonEnv = pkgs.python312.withPackages (ps: with ps; [
          yt-dlp
          av          # PyAV — FFmpeg bindings
          aiohttp
          aiofiles
          protobuf
        ]);

        # Qt6 packages needed by the UI
        qt6Pkgs = with pkgs.qt6; [
          qtbase
          qtmultimedia
          qtwayland    # Wayland backend (remove if X11-only)
          wrapQtAppsHook
        ];

      in {
        devShells.default = pkgs.mkShell {
          name = "hypervane";

          nativeBuildInputs = with pkgs; [
            # C++ / Qt6
            cmake
            ninja
            pkg-config
            protobuf   # provides protoc

            # Go
            go
            protoc-gen-go

            # Qt6 build hooks
            pkgs.qt6.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            # C++ / Qt6
            pkgs.qt6.qtbase
            pkgs.qt6.qtmultimedia
            pkgs.qt6.qtwayland
            protobuf

            # Python
            pythonEnv
            ffmpeg

            # Rust
            rustToolchain

            # Fonts (needed by gen_test_video.sh via fc-match / drawtext)
            dejavu_fonts
            fontconfig

            # Runtime libs for Qt
            libGL
            libxkbcommon
            wayland
            libx11
            libxext
          ];

          shellHook = ''
            # Let CMake find Qt6
            export CMAKE_PREFIX_PATH="${pkgs.qt6.qtbase.dev}:${pkgs.qt6.qtmultimedia.dev}:$CMAKE_PREFIX_PATH"

            # Rust: use project-local target dir so ~/cargo stays clean
            export CARGO_TARGET_DIR="$(pwd)/src/db_engine/target"

            # Go: keep module cache inside the project tree if desired
            export GOPATH="$(pwd)/.gopath"
            export PATH="$GOPATH/bin:$PATH"

            # Qt Wayland/X11 fallback
            export QT_QPA_PLATFORM=''${QT_QPA_PLATFORM:-xcb}

            echo "HyperVane dev shell ready."
            echo "  Run: bash scripts/dev_setup.sh   (first time only)"
            echo "  Run: bash scripts/build_all.sh"
          '';
        };
      }
    );
}
