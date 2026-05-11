#!/bin/bash
#
# Author  : Sylvain Deguire (VA2OPS)
# Date    : May 2026
# Purpose : Install QtDashboard and replace et-dashboard autostart.
#

set -e

BINARY="QtDashboard"
INSTALL_DIR="/opt/emcomm-tools/bin"
AUTOSTART_DIR="$HOME/.config/autostart"
ET_DESKTOP="$AUTOSTART_DIR/et-dashboard.desktop"
QT_DESKTOP="$AUTOSTART_DIR/QtDashboard.desktop"

# 1. Install binary (needs root)
echo "Installing $BINARY to $INSTALL_DIR..."
sudo install -m 755 "$(pwd)/$BINARY" "$INSTALL_DIR/$BINARY"

# 2. Kill running et-dashboard and remove autostart + saved sessions
echo "Killing et-dashboard..."
pkill -f et-dashboard || true
pkill -f QtDashboard || true
rm -f "$ET_DESKTOP"
rm -rf "$HOME/.cache/sessions/"
xfconf-query -c xfce4-session -p /general/SaveOnExit -s false 2>/dev/null || true

# 3. Install QtDashboard autostart (runs as current user, no sudo)
echo "Installing autostart entry..."
mkdir -p "$AUTOSTART_DIR"
cat > "$QT_DESKTOP" <<'EOF'
[Desktop Entry]
Type=Application
Name=LiaisonOS Dashboard
Comment=LiaisonOS Interactive Dashboard
Exec=/opt/emcomm-tools/bin/QtDashboard
Icon=emcomm-tools
Terminal=false
X-GNOME-Autostart-enabled=true
X-XFCE-Autostart-enabled=true
StartupNotify=false
NoDisplay=true
EOF

echo "Done. QtDashboard will start automatically on next login."
echo "To start now: $INSTALL_DIR/$BINARY &"
