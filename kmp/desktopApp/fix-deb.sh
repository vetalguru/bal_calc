#!/bin/sh
# Lets the DEB install where there is no desktop menu (a server, WSL, a
# container): jpackage's postinst and prerm run xdg-desktop-menu under set -e,
# and it fails there, which leaves the package half-configured. The menu entry
# is then skipped with a note; on a desktop nothing changes.
#   fix-deb.sh <the .deb>   (rewrites it in place)
set -eu

deb=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() {
    echo "fix-deb: $*" >&2
    exit 1
}

dpkg-deb -R "$deb" "$work/root"
for script in postinst prerm; do
    file=$work/root/DEBIAN/$script
    grep -q '^xdg-desktop-menu ' "$file" || fail "no xdg-desktop-menu line in $script: has jpackage changed it?"
    sed -i 's|^\(xdg-desktop-menu .*\)$|\1 \|\| echo "holdmark: no desktop menu here, so no menu entry" >\&2|' "$file"
done

# The same compression as jpackage's, and every file root's as before.
case "$(ar t "$deb" | grep '^data\.tar')" in
data.tar.zst) compression=zstd ;;
data.tar.xz) compression=xz ;;
data.tar.gz) compression=gzip ;;
*) fail "unknown data archive in $deb" ;;
esac
dpkg-deb --root-owner-group -Z"$compression" -b "$work/root" "$work/fixed.deb" >/dev/null
mv "$work/fixed.deb" "$deb"
echo "fix-deb: $(basename "$deb") installs without a desktop menu too"
