FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /app

COPY ClockServer/*.csproj ./ClockServer/
RUN dotnet restore ./ClockServer/ClockServer.csproj

COPY ClockServer/. ./ClockServer/
WORKDIR /app/ClockServer
RUN dotnet publish -c Release -o out

FROM mcr.microsoft.com/dotnet/runtime:8.0
WORKDIR /app
COPY --from=build /app/ClockServer/out .

ENTRYPOINT ["dotnet", "ClockServer.dll"]