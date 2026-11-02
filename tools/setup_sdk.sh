#!/bin/sh
# Extracts what the build needs from YOUR copy of the Discord Social SDK (proprietary; not redistributed here).
# Usage: tools/setup_sdk.sh /path/to/DiscordSocialSdk-1.10.xxxx.zip   (or the .aar, or an unpacked SDK folder)
set -e
SRC="$1"; [ -n "$SRC" ] || { echo "usage: $0 <sdk zip | discord_partner_sdk.aar | sdk folder>"; exit 1; }
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="$ROOT/third_party/discord_sdk"; TMP="$(mktemp -d)"
mkdir -p "$OUT/include" "$OUT/java"
AAR=""
case "$SRC" in
  *.aar) AAR="$SRC" ;;
  *.zip) unzip -q -o "$SRC" 'release/discord_partner_sdk.aar' 'include/*' 'License-Notices.txt' -d "$TMP" || true
         AAR="$TMP/release/discord_partner_sdk.aar"; [ -d "$TMP/include" ] && cp "$TMP/include/"*.h "$OUT/include/" ;;
  *) AAR="$SRC/release/discord_partner_sdk.aar"; cp "$SRC/include/"*.h "$OUT/include/" ;;
esac
[ -f "$AAR" ] || { echo "discord_partner_sdk.aar not found"; exit 1; }
unzip -q -o "$AAR" 'jni/armeabi-v7a/*' 'libs/*.jar' 'prefab/modules/discord_partner_sdk/include/*.h' -d "$TMP/aar"
[ -f "$OUT/include/discordpp.h" ] || cp "$TMP/aar/prefab/modules/discord_partner_sdk/include/"*.h "$OUT/include/"
for abi in armeabi-v7a; do mkdir -p "$OUT/lib/$abi"; cp "$TMP/aar/jni/$abi/libdiscord_partner_sdk.so" "$OUT/lib/$abi/"; done
cp "$TMP/aar/libs/"*.jar "$OUT/java/"
[ -f "$TMP/License-Notices.txt" ] && cp "$TMP/License-Notices.txt" "$OUT/"
rm -rf "$TMP"
echo "SDK prepared in $OUT"; ls -R "$OUT" | head -30
