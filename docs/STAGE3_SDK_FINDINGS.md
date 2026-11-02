# Stage 3 - SDK findings (Discord Social SDK 1.10.19337, inspected from the uploaded package)

Corrects STAGE1_COMPATIBILITY_REPORT.md items marked UNKNOWN.

| Question | Result | Status |
|---|---|---|
| Does the SDK ship 32-bit ARM? | **Yes.** `discord_partner_sdk.aar` has `jni/armeabi-v7a/libdiscord_partner_sdk.so` (ELF32 ARM EABI5, 5.5 MB) plus arm64-v8a, x86, x86_64. The top-level `release/libdiscord_partner_sdk.so` is a Linux x86-64 desktop build and is irrelevant. | VERIFIED (file/readelf) |
| Native dependencies of the armv7 lib | Only NDK system libs: libOpenSLES, libandroid, liblog, libm, libdl, libc. No libc++_shared. | VERIFIED |
| Does it need Java? | **Yes.** It exports `JNI_OnLoad` and `Java_com_discord_socialsdk_*` natives. Java classes `com.discord.socialsdk.*` (+ `libwebrtc.jar`) must exist in the process. | VERIFIED |
| How it talks to Discord | `DiscordRpcClient` binds the Discord Android app's AIDL service, action `com.discord.socialsdk.rpc.IDiscordRpcService`, RPC version "1", via `Context.bindService`. | VERIFIED (bytecode) |
| Java init requirement | `DiscordSocialSdkInit.setEngineActivity(Activity)` must be called (sets context, initializes webrtc ContextUtils, calls `nativeSetContext`). Static init calls `System.loadLibrary("discord_partner_sdk")`. `DiscordRpcClient` uses that Activity for `bindService`. | VERIFIED (bytecode) |
| Package visibility (`<queries>`) | The AAR manifest declares it for the Discord package. Nezuko targets SDK 28, so Android's API-30 package-visibility filtering should not apply. | ASSUMED |
| Backend API names | `src/discord_backend.cpp` passes `-fsyntax-only` for ARM32 against the real `discordpp.h`; `ClearRichPresence`, `UpdateRichPresence`, `RunCallbacks`, `SetApplicationId` all exist. | VERIFIED (syntax only; never linked or run) |
| AuthenticationActivity / ForegroundService manifest entries | Needed for OAuth/voice, not for plain presence. Presence without auth is claimed by the docs; unverified. | PARTIALLY VERIFIED |

## What this means for a mod (.so loaded by AML)
The ABI blocker is gone. The remaining blocker is **getting the Java side into the game process**:
1. Convert `libs/discord_partner_sdk.jar` + `libs/libwebrtc.jar` to a `.dex` (needs Android build-tools `d8`; not available in my sandbox).
2. At runtime, load it into the app class loader from native code through JNI (e.g. DexClassLoader, or appending dex elements to the app's PathClassLoader). `JNI_OnLoad`/`FindClass` inside the SDK look classes up through the loader of the class that called `loadLibrary`, so the loader choice matters.
3. Obtain an `Activity` and call `setEngineActivity`. AML 1.3.0 gives `GetJNIEnvironment` and `GetAppContextObject` but no Activity getter (`GetCurrentActivity` is 1.4.0-only).
4. Make `libdiscord_partner_sdk.so` loadable (Android's linker may refuse libraries on external storage; copying to the app's private dir is a candidate) and make sure only ONE copy is loaded, so our `discordpp` calls and the Java `nativeSetContext` hit the same instance.
None of this is built or tested. It is feasible in principle, risky in practice. A cleaner alternative is a 4-line Java hook compiled into the game APK, which the "no APK modification" rule rules out.

## Correction (after reading the Discord C++ and mobile guides)
The earlier row "Presence without auth is claimed by the docs; unverified" was wrong: presence is accepted only when `Client::Status::Ready`,
which needs UpdateToken + Connect, i.e. a signed-in user. `Client::Authorize` on Android needs `AuthenticationActivity` in the game's manifest
(not possible without modifying the APK), so the backend uses `GetTokenFromDevice` (device flow, Public Client). The SDK needs C++20 (1.9+).

## Update: implemented (still unrun)
`src/java_bootstrap.cpp` + `java/net/sampdiscordrpc/Boot.java` implement steps 1-4 above. Design decision: the SDK native library is NOT
linked into the AML plugin. `Boot.init()` calls `System.loadLibrary("SampDiscordBackend")` from a class defined by the DexClassLoader, so the
SDK's `JNI_OnLoad` resolves the SDK Java classes through the same loader and only one copy of the SDK exists. The plugin talks to it
through a plain-C table (`include/backend_c_api.h`). The Activity comes from reflection on `ActivityThread.mActivities` (hidden API; may
be blocked on some Android versions). OPEN: whether `setEngineActivity` must run on the main thread; whether Discord accepts the game's package;
whether AML ignores the `SampDiscordRPC/` subfolder.

## Next verifiable step (original note)
Write `src/java_bootstrap.cpp` (JNI: dex load + setEngineActivity) behind `SAMPRPC_WITH_DISCORD_SDK`, then test on a device with logcat. Needs: a dex file built from the two jars, and a test run. I cannot run Android code here.
