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

"$DIR/typyst-bin" -p 60 -t 0.80 "${BG_ARGS[@]}" /bin/zsh -c \
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

# Copy to /Applications so Spotlight/Launchpad can find it
rm -rf "/Applications/${APP_DIR}"
cp -R "${APP_DIR}" "/Applications/${APP_DIR}"

# Register with LaunchServices so open/Finder recognises the app
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "/Applications/${APP_DIR}" &>/dev/null || true

echo "Created ${APP_DIR}"
echo "Installed to /Applications/${APP_DIR}"
echo "You can now find typyst in Spotlight"
echo "Or run: open /Applications/${APP_DIR}"
