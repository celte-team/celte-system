# Variables
CLIENT_DIR = client-side/test
SERVER_DIR = server-side
RUST_DIR = $(SERVER_DIR)/network-layer
CS_DIR = $(SERVER_DIR)/master/core
DOCKER_DIR = $(SERVER_DIR)/docker
LIB_DIR = /usr/lib64/dotnet/shared/Microsoft.NETCore.App/8.0.4/
# Phony targets
.PHONY: all clean client server rust cs docker

# Default target
all: server

# Clean the project
clean:
	rm -rf $(CLIENT_DIR)/*.o $(RUST_DIR)/target $(CS_DIR)/bin $(CS_DIR)/obj

# Build the client
client:
	@g++ -o $(CLIENT_DIR)/main $(CLIENT_DIR)/main.cpp

# Build the C# part of the server
cs:
	@dotnet build $(CS_DIR)/core.csproj

# Build the Docker part of the server
docker:
	docker build -t serverstack $(DOCKER_DIR)
