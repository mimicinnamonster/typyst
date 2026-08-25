#!/bin/bash
# One-time setup: create a self-signed codesigning identity for typyst
# Run in a real terminal: sh ~/Projects/typyst/setup-codesign.sh
set -e

CERT_NAME="Typyst Dev"
P12_PASS="typyst-p12"
KEY=/tmp/typyst-codesign.key
CRT=/tmp/typyst-codesign.crt
P12=/tmp/typyst-codesign.p12

if security find-identity -p codesigning 2>/dev/null | grep -q "\"${CERT_NAME}\""; then
    echo "'${CERT_NAME}' identity already exists in the login keychain. Nothing to do."
    exit 0
fi

echo "==> Generating self-signed certificate (10 years)..."
openssl req -x509 -newkey rsa:2048 \
  -keyout "$KEY" -out "$CRT" \
  -days 3650 -nodes \
  -subj "/CN=${CERT_NAME}" \
  -addext "basicConstraints=critical,CA:true" \
  -addext "keyUsage=critical,digitalSignature" \
  -addext "extendedKeyUsage=codeSigning"

echo "==> Packaging as PKCS12 (legacy PBE for macOS security import)..."
# macOS 'security import' fails MAC verification on the AES-based PKCS12 that
# OpenSSL 3.x produces by default — force legacy 3DES PBE.
# An empty password also triggers MAC verification failures, so use a real one.
openssl pkcs12 -export -out "$P12" \
  -inkey "$KEY" -in "$CRT" \
  -passout "pass:${P12_PASS}" \
  -certpbe PBE-SHA1-3DES -keypbe PBE-SHA1-3DES -macalg sha1

echo "==> Importing into login keychain (may prompt for your Mac password)..."
security import "$P12" \
  -k "$HOME/Library/Keychains/login.keychain-db" \
  -P "$P12_PASS" \
  -T /usr/bin/codesign

# Clean up key material from /tmp
rm -f "$KEY" "$P12" "$CRT"

echo "==> Verifying:"
if security find-identity -p codesigning | grep -q "\"${CERT_NAME}\""; then
    echo "OK: '${CERT_NAME}' is available for codesign."
    echo "(It shows as 'not trusted' in some listings — that's fine for local self-signing.)"
else
    echo "FAILED: identity not found. Check that the keychain import succeeded."
    exit 1
fi
