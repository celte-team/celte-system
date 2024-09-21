# /////////////////////////////////////////
# This is a Dockerfile for building a container image able to use :
    # - vcpkg
    # - c++

    # please for ARM64, use the following command:
        # Docker buildx build -t [NAMEOFTHEIMAGE]_x86 --platform=linux/amd64 . --output type=docker
        # Docker run -p 8080-8100:8080-8100 -p 35729:35729 -v $(pwd):/workdir -it [NAMEOFTHEIMAGE]_x86 /bin/bash
        # Then cd /workdir and run the following command:
        # cd build, etc...

# ////////////////////////////////////////


# Use Fedora as the base image
FROM fedora:38

# Set environment variables for vcpkg paths
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$PATH:/opt/vcpkg
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

# //
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++
ENV VCPKG_TARGET_TRIPLET="x64-linux"
RUN echo "export VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}" >> ~/.bashrc

# // not sure if this is needed
#   CMAKE_PREFIX_PATH:STRING="/opt/vcpkg/installed/x64-linux"
#   CMAKE_TOOLCHAIN_FILE:FILEPATH="/opt/vcpkg/scripts/buildsystems/vcpkg.cmake"
# //

# Update the system and install required packages
RUN dnf update -y && \
    dnf install -y \
    git \
    gcc \
    gcc-c++ \
    cmake \
    ninja-build \
    curl \
    unzip \
    zip \
    tar \
    pkgconfig \
    which \
    make \
    bash-completion \
    python3 \
    python3-pip \
    pacman

RUN dnf install -y glib2-devel

RUN dnf install -y openssl openssl-devel

RUN dnf install perl -y

RUN dnf install openssl-devel openssl -y
RUN dnf install libcurl-devel -y
RUN dnf install perl-IPC-Cmd -y

# ÷///
RUN dnf install kernel-devel -y
# RUN dnf install linux-libc-dev -y

# RUN pacman -Sy base-devel --noconfirm

#  autoconf automake autoconf-archive
RUN dnf install autoconf automake autoconf-archive -y

# Clone vcpkg and bootstrap it
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT && \
    cd $VCPKG_ROOT && \
    ./bootstrap-vcpkg.sh -useSystemBinaries

# Optionally, you can install some common packages using vcpkg
# Example: RUN $VCPKG_ROOT/vcpkg install boost

# Set the default work directory
WORKDIR /workspace

# Display vcpkg help when starting the container
CMD ["vcpkg", "--help"]
