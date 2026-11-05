"""Writes the app's UI icons as vector drawables, from Material Symbols
(Outlined, weight 400, 24 px; Apache-2.0, see LICENSE-material-symbols.txt).

    python make_ui_icons.py

Add an icon's name to ICONS and run this again; only stdlib is needed.
"""

import re
import urllib.request
from pathlib import Path

# The icons' source, pinned: a run gives the same files.
COMMIT = "10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7"
URL = (
    "https://raw.githubusercontent.com/google/material-design-icons/"
    + COMMIT
    + "/symbols/web/{0}/materialsymbolsoutlined/{0}_24px.svg"
)
LICENSE_URL = f"https://raw.githubusercontent.com/google/material-design-icons/{COMMIT}/LICENSE"

ICONS = [
    # Navigation.
    "my_location", "table_chart", "air", "inventory_2", "settings",
    # Actions.
    "add", "adjust", "arrow_back", "bookmarks", "calculate", "close", "compare_arrows",
    "delete", "edit", "file_open", "fullscreen", "image", "ios_share", "library_books",
    "more_vert", "photo_camera", "qr_code_2", "qr_code_scanner", "remove", "restart_alt",
    "save", "scatter_plot", "timer", "undo", "view_column",
    # Sensors.
    "explore", "speed", "straighten",
    # In place of symbols from the font.
    "arrow_drop_down", "arrow_drop_up", "chevron_left", "chevron_right", "content_paste", "info",
]

# The symbols' viewBox is "0 -960 960 960": the group moves it into the viewport.
TEMPLATE = """<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="24dp"
    android:height="24dp"
    android:viewportWidth="960"
    android:viewportHeight="960">
    <group android:translateY="960">
        <path
            android:fillColor="#FF000000"
            android:pathData="{path}" />
    </group>
</vector>
"""


def fetch(url):
    with urllib.request.urlopen(url) as reply:
        return reply.read().decode()


def main():
    here = Path(__file__).parent
    out = here.parent / "shared/src/commonMain/composeResources/drawable"
    out.mkdir(parents=True, exist_ok=True)
    for name in ICONS:
        svg = fetch(URL.format(name))
        paths = re.findall(r'<path d="([^"]+)"', svg)
        assert len(paths) == 1, f"{name}: {len(paths)} paths"
        (out / f"ic_{name}.xml").write_text(TEMPLATE.format(path=paths[0]), newline="\n")
    (here / "LICENSE-material-symbols.txt").write_text(fetch(LICENSE_URL), newline="\n")
    print(f"{len(ICONS)} icons in {out}")


if __name__ == "__main__":
    main()
