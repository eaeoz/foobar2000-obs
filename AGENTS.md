# OBS Plugin Template — Agent Guide

## Project structure

- **`buildspec.json`** — single source of truth for plugin name, version, displayName, author
- **`src/plugin-main.c`** — module entrypoint (`obs_module_load`), registers sources
- **`src/foobar2000-source.cpp`** — the plugin's source implementation (C++, uses GDI+)
- **`src/foobar2000-bridge/`** — foobar2000 component (`foo_obsbridge.dll`) that writes the current file path to a bridge file for the OBS plugin
- **`src/plugin-support.c.in`** — template, `@CMAKE_PROJECT_NAME@` / `@CMAKE_PROJECT_VERSION@` expanded at configure time
- **`.deps/`** — prebuilt OBS SDK and dependencies (not committed to git per `.gitignore`)
- **`data/locale/en-US.ini`** — localizable strings, referenced via `obs_module_text()`
- **`build-all.bat`** — builds OBS plugin + bridge component in one step
- **`script.nsi`** — NSIS installer, installs OBS plugin + bridge component to foobar2000

## Build

Use CMake presets, not manual `-G` flags:

```
cmake --preset windows-x64
cmake --build --preset windows-x64        # RelWithDebInfo
cmake --build build_x64 --config Debug    # alternative
```

- Build output: `build_x64/RelWithDebInfo/foobar2000-obs.dll`
- Post-build copies DLL + PDB to `build_x64/rundir/RelWithDebInfo/` automatically
- Plugin uses `C++20`, MSVC `/permissive-`, `/utf-8`, `/Zc:__cplusplus

### Bridge component

The `build_x64` preset also builds `foo_obsbridge.dll` (foobar2000 component) via `BUILD_FOOBAR2000_BRIDGE=ON`. The SDK is auto-downloaded via FetchContent. Output: `build_x64/out/foo_obsbridge.dll`.

## Install to OBS

```powershell
.\install-plugin.ps1                        # auto-detect OBS dir or prompt
.\install-plugin.ps1 -ObsDir "C:\Program Files\obs-studio"
```

Copies DLL → `{ObsDir}\obs-plugins\64bit\`, data → `{ObsDir}\data\obs-plugins\foobar2000-obs\`.

## Install to foobar2000

The NSIS installer (`script.nsi`) auto-detects foobar2000 and installs `foo_obsbridge.dll` to the components folder.

## Formatting

- **C/C++**: `clang-format-19` exactly (no other version accepted), config in `.clang-format`
- **CMake**: `gersemi >= 0.12.0`, config in `.gersemirc` (2-space indent, 120 line length)
- **Run**: `build-aux/run-clang-format [--check]` and `build-aux/run-gersemi [--check]`
- Format scripts are zsh-based symlinks into `build-aux/.run-format.zsh`
- CI checks formatting before build via `.github/workflows/check-format.yaml`

## Key OBS plugin patterns

- `OBS_DECLARE_MODULE()` in plugin-main.c before `obs_module_load`
- `OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")` — locale from `data/locale/`
- Source info struct (C-linkage) registered via `obs_register_source()`
- Texture creation (`gs_texture_create`) must happen on the render thread — from `video_render`, NOT `video_tick`, or it returns NULL in OBS 32
- `obs_log()` wrapper prefixes `[plugin-name]` automatically
- Logging prefix convention used: `[fb2k]` for source-specific messages

## Platform notes

| Platform | Generator | Build dir | Notes |
|----------|-----------|-----------|-------|
| Windows  | Visual Studio 18 2026 | `build_x64` | Win 10 SDK ≥ 10.0.20348 |
| macOS    | Xcode 16.0            | `build_macos` | Universal binary (arm64+x86_64), deployment target 12.0 |
| Ubuntu   | Ninja                 | `build_x86_64` | packages: `ninja-build pkg-config build-essential` |

CI build presets add `CMAKE_COMPILE_WARNING_AS_ERROR=ON`.

## CI workflows

- `push.yaml` / `pr-pull.yaml` → `check-format.yaml` + `build-project.yaml`
- `build-project.yaml` builds on all 3 platforms, packages artifacts, handles codesigning on macOS
- Tag push (semver like `1.2.3`) creates a draft release with packages
- PRs labeled `Seeking Testers` enable codesigning and packaging on macOS

## What not to touch

- `cmake/common/`, `cmake/windows/`, `cmake/macos/`, `cmake/linux/` — OBS-provided CMake modules
- `.github/actions/`, `.github/scripts/` — CI action implementations
- `build-aux/` — format runner scripts (zsh)

## Gotchas

- `.gitignore` is inverted (ignore everything, un-ignore select files) — new source files must be added to the whitelist
- No test suite exists; no lint or typecheck runs locally (only formatting check)
- `plugin-support.c.in` is a `.in` template configured by CMake, not edited directly
- GDI+ is initialized in `foobar2000_module_init()` / shut down in `foobar2000_module_unload()`
- The plugin targets `OBS_SOURCE_TYPE_INPUT` with `OBS_SOURCE_VIDEO` output flags only (no audio)
