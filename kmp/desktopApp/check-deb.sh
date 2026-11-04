#!/bin/sh
# Checks the Linux package as it ships: its control data, its files, that the
# core library finds what it needs, and that the app starts from it, makes
# its database and fills the library.
#   check-deb.sh <the .deb> <the repository>
# The app needs a display: DISPLAY, or xvfb-run when there is none (CI).
set -eu

deb=$1
repo=$2
version=$(sed -n 's/^project(ballistics VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt")
work=$(mktemp -d)
app=""
trap 'if [ -n "$app" ]; then kill "$app" 2>/dev/null || true; fi; rm -rf "$work"' EXIT

fail() {
    echo "check-deb: $*" >&2
    exit 1
}
field() { dpkg-deb -f "$deb" "$1"; }

# --- Control data -------------------------------------------------------------
[ "$(field Package)" = holdmark ] || fail "package is $(field Package), not holdmark"
# Some JDKs' jpackage adds a Debian revision (1.0.0-1).
[ "$(field Version | cut -d- -f1)" = "$version" ] || fail "version is $(field Version), the sources say $version"
[ "$(field Architecture)" = amd64 ] || fail "architecture is $(field Architecture)"
case "$(basename "$deb")" in
Holdmark-"$version"-linux-amd64-release.deb) ;;
*) fail "file name $(basename "$deb") is not Holdmark-$version-linux-amd64-release.deb" ;;
esac

# --- Files --------------------------------------------------------------------
dpkg-deb -x "$deb" "$work/root"
opt=$work/root/opt/holdmark
res=$opt/lib/app/resources
[ -x "$opt/bin/Holdmark" ] || fail "no launcher bin/Holdmark"
[ -f "$res/libbalcalc_jni.so" ] || fail "no libbalcalc_jni.so in the resources"
[ -f "$res/balcalc.png" ] || fail "no window icon in the resources"
shipped=$(find "$res/seed" -type f | wc -l)
expected=$(cd "$repo/data/seed" && find . -type f \( -name '*.ammo' -o -name '*.drg' -o \
    -name '*.reticle' -o -name '*.json' \) -not -path './sources/*' | wc -l)
[ "$shipped" -eq "$expected" ] || fail "$shipped seed files shipped, data/seed has $expected"

# The menu entry (freedesktop.org): what it starts, its icon, where it sits.
entry=$opt/lib/holdmark-Holdmark.desktop
[ -f "$entry" ] || fail "no menu entry"
grep -qx 'Type=Application' "$entry" || fail "menu entry: not an application"
grep -qx 'Name=Holdmark' "$entry" || fail "menu entry: wrong name"
grep -qx 'Exec=/opt/holdmark/bin/Holdmark' "$entry" || fail "menu entry: wrong Exec"
icon=$(sed -n 's/^Icon=//p' "$entry")
[ -f "$work/root$icon" ] || fail "menu entry: icon $icon is not in the package"
grep -Eqx 'Categories=Science;([A-Za-z]+;)*' "$entry" || fail "menu entry: Categories must be Science;...; ($(grep Categories "$entry"))"
dpkg-deb -e "$deb" "$work/control"
grep -q "xdg-desktop-menu install /opt/holdmark/lib/holdmark-Holdmark.desktop" "$work/control/postinst" ||
    fail "postinst does not install the menu entry"

# Everything the core library links against is on this system.
if ldd "$res/libbalcalc_jni.so" | grep -q 'not found'; then
    ldd "$res/libbalcalc_jni.so" >&2
    fail "libbalcalc_jni.so: missing libraries"
fi

# --- The app starts -----------------------------------------------------------
db=$work/data/holdmark.db
mkdir -p "$work/data"
run="env HOLDMARK_DB=$db $opt/bin/Holdmark"
if [ -z "${DISPLAY:-}" ]; then
    command -v xvfb-run >/dev/null || fail "no DISPLAY and no xvfb-run"
    run="xvfb-run -a $run"
fi
$run >"$work/app.log" 2>&1 &
app=$!

# The starter library in the database: the core loaded, the seed read.
bullets=0
for _ in $(seq 120); do
    kill -0 "$app" 2>/dev/null || { cat "$work/app.log" >&2; fail "the app quit while starting"; }
    if [ -f "$db" ]; then
        bullets=$(python3 -c "import sqlite3,sys
try: print(sqlite3.connect('file:$db?mode=ro', uri=True).execute('select count(*) from bullet').fetchone()[0])
except Exception: print(0)")
        [ "$bullets" -gt 0 ] && break
    fi
    sleep 0.5
done
[ "$bullets" -gt 0 ] || { cat "$work/app.log" >&2; fail "no library in $db after 60 s"; }
# Still up a moment later, with nothing thrown. Without OpenGL (xvfb, WSL)
# Skiko says it falls back to software drawing: that one is expected.
sleep 3
kill -0 "$app" 2>/dev/null || { cat "$work/app.log" >&2; fail "the app quit after starting"; }
if grep -v 'org.jetbrains.skiko.RenderException: Cannot create Linux GL context' "$work/app.log" |
    grep -q 'Exception'; then
    cat "$work/app.log" >&2
    fail "the app logged an exception"
fi
echo "check-deb: $(basename "$deb") is fine ($shipped seed files, $bullets bullets in a new database)"
