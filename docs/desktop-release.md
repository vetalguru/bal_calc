# Desktop packages

The desktop app is the Kotlin app in `kmp/` (Compose Multiplatform for the
JVM). Packages carry their own Java runtime and the C++ core
(`balcalc_jni`), so nothing else needs installing.

Requirements: JDK 17, CMake, and a C++ compiler — Visual Studio 2022 on
Windows, GCC and Ninja on Linux. Gradle comes with the wrapper; it builds the
C++ core by itself (`:desktopApp:buildNative`).

## Windows: MSI and ZIP

The MSI needs the WiX Toolset 3 (`winget install --id WiXToolset.WiXToolset -e`).

```powershell
cd kmp
.\gradlew.bat :desktopApp:packageMsi :desktopApp:packageZip
```

Results in `kmp\desktopApp\build\compose\binaries\main\`:

- `msi\BalCalc-<version>.msi` - installer (Start-menu and desktop shortcuts,
  uninstall in Settings -> Apps); a newer MSI replaces the installed version;
- `zip\BalCalc-<version>-windows-x64.zip` - portable: unpack anywhere and run
  `BalCalc\BalCalc.exe`.

## Ubuntu: DEB and ZIP

```bash
sudo apt install openjdk-17-jdk ninja-build fakeroot   # or 21
cd kmp
./gradlew :desktopApp:packageDeb :desktopApp:packageZip
```

Results in `kmp/desktopApp/build/compose/binaries/main/`:
`deb/balcalc_<version>_amd64.deb` (installs to `/opt/balcalc`, menu entry
"BalCalc") and `zip/BalCalc-<version>-linux-x64.zip`.

```bash
sudo apt install ./kmp/desktopApp/build/compose/binaries/main/deb/balcalc_*_amd64.deb
sudo apt remove balcalc
```

## Data

The database is the same file the Qt version used, so an update keeps
rifles, cartridges, shot logs and settings:
`%APPDATA%\vetalguru\BalCalc\balcalc.db` on Windows,
`~/.local/share/vetalguru/BalCalc/balcalc.db` on Linux (`BALCALC_DB`
points the app at another file).

## Running from the sources

```bash
cd kmp
./gradlew :desktopApp:run            # the app
./gradlew :shared:desktopTest        # UI tests with the real core (screenshots in shared/build/screenshots)
```

## Version

`project(ballistics VERSION x.y.z)` in the top-level `CMakeLists.txt` sets
the version of every package (desktop, Android and `bal-cli`).
