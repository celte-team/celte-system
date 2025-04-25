FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src
COPY ["master.csproj", "."]
RUN dotnet restore "master.csproj"
COPY . .
RUN dotnet build "master.csproj" -c Release -o /app/build

FROM build AS publish
RUN dotnet publish "master.csproj" -c Release -o /app/publish

FROM mcr.microsoft.com/dotnet/aspnet:8.0 AS final
WORKDIR /app
COPY --from=publish /app/publish .
ENTRYPOINT ["dotnet", "master.dll"]