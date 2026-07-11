# 秋霜玉 Configurator

The configurator is a cross-platform [.NET 8](https://dotnet.microsoft.com/) desktop
application built with [Avalonia UI](https://avaloniaui.net/).

Avalonia is distributed under the MIT license:
<https://github.com/AvaloniaUI/Avalonia/blob/master/licence.md>

## Build

From the repository root:

```sh
dotnet build configurator/GIAN07.Configurator.csproj --configuration Release
```

Output is written to `bin/configurator/Release/`.

## Publish a single Windows executable

```sh
dotnet publish configurator/GIAN07.Configurator.csproj \
  --configuration Release \
  -p:PublishProfile=WindowsSingleFile
```

The single executable is written to:

```text
bin/configurator/Release/win-x64/GIAN07_configurator.exe
```

Single-file output is platform-specific. Linux uses the `linux-x64` runtime
and produces `bin/configurator/Release/linux-x64/GIAN07_configurator`.

The existing CMake presets expose the same project as:

- `windows-configurator-debug`
- `windows-configurator-release`
- `linux-configurator-debug`
- `linux-configurator-release`
