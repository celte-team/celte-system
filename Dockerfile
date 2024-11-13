# /////////////////////////////////////////
# This is a Dockerfile for building a container image able to use :
    # - vcpkg
    # - c++

    # please for ARM64, use the following command:
        # Docker buildx build -t [NAMEOFTHEIMAGE]_x86 --platform=linux/amd64 . --output type=docker
        # Docker run -p 8080-8100:8080-8100 -p 35729:35729 -v $(pwd):/celte-system -it [NAMEOFTHEIMAGE]_x86 /bin/bash
        # Then cd /celte-system and run the following command:
        # cd build, etc...

# ////////////////////////////////////////


# Use Fedora as the base image
FROM fedora:38

# Set environment variables for vcpkg paths
ENV VCPKG_ROOT=/opt/vcpkg
ENV GODOT_ROOT=/opt/godot
ENV PATH=$PATH:/opt/vcpkg
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++
ENV VCPKG_TARGET_TRIPLET="x64-linux"
RUN echo "export VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}" >> ~/.bashrc

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
    pacman \
    scons

# Additional dependencies
RUN dnf install -y \
    glib2-devel \
    libfl-devel \
    openssl \
    openssl-devel \
    perl \
    libcurl-devel \
    perl-IPC-Cmd \
    kernel-devel \
    librdkafka-devel \
    librdkafka \
    perl-FindBin \
    autoconf \
    automake \
    autoconf-archive \
    snapd \
    boost-devel \
    ansible

# Clone vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT && \
    cd $VCPKG_ROOT && \
    ./bootstrap-vcpkg.sh -useSystemBinaries

RUN pip3 install pyyaml

COPY . /celte-system

# install godot
ENV GODOT_PATH="/usr/bin/"

RUN dnf install godot -y

WORKDIR /celte-system

RUN rm -fr build && mkdir build \
    && cd build \
    && cmake --preset default .. \
    && make -j