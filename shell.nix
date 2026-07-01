# Compatibility shim — delegates to flake.nix via flake-compat.
# Usage: nix-shell   (no flakes needed)
(import
  (let lock = builtins.fromJSON (builtins.readFile ./flake.lock);
   in fetchTarball {
     url  = "https://github.com/edolstra/flake-compat/archive/${lock.nodes.flake-compat.locked.rev}.tar.gz";
     sha256 = lock.nodes.flake-compat.locked.narHash;
   })
  { src = ./.; }
).shellNix
