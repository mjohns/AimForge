#!/usr/bin/env python3
#
# Builds the SDL_shadercross tool and installs it locally.
#
# Prerequisites:
#   Python 3
#   Git
#   C++ compiler
#   CMake
#   Ninja
#
# Linux-only prerequisites:
#   patchelf
#
# Edit WORKING_DIR and BIN_INSTALL_DIR and LIB_INSTALL_DIR to suit your needs.
#

import os
import shutil
import subprocess
import sys

SDL_TAG = "release-3.2.6"

platform = sys.platform

if platform == "darwin" or platform == "linux":
    WORKING_DIR = os.path.expanduser("~/source/SDL3")
    BIN_INSTALL_DIR = os.path.expanduser("~/bin")
    LIB_INSTALL_DIR = os.path.expanduser("~/lib")
elif platform == "win32":
    WORKING_DIR = os.path.expanduser("~/shadercross_tmp/SDL3")
    BIN_INSTALL_DIR = os.path.expanduser("~/bin2")
    LIB_INSTALL_DIR = BIN_INSTALL_DIR
else:
    raise RuntimeError("Unsupported platform")


def main():
    if platform == "win32":
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        print("If you get the error:")
        print("    No CMAKE_C_COMPILER could be found.")
        print("then you need to run this script from a Developer Command Prompt for Visual Studio.")
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")

    os.makedirs(WORKING_DIR, exist_ok=True)

    if not os.path.exists(BIN_INSTALL_DIR):
        raise RuntimeError(f"Install directory {BIN_INSTALL_DIR} does not exist")

    if not os.path.exists(LIB_INSTALL_DIR):
        raise RuntimeError(f"Install directory {LIB_INSTALL_DIR} does not exist")

    clone_and_build_sdl()

    clone_and_build_shadercross()

    if platform == "darwin":
        install_binaries_macos()
    elif platform == "win32":
        install_binaries_windows()
    elif platform == "linux":
        install_binaries_linux()
    else:
        raise RuntimeError("Unsupported platform")

    cleanup()


def clone_and_build_sdl():
    sdl_dir = os.path.join(WORKING_DIR, "SDL")
    if not os.path.exists(sdl_dir):
        run("Cloning SDL", ["git", "clone", "https://github.com/libsdl-org/SDL", sdl_dir])

    run("Fetching SDL origin", ["git", "fetch", "origin"], cwd=sdl_dir)
    run(f"Resetting SDL to {SDL_TAG}", ["git", "reset", "--hard", SDL_TAG], cwd=sdl_dir)

    build_dir = os.path.join(sdl_dir, "build")
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

    run(
        "Configuring SDL",
        ["cmake", "-Bbuild", "-S.", "-GNinja", "-DCMAKE_BUILD_TYPE=Release"],
        cwd=sdl_dir,
    )
    run("Building SDL", ["cmake", "--build", "build"], cwd=sdl_dir)


def clone_and_build_shadercross():
    shadercross_dir = os.path.join(WORKING_DIR, "SDL_shadercross")
    if not os.path.exists(shadercross_dir):
        run(
            "Cloning shadercross",
            [
                "git",
                "clone",
                "--recursive",
                "https://github.com/libsdl-org/SDL_shadercross",
                shadercross_dir,
            ],
        )

    run("Fetching shadercross origin", ["git", "fetch", "origin"], cwd=shadercross_dir)

    run(
        "Resetting shadercross to origin/main",
        ["git", "reset", "--hard", "--recurse-submodules", "origin/main"],
        cwd=shadercross_dir,
    )

    build_dir = os.path.join(shadercross_dir, "build")
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

    run(
        "Configuring shadercross",
        [
            "cmake",
            "-Bbuild",
            "-S.",
            "-GNinja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DSDLSHADERCROSS_DXC=ON",
            "-DSDLSHADERCROSS_VENDORED=ON",
            "-DSDL3_DIR=../SDL/build",
        ],
        cwd=shadercross_dir,
    )
    run("Building shadercross", ["cmake", "--build", "build"], cwd=shadercross_dir)


