# BalCalc

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
| `app/` | Qt 6 Quick application (QML), translations, icons, Android files |
| `cli/` | `bal-cli` — range cards and data import from the command line |
| `tests/` | GoogleTest suites and Qt Quick UI tests (`tests/ui`) |
| `data/seed/` | bundled starter library and its licences |
| `docs/` | release builds for desktop and Android |

## Build

Clone with the submodule: `git clone --recursive https://github.com/vetalguru/bal_calc.git`.

**Ubuntu** (GCC, Qt 6.8+ from the distribution, see `docs/desktop-release.md`):

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

**Windows** (Visual Studio 2022, Qt 6.9.2 `msvc2022_64`):

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

**Android** (Qt 6.9.2 `android_arm64_v8a`, SDK 35, NDK r26b, JDK 17):

```powershell
cmake --preset android-arm64-debug
cmake --build --preset android-arm64-debug
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
