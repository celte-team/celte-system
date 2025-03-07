# This file is used to compile is the base image for the SN container
# You can use it to test you new feature inside a production like environment.
FROM fedora:41 AS builder

# Environment setup
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$PATH:$VCPKG_ROOT
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++
ENV CXXFLAGS="-std=c++17"

# Install all dependencies in a single layer
RUN dnf update -y && \
    dnf install -y \
    git \
    gcc \
    gcc-c++ \
    cmake \
    ninja-build \
    curl \
    unzip \
    plocate \
    libstdc++ \
    libstdc++-devel \
    libstdc++-static \
    fontconfig \
    boost-system \
    fontconfig-devel \
    boost-devel \
    zip \
    tar \
    pkgconfig \
    which \
    make \
    bash-completion \
    python3 \
    python3-pip \
    pacman \
    scons \
    glib2-devel \
    libfl-devel \
    perl \
    libcurl-devel \
    perl-IPC-Cmd \
    kernel-devel \
    perl-FindBin \
    autoconf \
    automake \
    autoconf-archive \
    snapd \
    ansible \
    wget && \
    dnf clean all && \
    rm -rf /var/cache/dnf/*

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT && \
    cd $VCPKG_ROOT && \
    ./bootstrap-vcpkg.sh -useSystemBinaries

# Install Python dependencies
RUN pip3 install pyyaml

# Install godot-cpp
RUN mkdir -p /workdir/godot-cpp && \
    git clone https://github.com/godotengine/godot-cpp.git /workdir/godot-cpp && \
    cd /workdir/godot-cpp && \
    git checkout 4.2 && \
    git submodule update --init --recursive && \
    scons

COPY debug_draw_3d.gdextension /workdir/debug_draw_3d.gdextension

RUN ARCH=$(uname -m) && \
    if [ "$ARCH" = "aarch64" ]; then \
        git clone https://github.com/DmitriySalnikov/godot_debug_draw_3d.git && \
        cd godot_debug_draw_3d && \
        git submodule update --init --recursive && \
        rm -rf godot-cpp && \
        cp -r /workdir/godot-cpp godot-cpp && \
        rm -f addons/debug_draw_3d/debug_draw_3d.gdextension && \
        cp /workdir/debug_draw_3d.gdextension addons/debug_draw_3d/debug_draw_3d.gdextension && \
        sed -i '1i #include <utility>' src/3d/debug_draw_3d.cpp && \
        # Build for ARM64
        scons platform=linux target=editor arch=arm64 -j$(nproc) && \
        scons platform=linux target=template_debug arch=arm64 -j$(nproc) && \
        scons platform=linux target=template_release arch=arm64 -j$(nproc); \
    elif [ "$ARCH" = "x86_64" ]; then \
        git clone https://github.com/DmitriySalnikov/godot_debug_draw_3d.git && \
        cd godot_debug_draw_3d && \
        git submodule update --init --recursive && \
        rm -rf godot-cpp && \
        cp -r /workdir/godot-cpp godot-cpp && \
        rm -f addons/debug_draw_3d/debug_draw_3d.gdextension && \
        cp /workdir/debug_draw_3d.gdextension addons/debug_draw_3d/debug_draw_3d.gdextension && \
        sed -i '1i #include <utility>' src/3d/debug_draw_3d.cpp && \
        # Build for x86_64
        scons platform=linux target=editor arch=x86_64 -j$(nproc) && \
        scons platform=linux target=template_debug arch=x86_64 -j$(nproc) && \
        scons platform=linux target=template_release arch=x86_64 -j$(nproc); \
    fi

# Install Godot
RUN ARCH=$(uname -m) && \
    if [ "$ARCH" = "x86_64" ]; then \
        GODOT_URL="https://github.com/godotengine/godot/releases/download/4.2.2-stable/Godot_v4.2.2-stable_linux.x86_64.zip"; \
    elif [ "$ARCH" = "aarch64" ]; then \
        GODOT_URL="https://github.com/godotengine/godot/releases/download/4.2.2-stable/Godot_v4.2.2-stable_linux.arm64.zip"; \
    else \
        echo "Unsupported architecture: $ARCH"; exit 1; \
    fi && \
    wget -O /tmp/godot.zip "${GODOT_URL}" && \
    unzip /tmp/godot.zip -d /usr/local/bin && \
    mv /usr/local/bin/Godot_v4.2.2-stable_linux.* /usr/local/bin/godot && \
    chmod +x /usr/local/bin/godot && \
    rm -f /tmp/godot.zip

# Update system libraries
RUN updatedb && ldconfig
