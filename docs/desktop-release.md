# Desktop packages

The desktop app is the Kotlin app in `kmp/` (Compose Multiplatform for the
JVM). Packages carry their own Java runtime and the C++ core
(`balcalc_jni`), so nothing else needs installing.

Requirements: JDK 17, CMake, and a C++ compiler — Visual Studio 2022 on
Windows, GCC and Ninja on Linux. Gradle comes with the wrapper; it builds the
C++ core by itself (`:desktopApp:buildNative`).

## Windows: MSI

The MSI needs the WiX Toolset 3 (`winget install --id WiXToolset.WiXToolset -e`).

```powershell
cd kmp
.\gradlew.bat :desktopApp:packageMsi
```

Result: `kmp\desktopApp\build\compose\binaries\main\msi\Holdmark-<version>-windows-x64-release.msi`,
an installer (Start-menu and desktop shortcuts, uninstall in Settings ->
Apps); a newer MSI replaces the installed version.

## Ubuntu: DEB

```bash
sudo apt install openjdk-17-jdk ninja-build fakeroot   # or 21
cd kmp
./gradlew :desktopApp:packageDeb
```

Result: `kmp/desktopApp/build/compose/binaries/main/deb/Holdmark-<version>-linux-amd64-release.deb`
(installs to `/opt/holdmark`, menu entry "Holdmark").

```bash
sudo apt install ./kmp/desktopApp/build/compose/binaries/main/deb/Holdmark-*-linux-amd64-release.deb
sudo apt remove holdmark
```

## Data

An update keeps rifles, cartridges, shot logs and settings in
`%APPDATA%\vetalguru\Holdmark\holdmark.db` on Windows,
`~/.local/share/vetalguru/Holdmark/holdmark.db` on Linux (`HOLDMARK_DB`
points the app at another file). On the first start after the rename from
BalCalc the app copies `vetalguru/BalCalc/balcalc.db` there; the old folder
stays untouched.

## Running from the sources

```bash
cd kmp
./gradlew :desktopApp:run            # the app
./gradlew :shared:desktopTest        # UI tests with the real core (screenshots in shared/build/screenshots)
```

## Version

`project(ballistics VERSION x.y.z)` in the top-level `CMakeLists.txt` sets
the version of every package (desktop, Android and `bal-cli`).
