{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gcc
    gnumake
    pkg-config
    bear
  ];

  buildInputs = with pkgs; [
    sqlite
    glibc.dev # Stellt errno.h, pthread.h, string.h etc. bereit
  ];
}
