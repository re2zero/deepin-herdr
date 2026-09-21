# Vendored: terminalwidget

## Origin

- Repository: https://github.com/linuxdeepin/deepin-terminal
- Tag: `6.5.40` (library version 0.14.1, target `libterminalwidget6.so`)
- Path: `3rdparty/terminalwidget/lib` ← upstream `3rdparty/terminalwidget/lib`
- Vendored on: 2026-09-16
- License: mixed GPL-2.0+/GPL-3.0+/LGPL-2.0+/BSD-3-clause (Konsole-derived
  code plus deepin additions); see `LICENSE.terminalwidget`,
  `LICENSE.BSD-3-clause`, `LICENSE.LGPL2+`. Compatible with this
  project's GPL-3.

deepin-terminal ships this library; deepin installs it system-wide as
`libterminalwidget6`. We vendor the sources so builds on other platforms
(Windows/macOS/portable Linux) do not depend on that system package.

## Why the loose headers were replaced

Before vendoring, this directory only held ABI headers matched to the
system library. The full sources (with `color-schemes/`, `kb-layouts/`,
OSC 52 support, Theme1-10 schemes) are required for self-contained
builds and were taken from the same upstream version.

## Local modifications

1. `CMakeLists.txt` — this directory's build script is a lean local
   replacement for the upstream one (upstream requires lxqt-build-tools).
   Static by default (`TERMINALWIDGET_STATIC_BUILD=OFF` for shared),
   no lib-translation handling, installs color schemes and keyboard
   layouts into `<prefix>/share/terminalwidget<QtMajor>/`.
2. `lib/encodes/detectcode.cpp` — libchardet is optional
   (`TERMINALWIDGET_HAVE_CHARDET`, enabled when pkg-config finds
   `chardet` and `TERMINALWIDGET_DISABLE_CHARDET=OFF`): without it the
   uchardet path drives encoding detection, which makes macOS/homebrew
   builds possible.
3. `lib/tools.cpp` — relocatable data-dir fallback (searched only when
   the compile-time `KB_LAYOUT_DIR` / `COLORSCHEMES_DIR` does not exist):
   `<appdir>/../share/terminalwidget<QtMajor>/{kb-layouts,color-schemes}`.
   This is what makes AppImage / Windows portable trees work; it is a
   no-op for a regular `/usr` install.
4. `qtermwidget_export.h` (kept one level up, next to this file) — the
   CMake-generated export header, committed for builds that link the
   **system** library. The vendored build generates its own into the
   build directory.

## Build modes

- Default on Linux (`USE_SYSTEM_TERMINALWIDGET=ON`): links the system
  `libterminalwidget<QtMajor>`; data files come from the system install.
- `-DUSE_SYSTEM_TERMINALWIDGET=OFF` (forced on Windows/macOS): builds
  the vendored static library and installs its data files; the app finds
  them via the relocatable fallback when running from a bundle/prefix.
- Windows: `kpty` is POSIX-only — bundling a Windows build waits for the
  ConPTY port (roadmap M4).

## Runtime dependencies (bundled builds)

- Qt (Widgets, Network, Core5Compat on Qt6) — bundle via
  windeployqt / macdeployqt / linuxdeploy.
- ICU (i18n, uc) — ships with Qt on Windows/macOS.
- libchardet (`chardet/chardet.h`, used by `lib/encodes/detectcode.cpp`)
  — small C library, static-link or bundle.

## Updating the vendored copy

```bash
git clone --depth 1 --branch <deepin-terminal-tag> https://github.com/linuxdeepin/deepin-terminal /tmp/dt-src
rm -rf 3rdparty/terminalwidget/lib
cp -r /tmp/dt-src/3rdparty/terminalwidget/lib 3rdparty/terminalwidget/lib
# re-apply the tools.cpp relocatable fallback (see git log of this file)
# local patch: ProcessInfo.cpp getpwuid_r cast uses Linux-only __uid_t;
# POSIX uid_t works on both Linux and macOS (macOS build fix, 2026-09-21)
# local patch: CMakeLists links iconv on APPLE (not in libSystem there);
# static-library consumers fail at link time without it (2026-09-21)
```
