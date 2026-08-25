#!/bin/bash
# Build a macOS .app bundle for typyst
set -e

NAME="typyst"
APP_DIR="${NAME}.app"
CONTENTS="${APP_DIR}/Contents"
MACOS="${CONTENTS}/MacOS"
RESOURCES="${CONTENTS}/Resources"
BINARY="_build/typyst"

cd "$(dirname "$0")"

# Ensure the binary exists
if [ ! -f "$BINARY" ]; then
    echo "Building typyst first..."
    make release
fi

# Create directory structure
rm -rf "$APP_DIR"
mkdir -p "$MACOS" "$RESOURCES"

# Copy the binary into the app
cp "$BINARY" "${MACOS}/typyst-bin"

# Launcher — runs the binary with transparency and auto-starts tmux
cat > "${MACOS}/typyst" << 'LAUNCHER'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$HOME/.local/bin"
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8
cd "$HOME"

# Animated background
BG_FILE="${HOME}/Videos/bgs/$(ls ${HOME}/Videos/bgs/ | sort -R | head -1)"
BG_ARGS=()
if [ -f "$BG_FILE" ]; then
    BG_ARGS=(-a "$BG_FILE")
fi

exec "$DIR/typyst-bin" -p 60 -t 0.80 "${BG_ARGS[@]}" /bin/zsh -c \
    'clear; tmux attach 2>/dev/null || tmux new; while tmux has-session 2>/dev/null; do tmux attach 2>/dev/null; sleep 1; done'
LAUNCHER
chmod +x "${MACOS}/typyst"

# Info.plist
cat > "${CONTENTS}/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>typyst</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleIdentifier</key>
    <string>com.pixzor.typyst</string>
    <key>CFBundleName</key>
    <string>${NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>${NAME}</string>
    <key>CFBundleVersion</key>
    <string>0.1</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
    <key>CFBundleIconFile</key>
    <string>icon</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSCalendarsUsageDescription</key>
    <string>typyst needs calendar access so terminal tools (like ical) can read and manage your calendars.</string>
    <key>NSCalendarsFullAccessUsageDescription</key>
    <string>typyst needs full calendar access so terminal tools (like ical) can read and manage your calendars.</string>
    <key>NSCalendarsWriteAccessUsageDescription</key>
    <string>typyst needs to write to your calendars so terminal tools (like ical) can create and edit events.</string>
</dict>
</plist>
PLIST

# Use little-typewriters image as icon
ICON_SRC="icon.png"
if [ -f "$ICON_SRC" ]; then
    # PNG icon (as requested)
    sips -z 512 512 "$ICON_SRC" --out "${RESOURCES}/icon.png" &>/dev/null
    # ICNS icon for full macOS support
    ICONSET="${RESOURCES}/icon.iconset"
    mkdir -p "$ICONSET"
    for size in 16 32 64 128 256 512; do
        sips -z $size $size "$ICON_SRC" --out "${ICONSET}/icon_${size}x${size}.png" &>/dev/null
        if [ $size -le 256 ]; then
            sips -z $((size*2)) $((size*2)) "$ICON_SRC" --out "${ICONSET}/icon_${size}x${size}@2x.png" &>/dev/null
        fi
    done
    iconutil -c icns "$ICONSET" -o "${RESOURCES}/icon.icns" 2>/dev/null || true
    rm -rf "$ICONSET"
fi

# Sign with a stable identity so macOS TCC treats typyst as a proper
# responsible app (required for permission prompts like Calendar/Contacts).
# Using the 'Typyst Dev' self-signed cert (see setup-codesign.sh) means Full
# Disk Access grants survive rebuilds — they are keyed to the cert, not the
# binary hash. Falls back to ad-hoc if the cert is missing.
SIGNER=""
# Note: no -v flag — self-signed certs show up as 'not trusted' with -v,
# but codesign still accepts them for local use.
if security find-identity -p codesigning 2>/dev/null | grep -q '"Typyst Dev"'; then
    SIGNER="Typyst Dev"
else
    echo "warning: 'Typyst Dev' identity not found — using ad-hoc signing."
    echo "        TCC grants (Full Disk Access) will break on every rebuild."
    echo "        Run: sh setup-codesign.sh  (one-time)"
fi

if [ -n "$SIGNER" ]; then
    codesign --force --deep --sign "$SIGNER" "${APP_DIR}" || { echo "error: codesign failed"; exit 1; }
else
    codesign --force --deep --sign - "${APP_DIR}" || echo "warning: codesign failed, continuing"
fi

# Copy to /Applications so Spotlight/Launchpad can find it.
# TCC grants (Full Disk Access etc.) are keyed to the code signature, so a
# re-sign invalidates them. Skip the copy when the binary is unchanged.
INSTALL_BIN="/Applications/${APP_DIR}/Contents/MacOS/typyst"
NEW_HASH=$(md5 -q "${APP_DIR}/Contents/MacOS/typyst-bin")
OLD_HASH=$( [ -f "$INSTALL_BIN" ] && md5 -q "$INSTALL_BIN" )

if [ "$NEW_HASH" = "$OLD_HASH" ]; then
    echo "Binary unchanged, skipping /Applications update (TCC grants preserved)"
else
    rm -rf "/Applications/${APP_DIR}"
    cp -R "${APP_DIR}" "/Applications/${APP_DIR}"
    if [ -n "$SIGNER" ]; then
        codesign --force --deep --sign "$SIGNER" "/Applications/${APP_DIR}" || { echo "error: codesign failed"; exit 1; }
    else
        codesign --force --deep --sign - "/Applications/${APP_DIR}" || true
    fi

    # Register with LaunchServices so open/Finder recognises the app
    /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "/Applications/${APP_DIR}" &>/dev/null || true

    if [ -n "$SIGNER" ]; then
        echo "Binary changed — installed with stable signature, TCC grants should be preserved."
    else
        cat << 'EOF'

================================ WARNING =================================
Binary changed and app was signed AD-HOC — your TCC grants (Full Disk
Access) for typyst are now INVALID and must be re-added, or Desktop/
Documents access will fail silently.

To do so:
  1. System Settings → Privacy & Security → Full Disk Access
  2. Remove old "typyst" entry, add /Applications/typyst.app, toggle ON
  3. Fully quit typyst (Cmd+Q) and run:  tmux kill-server
  4. Relaunch typyst

Alternatively reset first (needs sudo):
  sudo tccutil reset SystemPolicyAllFiles com.pixzor.typyst
  sudo tccutil reset SystemPolicyDesktopFolder com.pixzor.typyst

Fix forever: run 'sh setup-codesign.sh' once, then rebuild.
=========================================================================
EOF
    fi
fi

echo "Created ${APP_DIR}"
echo "Installed to /Applications/${APP_DIR}"
echo "You can now find typyst in Spotlight"
echo "Or run: open /Applications/${APP_DIR}"
