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
        hypervane = pkgs.stdenv.mkDerivation {
          pname   = "hypervane";
          version = "0.1.0";

          src = self;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            protobuf
            qt6.wrapQtAppsHook   # wraps binary with correct Qt env vars
          ];

          buildInputs = with pkgs; [
            qt6.qtbase
            qt6.qtmultimedia
            qt6.qtsvg
            qt6.qtwayland
            protobuf
            abseil-cpp
          ];

          # Qt6's qt_add_executable breaks when the build dir is inside the
          # source tree. Use a fully out-of-tree build with an absolute source
          # path so CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR never overlap.
          dontUseCmakeBuildDir = true;

          configurePhase = ''
            runHook preConfigure
            mkdir -p $NIX_BUILD_TOP/build
            cmake -S $NIX_BUILD_TOP/source/src/ui \
                  -B $NIX_BUILD_TOP/build \
                  -GNinja \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DCMAKE_INSTALL_PREFIX=$out \
                  -DHYPERVANE_ASSETS_DIR=$out/share/hypervane/assets \
                  $cmakeFlags
            runHook postConfigure
          '';

          buildPhase = ''
            runHook preBuild
            ninja -C $NIX_BUILD_TOP/build -j$NIX_BUILD_CORES
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            ninja -C $NIX_BUILD_TOP/build install
            runHook postInstall
          '';

          postInstall = ''
            # Assets (intro video + platform badges + icon)
            mkdir -p $out/share/hypervane
            cp -r $src/assets $out/share/hypervane/assets

            # Desktop entry
            mkdir -p $out/share/applications
            cat > $out/share/applications/hypervane.desktop << EOF
[Desktop Entry]
Name=HyperVane
GenericName=Emulation Frontend
Comment=Intelligent multi-system emulation frontend and library manager
Exec=$out/bin/hypervane-ui
Icon=hypervane
Type=Application
Categories=Game;Emulator;
Keywords=emulator;rom;retro;gaming;
StartupNotify=true
StartupWMClass=hypervane-ui
EOF

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
