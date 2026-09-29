#!/bin/sh
#
# Builds Prince of Persia and installs it into ~/config/non-packaged, with an
# entry in the Deskbar's Applications menu. Run this on the Haiku machine.
#
# Usage:
#   ./install.sh              build and install
#   ./install.sh --uninstall  remove it again
#   ./install.sh --build-only build in place, install nothing

set -e

APP="PrinceOfPersia"
MENU_NAME="Prince of Persia"

APPS_DIR="$HOME/config/non-packaged/apps/$MENU_NAME"
# Deskbar follows the search list in /boot/system/data/deskbar/menu_entries;
# this is the user-writable directory in that list.
MENU_DIR="$HOME/config/non-packaged/data/deskbar/menu/Applications"
DESKTOP_DIR="$HOME/Desktop"

cd "$(dirname "$0")"

if [ "$(uname -s)" != "Haiku" ]; then
	echo "This builds a Haiku application; run it on Haiku." >&2
	exit 1
fi

if [ "$1" = "--uninstall" ]; then
	quit application/x-vnd.rainygirl-princeofpersia >/dev/null 2>&1 || true
	rm -f "$MENU_DIR/$MENU_NAME" "$DESKTOP_DIR/$MENU_NAME"
	# Keep save games and settings unless asked to remove everything.
	rm -f "$APPS_DIR/$APP"
	rm -rf "$APPS_DIR/data"
	echo "Removed $MENU_NAME."
	exit 0
fi

# The x86_gcc2 hybrid needs the modern secondary toolchain for C99 and the C++
# platform layer; on x86_64 the default compiler is already the right one.
if [ "$(getarch)" = "x86_gcc2" ]; then
	echo "Compiling (setarch x86)..."
	setarch x86 make
else
	echo "Compiling..."
	make
fi

if [ "$1" = "--build-only" ]; then
	echo "Built build/$APP."
	exit 0
fi

echo "Installing into $APPS_DIR..."
# Stop a running copy first, so the binary is not replaced underneath it.
quit application/x-vnd.rainygirl-princeofpersia >/dev/null 2>&1 || true
mkdir -p "$APPS_DIR" "$MENU_DIR" "$DESKTOP_DIR"
cp -f "build/$APP" "$APPS_DIR/$APP"

# Tracker reads the icon from the file's attributes, not from the resources the
# linker wrote into the binary. mimeset is meant to copy one to the other, but
# recent Haiku no longer sniffs ELF files, so a freshly installed app shows a
# blank document icon until resattr does the copy.
resattr -O -o "$APPS_DIR/$APP" "$APPS_DIR/$APP" 2>/dev/null \
	|| echo "install.sh: resattr failed; the icon may show as a blank document" >&2
cp -r data "$APPS_DIR/"
# The settings file is the user's once installed; only put the default in place.
[ -f "$APPS_DIR/SDLPoP.ini" ] || cp SDLPoP.ini "$APPS_DIR/"
mkdir -p "$APPS_DIR/mods" "$APPS_DIR/replays" "$APPS_DIR/screenshots"
mimeset -f "$APPS_DIR/$APP"

ln -sf "$APPS_DIR/$APP" "$MENU_DIR/$MENU_NAME"
ln -sf "$APPS_DIR/$APP" "$DESKTOP_DIR/$MENU_NAME"

echo "Done. $MENU_NAME is on the Desktop and in Deskbar -> Applications."
echo "Save games and settings are kept in $APPS_DIR."
