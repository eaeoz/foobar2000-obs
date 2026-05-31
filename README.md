# foobar2000 Now Playing

An [OBS Studio](https://obsproject.com/) plugin that displays the currently playing track from foobar2000 as an overlay source.

## Features

- Shows artist name, track title, and album art from foobar2000
- Reads track info directly from the foobar2000 window title — no additional components or IPC needed
- Two album art sources:
  - **foobar2000 album-art cache** (`%APPDATA%\foobar2000[-v2]\album-art\`)
  - **Embedded art** extracted from audio files via Windows `IShellItemImageFactory`
- Automatically searches for audio files in your music directories when cache is unavailable
- Transparent background — blends over any scene
- Clears the display when playback stops or foobar2000 is closed

## How it works

Every second, the plugin polls the foobar2000 window title via `EnumWindows` / `GetWindowText`. The window title format is `Artist - Title [foobar2000]`, which it parses to extract artist and track name.

Album art is resolved by checking foobar2000's album-art cache first, then falling back to embedded art extracted from matching audio files found in known music directories.

The overlay is rendered with GDI+ into a 750×340 pixel bitmap, converted to an OBS texture, and drawn as a sprite.

## Requirements

- **OBS Studio** ≥ 31.1.1
- **foobar2000** running on the same machine
- **Windows** 10 or later (x64)

## Installation

Download the [latest release](https://github.com/eaeoz/foobar2000-obs/releases/latest) and choose one of the methods below.

### Option 1 — EXE Installer (recommended)

[Download `foobar2000-obs-installer.exe`](https://github.com/eaeoz/foobar2000-obs/releases/download/1.0.0/foobar2000-obs-installer.exe)

Run the installer — it automatically detects your OBS Studio directory and copies all files to the correct locations. No manual steps required.

### Option 2 — Manual ZIP (for portable / multi-machine setups)

[Download `foobar2000-obs.zip`](https://github.com/eaeoz/foobar2000-obs/releases/download/1.0.0/foobar2000-obs.zip)

The ZIP contains these files:

```
foobar2000-obs.dll        # Plugin binary
foobar2000-obs.pdb        # Debug symbols (optional)
foobar2000-obs/
└── locale/
    └── en-US.ini          # Localization file
```

1. Extract the ZIP
2. Copy `foobar2000-obs.dll` and `foobar2000-obs.pdb` to  
   `{OBS_DIR}\obs-plugins\64bit\`
3. Copy the entire `foobar2000-obs` folder to  
   `{OBS_DIR}\data\obs-plugins\`  
   (result: `{OBS_DIR}\data\obs-plugins\foobar2000-obs\locale\en-US.ini`)
4. Restart OBS Studio

### Build from source

See [Development](#development) below.

## Usage in OBS

1. Add a new **Source** → **foobar2000 Now Playing** to your scene
2. (Optional) Set a custom **Music Directory** in the source properties to help locate audio files for embedded album art extraction
3. Make sure foobar2000 is running and playing a track

The overlay is 750×340 px. Scale or position it as needed.

## Development

### Prerequisites

- **Visual Studio 2022** (or later) with **Desktop development with C++** workload
- **CMake** ≥ 3.28 (bundled with Visual Studio)
- **Git**
- OBS Studio installed (for runtime testing)

### Clone

```powershell
git clone https://github.com/yourusername/foobar2000-obs
cd foobar2000-obs
```

### Setup dependencies

The build system fetches prebuilt OBS SDK and dependencies automatically. Check `.deps/` after the first configure step.

### Build

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

- Output: `build_x64/RelWithDebInfo/foobar2000-obs.dll`
- Build type: `RelWithDebInfo` (optimized with debug symbols)
- Alternative: `cmake --build build_x64 --config Debug`

If you are not using a Visual Studio Developer Command Prompt, run the build with full VC environment setup:

```cmd
cmd.exe /c "`"%VCVARS%`" x64 >nul && cmake --build --preset windows-x64"
```

(Set `%VCVARS%` to the path of `vcvarsall.bat` — e.g. `D:\VS\Product\VC\Auxiliary\Build\vcvarsall.bat`)

### Build installer (NSIS)

```powershell
makensis script.nsi
```

The NSIS script `script.nsi` creates `foobar2000-obs-installer.exe` which installs:

| File | Destination |
|------|-------------|
| `foobar2000-obs.dll` | `{OBS}\obs-plugins\64bit\` |
| `foobar2000-obs.pdb` | `{OBS}\obs-plugins\64bit\` |
| `locale\en-US.ini` | `{OBS}\data\obs-plugins\foobar2000-obs\locale\` |

The installer auto-detects the OBS Studio directory from the registry.

### Install to OBS

```powershell
.\install-plugin.ps1                        # auto-detect OBS dir
.\install-plugin.ps1 -ObsDir "C:\Program Files\obs-studio"
```

### Formatting

```powershell
build-aux\run-clang-format                   # C/C++ (uses clang-format-19)
build-aux\run-clang-format --check           # check-only
build-aux\run-gersemi                        # CMake (uses gersemi >= 0.12.0)
```

### Project structure

| Path | Purpose |
|------|---------|
| `src/foobar2000-source.cpp` | Main source implementation (C++, GDI+) |
| `src/plugin-main.c` | OBS module entrypoint |
| `src/plugin-support.c.in` | CMake-configured support template |
| `data/locale/en-US.ini` | Localizable strings |
| `CMakeLists.txt` | Build definition |
| `CMakePresets.json` | Build presets (Windows, macOS, Linux) |
| `buildspec.json` | Plugin metadata and dependency versions |
| `install-plugin.ps1` | OBS installation helper |
| `script.nsi` | NSIS installer script (builds `foobar2000-obs-installer.exe`) |
| `.deps/` | Prebuilt OBS SDK (not in git) |

### Platform support

| Platform | Generator | Build dir |
|----------|-----------|-----------|
| Windows | Visual Studio 18 2026 | `build_x64` |
| macOS | Xcode 16.0 | `build_macos` |
| Ubuntu | Ninja | `build_x86_64` |

## License

GNU General Public License v2.0. See [LICENSE](LICENSE).
