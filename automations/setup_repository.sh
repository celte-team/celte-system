#!/bin/bash

# required arguments:
#   $1: path to the godot project where binaries must be copied (under /bin/deps)

# Check if the required argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_godot_project>"
    exit 1
fi

# Set the path to the godot project
GODOT_PROJECT_PATH=$1


# Set the install prefix dynamically
INSTALL_PREFIX="$GODOT_PROJECT_PATH/addons/celte/deps"

# Detect the operating system and set VCPKG_TARGET_TRIPLET accordingly
case "$OSTYPE" in
    linux-gnu*)
        export VCPKG_TARGET_TRIPLET="x64-linux"
        ;;
    darwin*)
        export VCPKG_TARGET_TRIPLET="arm64-osx"
        ;;
    msys*|cygwin*|win32)
        export VCPKG_TARGET_TRIPLET="x64-windows"
        ;;
    *)
        echo "Unsupported OS: $OSTYPE"
        exit 1
        ;;
esac

# Ensure VCPKG_ROOT is set
if [ -z "$VCPKG_ROOT" ]; then
    echo "Please set the VCPKG_ROOT environment variable to the path where vcpkg is installed."
    exit 1
fi

# Run CMake with the specified options using the preset
# cmake --preset defaultP ..

# if installation dir does not exist, show error and show current dir
if [ ! -d "$INSTALL_PREFIX" ]; then
    echo "The specified install prefix does not exist: $INSTALL_PREFIX"
    echo "Please create the directory and try again."
    echo "Script currently running in: $(pwd)"
    exit 1
fi

# print real path of the install prefix and ask for confirmation
echo "The headers and libraries will be installed to $(cd "$INSTALL_PREFIX" && pwd)"

# Skip confirmation in non-interactive environments (like Docker/CI)
# Check if CI/DOCKER env var is set or if stdin is not a terminal
if [ "$CI" = "true" ] || [ "$DOCKER" = "true" ] || [ ! -t 0 ]; then
    echo "Running in non-interactive mode, skipping confirmation"
    # Set REPLY to 'y' to continue
    REPLY="y"
else
    # Ask for confirmation in interactive mode
    read -p "Do you want to continue? (y/n) " -n 1 -r
    echo
fi

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Exiting..."
    exit 1
fi

# setup cmake
echo "Setting up the project..."
mkdir -p system/build
cmake --preset=default -G Ninja -S ./system -B ./system/build  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

# build && install
echo "Building the project..."
cmake --build ./system/build --target install

if [ $? -ne 0 ]; then
    echo "Build failed. Exiting..."
    exit 1
fi

# Check if the godot is installed (ie GODOT_PATH environment variable is set)
if [ -z "$GODOT_PATH" ]; then
    echo "Godot is not installed. Please set the GODOT_PATH environment variable to the path where Godot is installed."
    echo "You may install godot 4.2 by downloading it from https://godotengine.org/download/archive/"
fi

echo "Setup complete."
