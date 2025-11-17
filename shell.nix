{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    cmake
    ninja
    clang
    pkg-config

    # Core dependencies
    openssl
    zstd
    zlib

    # Testing frameworks
    gtest
    gbenchmark

    # GUI dependencies
    glfw
    glew
    libGL
    xorg.libX11
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXi
    xorg.libXext

    # Development tools
    git
    gdb
  ];

  # Set environment variables
  CMAKE_EXPORT_COMPILE_COMMANDS = "1";
  CXXFLAGS = "-std=c++17";
}
