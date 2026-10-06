# Android release build

The Android app is the Kotlin app in `kmp/` (Compose Multiplatform); Gradle
builds the C++ core with CMake and the NDK.

Requirements: JDK 17, Android SDK with platform 37 (`android-37.0`),
NDK r26b (26.1.10909125) and CMake 3.31.6 from the SDK manager, with
`ANDROID_SDK_ROOT` (or `ANDROID_HOME`) and `JAVA_HOME` set. Gradle itself
comes with the wrapper.

## Quick way: `tools/android-release.ps1`

```powershell
.\tools\android-release.ps1 -Setup     # once: create the key (or point to an existing one)
.\tools\android-release.ps1            # signed APK, signature checked
.\tools\android-release.ps1 -Install   # ... and install on the phone over adb
.\tools\android-release.ps1 -Aab       # ... plus the Google Play bundle
```

The script keeps its settings in `%APPDATA%\BalCalc\android-signing.json`:
keystore path, alias and the password encrypted with Windows DPAPI (only your
Windows account on this PC can decrypt it). Nothing goes into the repository.
Back up the keystore file and remember its password: an app update must be
signed with the same key.

Output: `kmp/androidApp/build/outputs/apk/release/androidApp-release.apk`
(and `bundle/release/androidApp-release.aab`). Copy the APK to the phone
(e.g. through Google Drive) or install it with `-Install`.

## By hand

Signing is read from the environment by `kmp/androidApp/build.gradle.kts`
(without it the release APK is unsigned):

```powershell
$env:BALCALC_KEYSTORE_PATH = "$env:USERPROFILE\balcalc-release.keystore"
$env:BALCALC_KEYSTORE_ALIAS = "balcalc"
$env:BALCALC_KEYSTORE_PASSWORD = Read-Host "Keystore password"
cd kmp
.\gradlew.bat :androidApp:assembleRelease --no-configuration-cache
```

`--no-configuration-cache` keeps the password out of Gradle's cache.
Check the signature:

```powershell
& "$env:ANDROID_SDK_ROOT\build-tools\36.0.0\apksigner.bat" verify --print-certs <apk>
```

## Debug builds

`.\gradlew.bat :androidApp:assembleDebug` gives a debug-signed APK for
emulators and testing (`kmp/androidApp/build/outputs/apk/debug/`).

## Notes

- The app has the same `applicationId` (`org.vetalguru.balcalc`) as the Qt
  version and installs over it when signed with the same key: its database
  (`files/balcalc.db`), selection, units and language carry over.
- Version: `versionName` follows `project(VERSION)` of the top-level
  `CMakeLists.txt`; `versionCode` in `kmp/androidApp/build.gradle.kts` must
  grow with every release uploaded to Google Play.
- The app needs no Android permissions: files are opened and saved through
  the system document picker.
