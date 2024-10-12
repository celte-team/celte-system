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

# Define the build directory
BUILD_DIR="build"

# Create the build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    mkdir "$BUILD_DIR"
fi

# Navigate to the build directory
cd "$BUILD_DIR"

# Set the install prefix dynamically
INSTALL_PREFIX="$GODOT_PROJECT_PATH/bin/deps"

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
cmake --preset default -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" ..

# Build the project
echo "Building the project..."
cd ..
cmake --preset=default -S . -B build
cd build && make -j
cd ..

if [ $? -ne 0 ]; then
    echo "Build failed. Exiting..."
    exit 1
fi

# Install the headers and libraries
cd build
echo "Installing the headers and libraries..."
cmake --install . --prefix "$INSTALL_PREFIX"

if [ $? -ne 0 ]; then
    echo "Installation failed. Exiting..."
    exit 1
fi

# Navigate back to the original directory
cd ..

echo "Headers and libraries have been installed to $GODOT_PROJECT_PATH/bin/deps"

# Check for requirements. If not installed, signal the user to install them
echo "Checking for requirements..."
if ! command -v ansible &> /dev/null; then
    echo "Ansible is not installed. Please install it using the following command:"
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "sudo apt install ansible"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "brew install ansible"
    elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        echo "Please install Ansible manually"
    fi
fi

# Check if the godot is installed (ie GODOT_PATH environment variable is set)
if [ -z "$GODOT_PATH" ]; then
    echo "Godot is not installed. Please set the GODOT_PATH environment variable to the path where Godot is installed."
    echo "You may install godot 4.2 by downloading it from https://godotengine.org/download/archive/"
fi

echo "Setup complete."