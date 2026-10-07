# Holdmark

[![CI](https://github.com/vetalguru/bal_calc/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/vetalguru/bal_calc/actions/workflows/ci.yml)

Ballistic calculator for small arms out to 2500 m — Windows, Ubuntu and
Android, in Ukrainian, Russian and English.

- **Physics** — point-mass trajectory (adaptive Dormand–Prince 5(4)),
  CIPM-2007 moist air, G1/G2/G5/G6/G7/G8/GI/GS/RA4, velocity-banded BCs and
  Doppler-radar drag curves, wind zones, shot angle and cant,
  Coriolis/Eötvös, Miller stability, spin drift and aerodynamic jump,
  powder temperature sensitivity. Checked against an independent
  reference to 0.01 MRAD at 2500 m.
- **App** — firing solution with turret clicks and reticle holds (FFP/SFP),
  range table and trajectory chart, rifles (with scope and zero) and
  cartridges as separate lists, one cartridge usable in several rifles, bullet
  library (Lapua radar curves, manufacturer BCs, BallisticCalculator data),
  shot log and truing per rifle + cartridge, JSON sharing of rifles and
  cartridges.
- **Storage** — SQLite through
  [sqlite_manager](https://github.com/vetalguru/sqlite_manager) (submodule).

## Layout

| Directory | What |
|---|---|
| `core/` | ballistics engine, plain C++17 |
| `storage/` | SQLite schema, migrations, repositories, firing solution |
| `applogic/` | toolkit-free screen logic: forms, truing, importers, reticle holds |
| `bridge/` | JSON facade over applogic, the one entry point of the apps |
| `jni/` | `libbalcalc_jni`: the C++ core for the Kotlin app |
| `kmp/` | the app: Kotlin, Compose Multiplatform (Android, Windows, Linux), icons |
| `cli/` | `bal-cli` — range cards and data import from the command line |
| `tests/` | GoogleTest suites (UI tests: `kmp/shared/src/desktopTest`) |
| `data/seed/` | bundled starter library and its licences |
| `docs/` | release builds for desktop and Android |

## Build

Clone with the submodule: `git clone --recursive https://github.com/vetalguru/bal_calc.git`.

**C++ core, CLI and tests** on Ubuntu (GCC, CMake, Ninja):

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

On Windows (Visual Studio 2022):

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

**The app** (JDK 17; Gradle builds the C++ core itself; Android needs the SDK
with NDK 26.1.10909125 and CMake 3.31.6):

```bash
cd kmp
./gradlew :desktopApp:run                # desktop app
./gradlew :shared:desktopTest            # UI tests, screenshots in shared/build/screenshots
./gradlew :androidApp:assembleDebug      # Android APK
```

Packages and signed releases: [`docs/desktop-release.md`](docs/desktop-release.md),
[`docs/android-release.md`](docs/android-release.md).

## Command line

```bash
bal-cli --db my.db demo
bal-cli --db my.db rifles
bal-cli --db my.db cartridges
bal-cli --db my.db table --rifle 1 --cartridge 1 --to 1200 --temp -5 --alt 400 --wind 4@3h
bal-cli quick --bc 0.243 --drag G7 --v0 800 --to 1000 --step 100
```

## Licence

GPL-3.0 (see `LICENSE`). Bundled data keep their own licences
(`data/seed/README.md`).
