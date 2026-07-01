{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    python313
    python313Packages.pip
    python313Packages.virtualenv
    # Playwright needs these system libs
    chromium
    playwright-driver.browsers
  ];

  shellHook = ''
    export PLAYWRIGHT_BROWSERS_PATH=${pkgs.playwright-driver.browsers}
    export PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1

    if [ ! -d .venv ]; then
      python3 -m venv .venv
      .venv/bin/pip install -r requirements.txt
    fi
    source .venv/bin/activate
  '';
}
