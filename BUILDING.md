# Building SampDiscordRPC

Target: armeabi-v7a only (Nezuko SA-MP 1.0.3).

## Requirements
- Android NDK (any recent r2x with `ndk-build`; the exact version used for the target is NOT verified).
- AML headers are bundled in `third_party/AndroidModLoader` (MIT). Override with `AML_PATH=` if you want another checkout.
  The plugin never calls `aml->` methods through the upstream (1.4.0) header; it casts to `IAML130` (include/iaml_130_abi.h),
  the 78-slot layout of the AML 1.3.0 shipped in the APK.

## Build (log-only, no Discord SDK)
```
./build.sh        # or: ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk NDK_APPLICATION_MK=Application.mk
```
Output: `libs/armeabi-v7a/libSampDiscordRPC.so`. This build NEVER talks to Discord (backend name: "log-only").

## Build with the Discord Social SDK (NOT verified to be possible, see STAGE1 report)
Add `DISCORD_SDK_DIR=/path/to/discord_social_sdk` containing `include/discordpp.h` and
`lib/armeabi-v7a/libdiscord_partner_sdk.so`. The build stops with an error if either is missing. If the SDK has no
32-bit ARM library, this path is blocked. `src/discord_backend.cpp` was written from the docs and has never been compiled.
You would also have to ship `libdiscord_partner_sdk.so` where the dynamic linker can find it (unverified how with AML).

## Host unit tests (no Android needed)
```
make -C tests run            # ASan+UBSan
g++ -std=c++17 -O1 -g -fsanitize=thread -Iinclude tests/test_main.cpp src/{config,presence_builder,game_adapter,rpc_manager,logger}.cpp -o /tmp/t -pthread && /tmp/t
```

## Common errors
- `AML_PATH is not set` / `amlmod.h not found`: point AML_PATH at the repo root (the folder containing `mod/`).
- `android/asset_manager.h` missing: you are not using the NDK toolchain.
- Plugin silently absent in game: wrong ABI, Modloader switch off, or wrong folder (see README).

## ABI
Only `armeabi-v7a` (Nezuko SA-MP 1.0.3 is 32-bit). Output: `libs/armeabi-v7a/libSampDiscordRPC.so`.
Discord SDK: `tools/setup_sdk.sh` extracts `jni/armeabi-v7a/libdiscord_partner_sdk.so` into `third_party/discord_sdk/lib/armeabi-v7a/`.
