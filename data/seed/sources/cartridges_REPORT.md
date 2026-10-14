# Factory cartridges: what was collected (2026-10-14)

Step A of adding factory cartridges to the starter library: every row below was
copied from the maker's own page (URL in each row's `source`) by the
`cartridges_<maker>.py` collectors into `cartridges_<maker>.jsonl`. Nothing in
the app reads these files yet. Counts come from `cartridges_report.py`.

## Loads per maker and calibre

| Maker | .223 | 5.56x45 | .243 | 6.5CM | 6.5x55 SE | 6.5 PRC | 7mmRM | .308 | 7.62x51 | .30-06 | .300WM | .300 PRC | .338LM | 7.62x54R | 7.62x39 | 5.45x39 | Total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Federal | 20 | 3 | 12 | 21 | 3 | 5 | 15 | 29 | 1 | 22 | 19 | 2 | 2 |  | 2 |  | **156** |
| Hornady | 22 | 15 | 10 | 14 | 3 | 5 | 8 | 27 | 1 | 17 | 12 | 5 | 4 |  | 6 | 1 | **150** |
| Winchester | 18 | 10 | 9 | 14 | 2 | 7 | 7 | 21 | 2 | 17 | 11 |  | 1 |  | 4 |  | **123** |
| Norma | 9 |  | 5 | 11 | 18 | 3 | 10 | 24 |  | 21 | 15 | 4 | 1 |  |  |  | **121** |
| Sellier & Bellot | 8 |  | 3 | 8 | 11 |  | 7 | 22 |  | 18 | 8 |  | 2 | 4 | 3 | 1 | **95** |
| Nosler | 8 | 2 | 7 | 10 | 4 | 3 | 5 | 14 |  | 14 | 9 |  | 3 |  | 3 |  | **82** |
| Sako | 10 |  | 8 | 9 | 5 | 4 | 5 | 20 |  | 13 | 5 |  | 1 |  | 1 |  | **81** |
| Remington | 17 |  | 4 | 4 | 1 | 1 | 5 | 11 |  | 11 | 2 |  |  |  | 1 |  | **57** |
| PPU | 7 |  | 2 |  | 5 |  |  | 10 |  | 10 | 7 |  | 2 | 6 | 3 |  | **52** |
| Lapua | 3 |  | 2 | 6 | 13 |  |  | 12 |  | 4 | 3 |  | 7 |  |  |  | **50** |
| Berger | 3 |  |  | 8 |  | 3 |  | 7 |  |  | 5 | 3 | 5 |  |  |  | **34** |
| Geco | 3 |  | 2 | 3 | 3 |  | 3 | 7 |  | 5 | 5 |  |  |  |  |  | **31** |
| Barnaul |  | 6 |  |  |  |  |  |  | 5 | 4 |  |  |  | 4 | 4 | 3 | **26** |
| GGG | 4 |  |  |  |  |  |  | 7 |  |  |  |  |  |  |  |  | **11** |
| **Total** | **132** | **36** | **64** | **108** | **68** | **31** | **65** | **211** | **9** | **156** | **101** | **14** | **28** | **14** | **27** | **5** | **1069** |

| Maker | Loads | MV | BC on page | of them G7 | BC via library bullet | No BC | Test barrel |
|---|---:|---:|---:|---:|---:|---:|---:|
| Federal | 156 | 156 | 156 | 34 | 0 | 0 | 0 |
| Hornady | 150 | 132 | 147 | 23 | 0 | 3 | 132 |
| Winchester | 123 | 122 | 116 | 0 | 0 | 7 | 0 |
| Norma | 121 | 111 | 110 | 0 | 0 | 11 | 112 |
| Sellier & Bellot | 95 | 95 | 95 | 95 | 0 | 0 | 95 |
| Nosler | 82 | 67 | 0 | 0 | 71 | 11 | 77 |
| Sako | 81 | 81 | 81 | 0 | 0 | 0 | 0 |
| Remington | 57 | 57 | 57 | 0 | 0 | 0 | 0 |
| PPU | 52 | 52 | 0 | 0 | 0 | 52 | 52 |
| Lapua | 50 | 49 | 49 | 33 | 0 | 1 | 50 |
| Berger | 34 | 34 | 34 | 34 | 0 | 0 | 33 |
| Geco | 31 | 31 | 31 | 0 | 0 | 0 | 31 |
| Barnaul | 26 | 26 | 26 | 0 | 0 | 0 | 0 |
| GGG | 11 | 11 | 11 | 0 | 0 | 0 | 11 |

Usable now (muzzle velocity and a BC): 958 of 1069

"BC via library bullet": the cartridge page has no BC, but the bullet is in
`../published_bullets.json` (matched by diameter, weight and bullet name).
"Usable now" = has a muzzle velocity and a BC (page or library bullet).

## What each maker publishes

