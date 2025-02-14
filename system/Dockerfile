FROM clmt/celte_server:latest

COPY . /system

WORKDIR /system

# RUN ls -la
RUN rm -fr build && mkdir build

# RUN cd build && \
#     cmake --preset default -G Ninja ..

# RUN cd build && \
#     ninja

# run this command when i use the command:
#     docker run -it --rm clmt/celte_system:latest
#     ../tests/server.debug
# CMD ["./build/tests/server.debug"]
# docker buildx build --platform linux/amd64 --builder cloud-clmt-celte-system . --output type=docker
# ENTRYPOINT ["./build/tests/server.debug"]


# //////////

FROM fedora:38 AS builder

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$PATH:$VCPKG_ROOT
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

# Install necessary tools and libraries
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
    python3 \
    python3-pip \
    glib2-devel \
    libfl-devel \
    openssl-devel \
    boost-devel \
    kernel-headers \
    perl-IPC-Cmd \
    perl-FindBin

RUN dnf install kernel-headers kernel-devel

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT && \
    cd $VCPKG_ROOT && \
    ./bootstrap-vcpkg.sh -useSystemBinaries

# Install Python dependencies
RUN pip3 install pyyaml

# Install additional tools
RUN dnf install -y godot

# Copy project files
COPY system /system

# Ensure build folder exists and run CMake with Ninja
WORKDIR /system

RUN rm -fr build && mkdir build && cd build && cmake --preset default ..

# Final runtime image
FROM alpine:latest

COPY --from=builder /opt/vcpkg /opt/vcpkg
COPY --from=builder /usr/bin/godot /usr/bin/godot
COPY --from=builder /usr/bin/c++ /usr/bin/c++
COPY --from=builder /usr/bin/gcc /usr/bin/gcc
COPY --from=builder /usr/lib64 /usr/lib64
COPY --from=builder /lib64 /lib64

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$PATH:/opt/vcpkg
ENV GODOT_PATH=/usr/bin/

# Install runtime dependencies
RUN apk add --no-cache \
    libstdc++ \
    libc6-compat \
    openssl \
    curl \
    bash \
    python3
