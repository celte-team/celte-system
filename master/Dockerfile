# Use the official .NET SDK image to build the app
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build

WORKDIR /app

COPY *.csproj ./
RUN dotnet restore

COPY . .
RUN dotnet publish -c Release -o out

# Use the official .NET runtime image to run the app
FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS runtime

# Install telnet
RUN apt-get update && apt-get install -y telnet iputils-ping

RUN ping -c 1 google.com

WORKDIR /app
COPY --from=build /app/out .
COPY configFile.yml configFile.yml
# Set the entry point for the container
ENTRYPOINT ["dotnet", "master.dll", "--config", "configFile.yml"]
# ENTRYPOINT ["ping", "172.16.106.182"]