| Maker | BC | Velocity | Test barrel | Notes |
|---|---|---|---|---|
| Federal | G1, plus G7 on 34 | fps | not stated | |
| Hornady | G1 and/or G7, labelled | fps | 24" (table header) | the 18 "International" pages have no ballistics table (no MV, no barrel); 8114 prints cm and m/s under the inch and FPS labels |
| Winchester | one BC, unlabelled (G1 by convention) | fps | not stated | |
| Norma | G1 | m/s | 610 mm | ~10 training/indoor loads have no MV or BC |
| Sellier & Bellot | G1 and G7 | m/s | per load (508–600 mm) | |
| Nosler | **none** on ammunition pages | fps | 24" | 71 loads take the BC of their bullet (Nosler's hunting bullets were added to the bullet library for this); 15 pages state no velocity; Solid Base is not sold as a component |
| Sako | one BC, unlabelled | m/s | not stated | grains rounded from the published grams |
| Remington | G1 | fps | not stated | |
| PPU | **none** | m/s | 610 mm | the page gives velocities at 0–300 m, so a BC could be fitted later (would be computed, not published) |
| Lapua | G1 and G7 | m/s | per load | |
| Berger | G1 and G7 | fps | 20–26" | calibre taken from the page address (the "Cartridge" row can run into the site menu) |
| Geco | one BC, unlabelled | m/s | 600 mm | |
| Barnaul | G1, printed without "0." ("381" = 0.381) | fps | not stated | |
| GGG (Lithuania) | one BC, unlabelled | m/s | 508–600 mm (C.I.P. barrel) | page typo "barrel lenght" |

## Not collected

| What | Why |
|---|---|
| RWS | `rws-ammunition.com/robots.txt` disallows ClaudeBot and other AI crawlers. Their pages do show BC, V0 and barrel length; they could be entered by hand. |
| Barnes (VOR-TX) | product specs and ballistics are loaded by script per variant; the static page has none. Reachable only through a browser session. |
| Tula | `tulammo.com` / `tulammo.ru` did not respond. |
| Military loads (7N1, 7N14, LPS, 7N6, 57-N-231) | no primary published source for BC (only unsourced figures). Commercial loads in these calibres are covered by Barnaul, S&B and PPU. |
| GGG military 5.56 / 7.62×51 | pages give velocity at 24 m and no BC. |
| Other calibres | outside the chosen set (e.g. .270, 7mm-08, .300 WSM, 6mm CM, 8×57, 9.3×62). |

## Data-quality notes

- **Same load, several product codes:** 95 rows repeat another row of the same
  maker exactly (calibre, weight, velocity, BC) — bulk packs, box sizes,
  lacquered vs polymer cases. They should become one cartridge in the app
  (keeping all codes).
- Barnaul lists 7.62×39 FMJ 123 gr twice on one page (two case finishes) — one
  `code` repeats.
- Unlabelled BCs (Winchester, Sako, Geco, GGG) are taken as G1; the values
  (0.2–0.6 for these weights) fit G1, not G7.
- Fixed while collecting: makers write "6,5" and "6.5×55" (decimal comma,
  multiplication sign); S&B splits numbers into spans ("0 .259", "1,006").

## Added to the app

`../published_cartridges.json` (built by `../collect_cartridges.py`): **936
factory loads**, every one with a muzzle velocity and a BC. Left out: loads
without either (PPU 52, Hornady 21, Nosler 18, Norma 11, Winchester 7,
Lapua 2), and 21 repeats of the same load under another product code in the
same line with the same test barrel (box sizes; merged, codes kept). Norma 20166512 prints its BC as "428" (read as 0.428).

## Verification

`cartridges_verify.py` re-reads every row's page, independently of the
collectors, and checks that each value is printed there and that the calibre
agrees with the page's title or address. All 1069 rows pass, apart from cases
explained by where the values live:

- Winchester (123): the product pages load their data by script; all 123 rows
  match the catalogue listing they were read from.
- PPU (52): the test barrel is on the calibre search page, not on the cited
  ballistics page (PPU loads are not in the app: no BC).
- Hornady 8114: "61"" barrel and "721" FPS are cm and m/s (see above).
- S&B V334992: its address is ".../detail/223/" (an id), the page is 6.5×55.

Also checked: every value in `published_cartridges.json` equals its source
rows; the 148 Nosler bullets added to the library match their pages; form
factors (BC against sectional density) and velocities against similar loads
are plausible (outliers are subsonic, reduced-recoil, short-barrel loads and
flat-base hunting bullets).

Left out of the app: Hornady 8114 (.30-06 220 gr RN) prints G1 .220, while
Hornady's 220 gr RN bullet is listed at .300, and its page prints metric
values under the inch and FPS labels (`EXCLUDED` in collect_cartridges.py).

## Spot checks against the pages

Hornady 81500 (0.646 G1 / 0.326 G7, 2710 fps, 24"), Federal GM308M
(0.462 / 0.224, 2650 fps), Winchester X65DS (0.54, 2850 fps), Norma Golden
Target .308 155 gr (0.384 G1, 810 m/s, 610 mm), GGG GPX13 (.308 168 gr HPBT,
805 m/s, 600 mm, 0.462), Barnaul 7.62×54R FMJ 148 gr (2641 fps, 0.381) — all
match.
