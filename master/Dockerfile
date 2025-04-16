# Use the official .NET SDK image to build the app
FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build

WORKDIR /app

COPY *.csproj ./
RUN dotnet restore

COPY . .
RUN dotnet publish -c Release -o out

# Use the official .NET runtime image to run the app
FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS runtime


WORKDIR /app
COPY --from=build /app/out .
COPY configFile.yml configFile.yml
# Set the entry point for the container
ENTRYPOINT ["dotnet", "master.dll", "--config", "configFile.yml"]