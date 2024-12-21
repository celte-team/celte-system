# Use the official .NET SDK image to build the app
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
# FROM fedora:latest

# Set the working directory
WORKDIR /master

# RUN dnf install -y dotnet-sdk-8.0

COPY *.csproj ./
RUN dotnet restore master.csproj

# Copy the project file and restore dependencies
COPY . .

WORKDIR /master

# ENV DOCKER_HOST_IP=192.168.1.130
# ENV PULSAR_BROKERS=pulsar://192.168.1.130:6650
# ENV REDIS_HOST=192.168.1.130:6379


# Set the entry point for the container
ENTRYPOINT ["dotnet", "run", "--config", "configFile.yml"]
