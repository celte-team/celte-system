# Use the official .NET SDK image to build the app
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build

# Set the working directory
WORKDIR /master

COPY *.csproj ./
RUN dotnet restore master.csproj

# Copy the project file and restore dependencies
COPY . .

WORKDIR /master

# Set the entry point for the container
ENTRYPOINT ["dotnet", "run", "--config", "configFile.yml"]
