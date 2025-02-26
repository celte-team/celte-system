# This file is used to compile the celte-godot server.
FROM fedora:41 AS builder

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$PATH:$VCPKG_ROOT
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

RUN dnf update -y && \
    dnf install -y \
    git \
    gcc \
    gcc-c++ \
    cmake \
    ninja-build \
    curl \
    unzip \
    plocate

RUN dnf install -y \
    fontconfig \
    boost-system \
    fontconfig-devel\
    boost-devel

RUN dnf install -y \
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
    perl-FindBin \
    autoconf \
    automake \
    autoconf-archive \
    snapd \
    ansible \
    wget \
    godot

RUN sudo updatedb

RUN sudo ldconfig


RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT && \
    cd $VCPKG_ROOT && \
    ./bootstrap-vcpkg.sh -useSystemBinaries

RUN pip3 install pyyaml

RUN dnf install -y perl perl-IPC-Cmd \
    autoconf automake autoconf-archive

COPY . /workdir

WORKDIR /workdir

# Compilation of the server

RUN mkdir -p system/build

RUN cd system/build && \
    cmake --preset default -DCMAKE_INSTALL_PREFIX=/workdir/celte-godot/addons/celte/deps/ ..

RUN cd system/build && ninja install

RUN rm -fr /workdir/celte-godot/projects/demo1/addons/celte

RUN mkdir -p /workdir/celte-godot/projects/demo1/addons

RUN ln -s /workdir/celte-godot/addons/celte /workdir/celte-godot/projects/demo1/addons/celte

RUN cd /workdir/celte-godot/extension-standalone && scons

RUN cd /workdir/celte-godot/projects/demo1 && godot . -v -e --quit-after 2 --headless

## Final runtime image

FROM fedora:41 AS runtime

ENV CELTE_HOST=192.168.0.161
ENV PUSHGATEWAY_HOST=192.168.0.161
ENV CELTE_MODE=server

# Install necessary dependencies for the runtime
RUN dnf install -y \
    fontconfig \
    wget \
    unzip \
    boost-system \
    boost-devel

# Detect architecture and set Godot URL accordingly
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

COPY --from=builder /workdir/celte-godot /workdir/celte-godot

WORKDIR /workdir/celte-godot/projects/demo1

CMD export CELTE_MODE=server && godot . --headless