def install_binaries_macos():
    print(f"Installing binaries to {BIN_INSTALL_DIR} and libraries to {LIB_INSTALL_DIR}")

    shadercross_build_dir = os.path.join(WORKING_DIR, "SDL_shadercross", "build")

    shutil.copy(os.path.join(shadercross_build_dir, "shadercross"), BIN_INSTALL_DIR)

    spirv_cross_build_dir = os.path.join(shadercross_build_dir, "external", "SPIRV-Cross")

    shutil.copy(os.path.join(spirv_cross_build_dir, "libspirv-cross-c-shared.0.dylib"), LIB_INSTALL_DIR)

    directx_shader_compiler_build_dir = os.path.join(
        shadercross_build_dir, "external", "DirectXShaderCompiler", "lib"
    )

    shutil.copy(os.path.join(directx_shader_compiler_build_dir, "libdxcompiler.dylib"), LIB_INSTALL_DIR)

    sdl_build_dir = os.path.join(WORKING_DIR, "SDL", "build")

    shutil.copy(os.path.join(sdl_build_dir, "libSDL3.0.dylib"), LIB_INSTALL_DIR)

    shadercross_bin = os.path.join(BIN_INSTALL_DIR, "shadercross")

    run(
        "Fixing shadercross rpath #1",
        ["install_name_tool", "-delete_rpath", sdl_build_dir, shadercross_bin],
    )

    run(
        "Fixing shadercross rpath #2",
        ["install_name_tool", "-delete_rpath", spirv_cross_build_dir, shadercross_bin],
    )

    run(
        "Fixing shadercross rpath #3",
        [
            "install_name_tool",
            "-rpath",
            directx_shader_compiler_build_dir,
            LIB_INSTALL_DIR,
            shadercross_bin,
        ],
    )


def install_binaries_windows():
    print(f"Installing binaries to {BIN_INSTALL_DIR} and libraries to {LIB_INSTALL_DIR}")

    shadercross_build_dir = os.path.join(WORKING_DIR, "SDL_shadercross", "build")

    shutil.copy(os.path.join(shadercross_build_dir, "shadercross.exe"), BIN_INSTALL_DIR)

    spirv_cross_build_dir = os.path.join(shadercross_build_dir, "external", "SPIRV-Cross")

    shutil.copy(os.path.join(spirv_cross_build_dir, "spirv-cross-c-shared.dll"), LIB_INSTALL_DIR)

    directx_shader_compiler_build_dir = os.path.join(
        shadercross_build_dir, "external", "DirectXShaderCompiler", "bin"
    )

    shutil.copy(os.path.join(directx_shader_compiler_build_dir, "dxcompiler.dll"), LIB_INSTALL_DIR)

    sdl_build_dir = os.path.join(WORKING_DIR, "SDL", "build")

    shutil.copy(os.path.join(sdl_build_dir, "SDL3.dll"), LIB_INSTALL_DIR)


def install_binaries_linux():
    print(f"Installing binaries to {BIN_INSTALL_DIR} and libraries to {LIB_INSTALL_DIR}")

    shadercross_build_dir = os.path.join(WORKING_DIR, "SDL_shadercross", "build")

    shutil.copy(os.path.join(shadercross_build_dir, "shadercross"), BIN_INSTALL_DIR)

    spirv_cross_build_dir = os.path.join(shadercross_build_dir, "external", "SPIRV-Cross")

    shutil.copy(os.path.join(spirv_cross_build_dir, "libspirv-cross-c-shared.so.0"), LIB_INSTALL_DIR)

    directx_shader_compiler_build_dir = os.path.join(
        shadercross_build_dir, "external", "DirectXShaderCompiler", "lib"
    )

    shutil.copy(os.path.join(directx_shader_compiler_build_dir, "libdxcompiler.so"), LIB_INSTALL_DIR)

    sdl_build_dir = os.path.join(WORKING_DIR, "SDL", "build")

    shutil.copy(os.path.join(sdl_build_dir, "libSDL3.so.0"), LIB_INSTALL_DIR)

    shadercross_bin = os.path.join(BIN_INSTALL_DIR, "shadercross")

    run("Fixing shadercross rpath", ["patchelf", "--set-rpath", LIB_INSTALL_DIR, shadercross_bin])


def cleanup():
    print("Cleaning-up")
    shutil.rmtree(os.path.join(WORKING_DIR, "SDL", "build"))
    shutil.rmtree(os.path.join(WORKING_DIR, "SDL_shadercross", "build"))


def run(purpose, cmd_line, cwd=None):
    print("{}: {}".format(purpose, " ".join(cmd_line)))
    subprocess.run(cmd_line, check=True, encoding="utf-8", cwd=cwd)


if __name__ == "__main__":
    main()
