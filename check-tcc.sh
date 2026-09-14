#!/bin/bash
# Read-only TCC diagnostics for typyst
# Run: sudo sh ./check-tcc.sh

echo "=== System TCC DB (Full Disk Access / Desktop folders) ==="
sudo -u _tccd sqlite3 "/Library/Application Support/com.apple.TCC/TCC.db" \
  "SELECT service, client, client_type, auth_value, auth_reason, datetime(last_modified,'unixepoch','localtime') FROM access WHERE client LIKE '%typyst%' OR client LIKE '%pixzor%';" 2>&1

echo
echo "=== User TCC DB (per-user grants) ==="
sudo -u _tccd sqlite3 "$HOME/Library/Application Support/com.apple.TCC/TCC.db" \
  "SELECT service, client, client_type, auth_value, auth_reason, datetime(last_modified,'unixepoch','localtime') FROM access WHERE client LIKE '%typyst%' OR client LIKE '%pixzor%';" 2>&1

echo
echo "=== Current app identity ==="
codesign -d --verbose=2 /Applications/typyst.app 2>&1 | grep -E 'CDHash|Identifier'

echo
echo "=== App mtime (last rebuild) ==="
stat -f '%Sm %N' /Applications/typyst.app/Contents/MacOS/typyst 2>&1
