# Starter library

Bundled with the app and imported into a new database on first start
(`applogic::SeedLibrary`). Refresh the third-party files with
`python collect_seed.py [path/to/Data/libs]`.

| Folder / file | Content | Origin and licence |
|---|---|---|
| `ammo/*.ammo` | 69 factory cartridges (bullet, BC, muzzle velocity, barrel length) | [BallisticCalculator](https://github.com/gehtsoft-usa/BallisticCalculator1) legacy data, LGPL-2.1 (`LICENSE-BallisticCalculator.txt`) |
| `reticle/*.reticle` | 4 reticle drawings (Mil-Dot, Chevron, BDC, Segmented) | BallisticCalculator, LGPL-2.1 |
| `drg/*.drg` | 55 Doppler-radar drag functions (Cd vs Mach): Lapua bullets and McCoy's .308 Sierra 168 gr | Lapua radar data as distributed with Exterior Ballistics / Lapua software; McCoy, *Modern Exterior Ballistics*. Encoded `.drg` files and artillery projectiles are not included. |
| `published_bullets.json` | 38 bullets with manufacturer-published G1/G7 BCs (Berger, Hornady, Sierra) | Manufacturer catalogs and product pages, checked 2026-10; the source of every value is in its `reference`. |

Data quality notes:

- The Berger 2025 catalog's component table repeats the .338 250 gr BCs for
  the .308 185 gr Juggernaut OTM Tactical; the value used here (G1 0.552 /
  G7 0.283) is the one the same catalog gives for its ammunition.
- Two BallisticCalculator files carry another cartridge's name
  (`22LR/.22LR 40gr Eley.ammo` says ".22LR 40gr Federal", `7.62x39 196gr SS`
  says "7.62x39 125gr FMJ"); `collect_seed.py` names them after their files.
- Published BCs are averages; a radar drag curve (the `drg` set) describes
  the bullet better over the whole velocity range. Always confirm with
  live fire (truing).
