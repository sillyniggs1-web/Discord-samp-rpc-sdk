#!/bin/sh
# Builds libSampDiscordRPC.so for armeabi-v7a (Nezuko SA-MP, 32-bit). Needs ANDROID_NDK_HOME (or ndk-build on PATH).
# If third_party/discord_sdk/ has been prepared (tools/setup_sdk.sh) the real Discord backend is compiled in.
set -e
cd "$(dirname "$0")"
NDKB="${ANDROID_NDK_HOME:+$ANDROID_NDK_HOME/}ndk-build"
SDKARG=""
[ -f third_party/discord_sdk/include/discordpp.h ] && SDKARG="DISCORD_SDK_DIR=$(pwd)/third_party/discord_sdk" && echo "Discord SDK found: building with real backend"
[ -z "$SDKARG" ] && echo "No Discord SDK prepared: building LOG-ONLY plugin (nothing reaches Discord)"
"$NDKB" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk AML_PATH="$(pwd)/third_party/AndroidModLoader" $SDKARG "$@"
echo "Output:"; ls -la libs/*/libSampDiscordRPC.so libs/*/libSampDiscordBackend.so 2>/dev/null || true
