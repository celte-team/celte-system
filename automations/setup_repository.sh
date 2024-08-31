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

# Run CMake with the specified options
cmake -DCMAKE_INSTALL_PREFIX="$GODOT_PROJECT_PATH/bin/deps" \
						..

# Build the project
echo "Building the project..."
make

if [ $? -ne 0 ]; then
				echo "Build failed. Exiting..."
				exit 1
fi

# Install the headers and libraries
echo "Installing the headers and libraries..."
make install

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
				elif	[[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
								echo "Please install Ansible manually"
				fi
fi

# Check if the godot is installed (ie GODOT_PATH environment variable is set)
if [ -z "$GODOT_PATH" ]; then
				echo "Godot is not installed. Please set the GODOT_PATH environment variable to the path where Godot is installed."
				echo "You may install godot 4.2 by downloading it from https://godotengine.org/download/archive/"
fi

echo "Setup complete."