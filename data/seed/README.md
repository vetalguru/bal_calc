# Starter library

Bundled with the app and imported into a new database on first start
(`applogic::SeedLibrary`). Refresh the third-party files with
`python collect_seed.py [path/to/Data/libs]`.

| Folder / file | Content | Origin and licence |
|---|---|---|
| `ammo/*.ammo` | 69 factory cartridges (bullet, BC, muzzle velocity, barrel length) | [BallisticCalculator](https://github.com/gehtsoft-usa/BallisticCalculator1) legacy data, LGPL-2.1 (`LICENSE-BallisticCalculator.txt`) |
| `reticle/*.reticle` | 4 reticle drawings (Mil-Dot, Chevron, BDC, Segmented) | BallisticCalculator, LGPL-2.1 |
| `reticle/generic-*.reticle` | 10 generic reticles: MRAD hash 0.5 / 0.2, MRAD tree 0.2 / 0.5, MRAD grid 0.5, MOA hash 1 / 2, MOA tree 2, MOA grid 2, Duplex | Ours, written by `make_reticles.py`. No maker's drawing: only the mark spacing a hold is read against (makers publish their drawings as pictures). A test checks every mark sits on a whole step. |
| `drg/*.drg` | 1 Doppler-radar drag function: McCoy's .308 Sierra 168 gr | McCoy, *Modern Exterior Ballistics*. Lapua's radar curves are **not** bundled: Lapua gave no general permission to redistribute them (only to Ballistic Explorer); users import the `.drg` files they download from Lapua. |
| `published_bullets.json` | 366 bullets with manufacturer-published BCs: Nosler 153, Berger 103 (2025 catalog), Hornady 57 (Doppler G7 BCs at Mach 2.25/2.0/1.75), Lapua 51, Sierra 2 | Built by `collect_bullets.py` from the makers' catalogs and product pages (`sources/`: the values as copied, with each page's address), checked 2026-10; every bullet carries its source in `reference`. |
| `published_cartridges.json` | 937 factory loads (muzzle velocity, test barrel where stated, the load's bullet with its BC) in .223 / 5.56, .243, 6.5 CM, 6.5×55, 6.5 PRC, 7mm RM, .308 / 7.62×51, .30-06, .300 WM, .300 PRC, .338 LM, 7.62×54R, 7.62×39, 5.45×39: Federal 151, Hornady 123, Winchester 115, Norma 109, Sellier & Bellot 95, Sako 80, Nosler 64, Remington 52, Lapua 48, Berger 34, Geco 31, Barnaul 24, GGG 11 | Built by `collect_cartridges.py` from `sources/cartridges_<maker>.jsonl`, each row copied from the maker's page by `sources/cartridges_<maker>.py` 2026-10 (the page in `reference`). Coverage and gaps: `sources/cartridges_REPORT.md`. Library cartridges ("From library"); each load's bullet stays with it and is not listed among library bullets. |
| `published_scopes.json` | 50 scope models: Nightforce 24, Schmidt & Bender 4, Kahles 7, Leupold 6, Athlon 6, Steiner 3 - magnification, focal plane, the click values each is sold with, reticles | Read-only catalog for "From library" in the rifle editor (not database records). Built by `collect_optics.py` from `sources/scopes_*.jsonl`, copied from the makers' product pages 2026-10; every model carries its page in `source`. |
| `published_rifles.json` | 126 rifle variants (maker, model, calibre, twist; barrel lengths merged): Tikka T3x TACT A1, CTR, Varmint, Super Varmint, Ace Target; Sako TRG; Bergara B-14 and Premier; Ruger Precision Rifle | Same, from `sources/rifles.psv` (one row per published variant). |

Data quality notes:

- Berger: the BCs and lengths come from the 2025 catalog's specification
  table, which corrects three typos of its product tables (#22418 G7 0.374 →
  0.191, #30107 the .338 250 gr BCs on the .308 185 gr Juggernaut OTM
  Tactical → 0.552 / 0.283, #30570 G1 0.498 → 0.489).
- Two BallisticCalculator files carry another cartridge's name
  (`22LR/.22LR 40gr Eley.ammo` says ".22LR 40gr Federal", `7.62x39 196gr SS`
  says "7.62x39 125gr FMJ"); `collect_seed.py` names them after their files.
- Published BCs are averages (Hornady publishes three by velocity, used as
  bands); a radar drag curve describes
  the bullet better over the whole velocity range. Always confirm with
  live fire (truing).

Scopes and rifles:

- Only what the maker's own page shows. Vortex, Zeiss, Swarovski and Sig
  load their specifications by script and are not collected yet; nor are
  rifle pages without a twist (Tikka T3x Hunter and Lite, UPR).
- The click value follows the turret the scope is sold with; one row per
  option in the picker. A second-focal-plane reticle is taken as true at the
  highest power unless the maker states another (Nightforce NX8 4-32x50:
  32x); check the reticle manual.
- Tikka's variant tables were read row by row with their item numbers; where
  a summary and the rows disagreed, only the rows were kept.

```bash
cd data/seed && python3 collect_optics.py
```

Rebuilding `published_bullets.json`:

```bash
cd data/seed/sources
python3 lapua_to_json.py && python3 hornady_to_json.py && python3 nosler_to_json.py
cd ..
pdftotext -layout Berger-2025-catalog.pdf berger.txt   # the yearly catalog
python3 collect_bullets.py berger.txt sources/lapua.json sources/hornady.json sources/nosler.json
```

Nosler's bullets: `python3 sources/nosler_bullets.py` adds the ones missing
from `sources/nosler_pages.jsonl`; then `nosler_to_json.py` and
`python3 collect_bullets.py sources/nosler.json` (the other makers are kept).

Rebuilding `published_cartridges.json`:

```bash
cd data/seed/sources
python3 cartridges_hornady.py   # and the other cartridges_<maker>.py
python3 cartridges_report.py    # counts for cartridges_REPORT.md
cd ..
python3 collect_cartridges.py
```

`collect_cartridges.py` leaves out a load without a muzzle velocity or a BC
(Nosler loads take the BC of their Nosler bullet in `published_bullets.json`),
merges a load sold under several product codes (same line, numbers and test barrel), and drops a G7 BC whose
G7/G1 ratio no real bullet has. RWS is not collected: its `robots.txt`
disallows AI crawlers.

`collect_bullets.py` keeps hand-collected bullets of makers it was not given
(the two Sierra entries), puts the calibre first in every name (makers reuse
model names across calibres) and drops a G7 BC whose G7/G1 ratio no real
bullet has; for Berger it takes BCs and lengths from the specification
table and reports where the product tables differ.
Sierra publishes its BCs only in pages that load them by script; Sierra and
other makers can be added the same way as `sources/*.json`.
