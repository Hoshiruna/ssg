# 秋霜玉

## Building

The modern Windows and Linux binaries use [CMake](https://cmake.org/) with
[Ninja](https://ninja-build.org/). CMake and Ninja manage the build; the
actual compiler is MSVC on Windows and GCC or Clang on Linux.

The minimum supported versions are:

* CMake 4.0.3
* Ninja 1.11
* Visual Studio 2022 17.6, GCC 15, or Clang 18.1.2

The older Tup build remains available for the Windows 98-compatible binary.

All binaries will be put into the `bin/` subdirectory.

### Windows

MSVC is the supported Windows compiler. The build is 32-bit, so commands must
be run from Visual Studio's *x64_x86 Cross Tools Command Prompt*.

To build:

1. Install [Git for Windows](https://gitforwindows.org/).
2. Install Visual Studio Community 2022, with the *Desktop development for C++* workload.\
   If you haven't already installed the IDE for other projects and don't plan to, you can install only the command-line compilers via the [Build Tools installer](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022).
3. Install CMake and Ninja and make sure both are in `PATH`.
4. Open the *x64_x86 Cross Tools Command Prompt* and navigate to this checkout.
5. Build both configurations:

   ```batch
   build_windows.bat
   ```

   Pass `Debug` or `Release` to build only one configuration.

The equivalent direct preset commands are:

```batch
cmake --preset windows-msvc
cmake --build --preset windows-release
```

The repository's Visual Studio Code tasks use these CMake presets. Open VS Code
from the same compiler prompt:

```batch
code .
```

#### Windows 98 binary

The vintage target still uses Tup and its specialized dependency build:

```batch
build_windows_tup.bat bin/GIAN07_win98.exe
build_windows_tup.bat bin/GIAN07_win98d.exe
```

### Linux

Install CMake, Ninja, pkg-config, and the development packages for SDL 3,
PangoCairo, Fontconfig, WebP, Ogg, and Vorbis.

The build honors the `CC` and `CXX` environment variables when the preset is
configured for the first time. Build both Debug and Release with:

```sh
./build_linux.sh
```

Pass `Debug` or `Release` to build only one configuration. The equivalent
direct preset commands are:

```sh
cmake --preset linux
cmake --build --preset linux-release
```

Use `install_linux.sh` to copy a compiled release build to its standard install
locations.

## Debugging (Windows only)

.PDB files are generated for Debug and Release builds, so you should get symbol support with any Windows debugger.

### Visual Studio Code

Select between Debug and Release modes in the *Run and Debug* menu (`Ctrl-Shift-D` by default), and start debugging with the ▶ button or its keybinding.

### Visual Studio IDE

We don't support it for compilation, but you can still use it for debugging by running

```bat
devenv bin/GIAN07d.exe &::to run the Debug binary
devenv bin/GIAN07.exe  &::to run the Release binary
```

from the *x64_x86 Cross Tools Command Prompt*.
Strangely enough, this yields a superior IntelliSense performance than creating any sort of project. 🤷

----

Original README by pbg below.

----

## これは何？
* 西方プロジェクト第一弾 **秋霜玉** のソースコードです。
* コンパイルできるかもしれませんが, すべてのソースコードが含まれているわけではないのでリンクはできません。
* 画像、音楽、効果音、スクリプト等のリソースは含まれません。


## 参考までに
* 基本、開発当時（2000年前後）のままですが、文字コードを utf-8 に変更し、一部コメント（黒歴史ポエム）は削除してあります。インデント等も当時のままなので、読みにくい箇所があるかもしれません。
* 8bit/16bitカラーの混在、MIDI再生関連、浮動小数点数演算を避ける、あたりが懐かしポイントになるかと思います。
* 8.3形式のファイル名が多いのは、PC-98 時代に書いたコードの一部を流用していたためです。
* リソースのアーカイブ展開に関するコードはもろもろの影響を考え、このリポジトリには含めていません。


## ディレクトリ構成
* /**MAIN** : 秋霜玉WinMainあたり
* /**GIAN07** : 秋霜玉本体
* /**DirectXUTYs** : DirectX, MIDI再生、数学関数等の共通処理
* /**MapEdit2** : マップエディタ
* /**ECLC** : ECL(敵制御用) スクリプトコンパイラ
* /**SCLC** : SCL(敵配置用) スクリプトコンパイラ


## たぶん紛失してしまったソースコード
以下のコードについては、見つかり次第追加するかもしれません。
* リソースのアーカイバ


## ライセンス
* [MIT](LICENSE)
