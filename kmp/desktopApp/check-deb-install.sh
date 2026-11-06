#!/bin/sh
# Installs and removes the DEB with apt: first as on a server, with no desktop
# menu, then as on a desktop, where the menu entry comes and goes with it.
# Changes the system, so it is for a throwaway machine (CI); needs sudo.
#   check-deb-install.sh <the .deb>
set -eu

deb=$(realpath "$1")
# xdg-desktop-menu puts it in the first writable applications folder of these.
entries="/usr/local/share/applications/holdmark-Holdmark.desktop /usr/share/applications/holdmark-Holdmark.desktop"

fail() {
    echo "check-deb-install: $*" >&2
    exit 1
}
entry() { for e in $entries; do [ -f "$e" ] && echo "$e"; done; true; }
installed() { [ "$(dpkg-query -W -f '${Status}' holdmark 2>/dev/null)" = "install ok installed" ]; }

installed && fail "holdmark is already installed on this machine"
for dir in /usr/share/desktop-directories /etc/xdg/menus; do
    [ -d "$dir" ] && fail "$dir exists: this is not a machine without a desktop menu"
done

# --- No desktop menu ------------------------------------------------------------
sudo apt-get install -y "$deb"
installed || fail "without a desktop menu: not installed"
[ -x /opt/holdmark/bin/Holdmark ] || fail "without a desktop menu: no launcher"
sudo apt-get remove -y holdmark
[ -e /opt/holdmark/bin/Holdmark ] && fail "without a desktop menu: the launcher stayed after removing"

# --- A desktop ------------------------------------------------------------------
sudo mkdir -p /usr/share/desktop-directories /etc/xdg/menus
trap 'sudo rmdir /usr/share/desktop-directories /etc/xdg/menus 2>/dev/null || true' EXIT
sudo apt-get install -y "$deb"
installed || fail "on a desktop: not installed"
entry=$(entry)
[ -n "$entry" ] || fail "on a desktop: no menu entry in $entries"
sudo apt-get remove -y holdmark
[ -e "$entry" ] && fail "on a desktop: the menu entry $entry stayed after removing"

echo "check-deb-install: $(basename "$deb") installs and removes with and without a desktop menu ($entry)"
