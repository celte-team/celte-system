FROM clmt/celte_server:latest

COPY . /system

WORKDIR /system

RUN ls -la && \
    rm -rf build && mkdir build && \
    cd build && \
    cmake --preset default -G Ninja ..

# RUN cd build && \
#     ninja

# run this command when i use the command:
#     docker run -it --rm clmt/celte_system:latest
#     ../tests/server.debug
# CMD ["./build/tests/server.debug"]
# docker buildx build --platform linux/amd64 --builder cloud-clmt-celte-system . --output type=docker
ENTRYPOINT ["./build/tests/server.debug"]