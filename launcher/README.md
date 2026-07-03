# 秋霜玉 Launcher

The launcher is a cross-platform [.NET 8](https://dotnet.microsoft.com/) desktop
application built with [Avalonia UI](https://avaloniaui.net/).

Avalonia is distributed under the MIT license:
<https://github.com/AvaloniaUI/Avalonia/blob/master/licence.md>

## Build

From the repository root:

```sh
dotnet build launcher/GIAN07.Launcher.csproj --configuration Release
```

Output is written to `bin/launcher/Release/`.

## Publish a single Windows executable

```sh
dotnet publish launcher/GIAN07.Launcher.csproj \
  --configuration Release \
  -p:PublishProfile=WindowsSingleFile
```

The single executable is written to:

```text
bin/launcher/Release/win-x64/GIAN07_launcher.exe
```

Single-file output is platform-specific. Linux uses the `linux-x64` runtime
and produces `bin/launcher/Release/linux-x64/GIAN07_launcher`.

The existing CMake presets expose the same project as:

- `windows-launcher-debug`
- `windows-launcher-release`
- `linux-launcher-debug`
- `linux-launcher-release`
