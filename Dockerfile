# This file is used to compile the celte-godot server.
# Supports both ARM64 and x86_64 architectures
FROM clmt/parent-builder:latest AS builder

WORKDIR /workdir
COPY . .
# Ensure config is available at the expected path during build
COPY .celte.yaml /root/.celte.yaml

# Copy godot_debug_draw_3d from parent builder image
COPY --from=clmt/parent-builder:latest /godot_debug_draw_3d/addons/debug_draw_3d /workdir/debug_draw_3d

# Setup project structure
RUN mkdir -p /workdir/celte-godot/projects/demo-tek/addons/ && \
    mv /workdir/debug_draw_3d /workdir/celte-godot/projects/demo-tek/addons/debug_draw_3d

# Clean and prepare build directories
RUN rm -fr /workdir/system/build/ && \
    mkdir -p system/build && \
    rm -fr /workdir/celte-godot/projects/demo-tek/addons/celte && \
    ln -s /workdir/celte-godot/addons/celte /workdir/celte-godot/projects/demo-tek/addons/celte

# Build and setup
# The setup_repository.sh script will automatically detect the architecture
# and set the appropriate VCPKG_TARGET_TRIPLET (arm64-linux or x64-linux)
RUN chmod +x ./automations/setup_repository.sh && \
    ./automations/setup_repository.sh /workdir/celte-godot/projects/demo-tek && \
    cd system/build && ninja install && \
    cd /workdir/celte-godot/extension-standalone && scons && \
    cd /workdir/celte-godot/projects/demo-tek && godot . -v -e --quit-after 2 --headless

## Final runtime image
FROM fedora:41 AS runtime

# Environment variables (configurable at runtime)
# ENV CELTE_HOST=0.0.0.0
# ENV CELTE_PORT=6650
# ENV PUSHGATEWAY_HOST=0.0.0.0
# ENV CELTE_MODE=server
ENV CELTE_LOBBY_URL=http://192.168.1.113:5002/client

# Install runtime dependencies
RUN dnf install -y \
    fontconfig \
    boost-system \
    boost-devel && \
    dnf clean all && \
    rm -rf /var/cache/dnf/*

# Copy binaries and project files
COPY --from=builder /usr/local/bin/godot /usr/local/bin/godot
COPY --from=builder /workdir/celte-godot /workdir/celte-godot
# Ensure config is available at runtime too (unless overridden by a bind mount)
COPY --from=builder /root/.celte.yaml /root/.celte.yaml

WORKDIR /workdir/celte-godot/projects/demo-tek

CMD ["godot", ".", "--headless"]
