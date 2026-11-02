#!/bin/sh
# Builds dist/dex/classes.dex = net.sampdiscordrpc.Boot (java/) + the Discord SDK jars (third_party/discord_sdk/java/).
# Needs: a JDK (javac), Android build-tools (d8) and an Android platform (android.jar). GitHub's ubuntu runners have all three
# under $ANDROID_HOME (or $ANDROID_SDK_ROOT). Override with D8=/path/to/d8 ANDROID_JAR=/path/to/android.jar.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; J="$ROOT/third_party/discord_sdk/java"
SDKHOME="${ANDROID_HOME:-$ANDROID_SDK_ROOT}"
[ -n "$D8" ] || D8="$(command -v d8 || true)"
[ -n "$D8" ] || [ -z "$SDKHOME" ] || D8="$(ls -d "$SDKHOME"/build-tools/*/d8 2>/dev/null | sort -V | tail -n1)"
[ -x "$D8" ] || { echo "d8 not found (install Android build-tools or set D8=)"; exit 1; }
[ -n "$ANDROID_JAR" ] || [ -z "$SDKHOME" ] || ANDROID_JAR="$(ls -d "$SDKHOME"/platforms/android-*/android.jar 2>/dev/null | sort -V | tail -n1)"
[ -f "$ANDROID_JAR" ] || { echo "android.jar not found (install an Android platform or set ANDROID_JAR=)"; exit 1; }
[ -f "$J/discord_partner_sdk.jar" ] || { echo "SDK jars missing: run tools/setup_sdk.sh first"; exit 1; }
echo "d8: $D8"; echo "android.jar: $ANDROID_JAR"
OUT="$ROOT/dist/dex"; CLS="$OUT/classes_java"; rm -rf "$OUT"; mkdir -p "$CLS"
CP="$ANDROID_JAR:$J/discord_partner_sdk.jar"
[ -f "$J/libwebrtc.jar" ] && CP="$CP:$J/libwebrtc.jar"
# Compiling Boot.java also proves that DiscordSocialSdkInit.setEngineActivity(Activity) exists with that signature.
javac --release 8 -cp "$CP" -d "$CLS" "$ROOT"/java/net/sampdiscordrpc/Boot.java
JARS="$J/discord_partner_sdk.jar"; [ -f "$J/libwebrtc.jar" ] && JARS="$JARS $J/libwebrtc.jar"
"$D8" --release --min-api 21 --lib "$ANDROID_JAR" --output "$OUT" $(find "$CLS" -name '*.class') $JARS
ls -la "$OUT"/classes*.dex
# Only classes.dex is loaded by the plugin. If d8 produced classes2.dex the SDK is too big for one dex: the loader must be extended.
[ -f "$OUT/classes2.dex" ] && { echo "ERROR: classes2.dex produced; plugin loads only classes.dex"; exit 1; }
exit 0
