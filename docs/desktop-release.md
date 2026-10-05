# Desktop packages

## Ubuntu (.deb)

Requirements: GCC, CMake, Ninja and the Qt 6 packages
(`qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-l10n-tools` and the
`qml6-module-qtquick*` runtime modules).

```bash
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBALLISTICS_BUILD_TESTS=OFF
cmake --build build/release
(cd build/release && cpack -G DEB)
```

Result: `build/release/balcalc_<version>_amd64.deb`, built against the system
Qt (its library dependencies are found by `dpkg-shlibdeps`, the QML modules
are listed in `cmake/Packaging.cmake`). Install and remove:

```bash
sudo apt install ./build/release/balcalc_*_amd64.deb
sudo apt remove balcalc
```

The package installs `balcalc` (menu entry "BalCalc") and `bal-cli` in
`/usr/bin`, the icon, AppStream metadata and the licences in
`/usr/share/doc/balcalc`.

## Windows (ZIP and installer)

Requirements: Visual Studio 2022 (MSVC), Qt 6.9.2 `msvc2022_64`, CMake and,
for the installer, NSIS (`winget install --id NSIS.NSIS -e`).

```powershell
cmake --preset windows-msvc
cmake --build build\windows-msvc --config Release
cpack --config build\windows-msvc\CPackConfig.cmake -C Release -B build\windows-msvc\packages
```

Results in `build\windows-msvc\packages`:

- `BalCalc-<version>-win64.exe` - installer (Start-menu shortcut, uninstaller
  in Settings -> Apps); only when NSIS is installed;
- `BalCalc-<version>-win64.zip` - portable: unpack anywhere and run
  `bin\balcalc.exe`.

Both carry the Qt libraries, QML modules and the Visual C++ runtime, so no
other installation is needed. The user's database lives in
`%APPDATA%\vetalguru\BalCalc\balcalc.db` and survives reinstalls.

## Version

`project(ballistics VERSION x.y.z)` in the top-level `CMakeLists.txt` sets
the package version (and the Android version name).
