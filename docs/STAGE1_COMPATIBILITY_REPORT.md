# STAGE 1 — Compatibility Report: SA-MP Discord RPC (`libSampDiscordRPC.so`)

Date: 2026-11-01. Method: documentation review + notes from an earlier static APK analysis (skill reference files). **Nothing was compiled, executed or disassembled in this session.** No APK, AML source, Discord SDK or `.so` files were supplied in this conversation (`/mnt/user-data/uploads` is empty).

Status legend: VERIFIED / PARTIALLY VERIFIED / ASSUMED / BLOCKED

## 1. Target environment summary
| Item | Value | Status |
|---|---|---|
| Package | `com.nezukosamp.game`, v1.0.3 (code 103) | VERIFIED (prior static analysis; not re-checked now) |
| minSdk / targetSdk | 21 / 28 | VERIFIED (prior) |
| ABI | **armeabi-v7a only** | VERIFIED (prior) |
| Mod loader | AML **1.3.0** (`net.rusjj.aml`) in-APK | VERIFIED (prior) |
| Mod folder | `<Android/data/com.nezukosamp.game>/mods/*.so` (format string `%s/mods/`) | PARTIALLY VERIFIED (string seen; full path from upstream docs) |
| Enable switch | Launcher "Modloader" -> `samp_settings.modloader` | VERIFIED (prior) |
| Storage perms | MANAGE_EXTERNAL_STORAGE, legacy external storage | VERIFIED (prior) |

## 2. AML interfaces (1.3.0)
- Entry points AML looks up: `__GetModInfo`, `OnModPreLoad`, `OnModLoad`, `OnAllModsLoaded`, `OnInterfaceAdded`, `OnGameCrash`, `OnModUnload` — VERIFIED (strings in libAML).
- `IAML` vtable has 78 slots (not ~128 as in upstream 1.4.0). Only slots in `iaml_130_abi.h` may be called — VERIFIED. Calling 1.4.0-only methods (file helpers, `GetCurrentActivity`, `CallStaticJavaMethod`, `InjectSmaliDEX`, `GetAssetManager`...) = crash.
- Available and useful: `GetJavaVM`, `GetJNIEnvironment`, `GetAppContextObject`, `GetCurrentContext`, `GetAndroidDataPath`, `GetConfigPath`, `GetInternalPath`, `Hook*`, `PatternScan_Lib`, `GetLib`, `GetSym`, `ShowToast`.
- Logging: `Logger::Info/Error/...` (+ `EnableLogcats` config) — VERIFIED symbols.
- Config: `Config::Bind/GetInt/GetBool/GetString/Save` — VERIFIED symbols. **Exact writable INI path is UNKNOWN** (format `%s/%s.ini`). Consequence: use AML `Config` (`cfg`) for settings; a custom `SampDiscordRPC.ini` location must be confirmed on a device.
- Threading model, failure handling by AML for mods that crash in `OnModLoad`: UNKNOWN (no source). `OnGameCrash` exists.
- Build template: `armeabi-v7a`, `android-21`, `c++_static`, `-std=c++17`, needs upstream `mod/amlmod.h` headers (**not supplied**).

## 3. Target client (Nezuko)
- Native libs: `libGTASA.so`, `libsamp.so` (Arizona-derived glue), `libNezukoSA-MP.so` (custom client, imports `DobbyHook`), `libAML.so`, `libmonetloader.so`, `libluajit-5.1.so`, `libCLEO.so`, `libdobby.so`. VERIFIED (prior).
- Client files: `SAMP/nzsettings.ini`, `SAMP/settings.json`. VERIFIED strings.
- **Connection-state interface: UNKNOWN.** No exported symbol, callback, or offset for connect/disconnect, server name, IP/port, nickname has been identified. Java strings are encrypted. This blocks Stage 4.
- Possible existing source of state (UNVERIFIED hypotheses to test, not assumptions): (a) Nezuko writes server/nick to `SAMP/settings.json`/`nzsettings.ini`; (b) Lua side (MonetLoader/SAMPFUNCS-style globals) already exposes `sampGetCurrentServerName/Address`, `sampGetPlayerNickname`-like calls that the user's working scripts use; (c) `libNezukoSA-MP.so` exported symbols via `nm -D`/readelf.

## 4. Discord integration feasibility (from current official docs, fetched today)
| Finding | Status |
|---|---|
| Social SDK standalone C++ = header-only wrapper (`discordpp.h`, one file defines `DISCORDPP_IMPLEMENTATION`) over a shared C library; must call `discordpp::RunCallbacks()` regularly | VERIFIED (docs) |
| Android RPC (presence without auth) supported from **Social SDK 1.10+**; requires Discord app installed and signed in, and an Application ID; flow = `SetApplicationId` -> `UpdateRichPresence` (no `Connect`) | VERIFIED (docs; changelog entry dated Aug 3, 2026) |
| Docs conflict: the mobile page still says Rich Presence "requires account linking" on Android; the Rich Presence guide says RPC works without auth. Platform-compatibility page lists Android "Experimental", mobile page says "Generally Available" | PARTIALLY VERIFIED — must be resolved by testing |
| Android SDK is delivered as `discord_partner_sdk.aar` (Gradle) and is configured for standard Android projects | VERIFIED (docs) |
| **Which ABIs the Android SDK ships (is `armeabi-v7a` included?)** | **UNKNOWN — critical.** Nezuko is 32-bit only. If the SDK has no armv7 build, the official SDK cannot be loaded. |
| Does the `.aar` carry Java classes / manifest entries / JNI init that must exist in the host APK? A mod `.so` cannot add Java classes (no `InjectSmaliDEX` in AML 1.3.0, no APK modification allowed by the brief) | **UNKNOWN — likely blocker** |
| Buttons: up to 2 `ActivityButton` (label+URL), visible only to other users | VERIFIED (docs) |
| Assets: upload in Dev Portal, keys lowercased, 1024x1024 recommended, max 300 | VERIFIED (docs) |
| Timestamps are Unix seconds; start-only counts up | VERIFIED (docs) |
| SDK is proprietary, downloaded per-application from the Developer Portal; not redistributable with the source | VERIFIED (practice seen in other projects; confirm license text) |
| Android package-visibility: targetSdk 28 means the API 30 `<queries>` restriction does not apply to the host app | ASSUMED (reasoning from targetSdk; test) |
| Mobile RPC works via Discord Android app IPC (mechanism undocumented) | UNKNOWN |

## 5. Dependencies
Required inputs not yet available: AML upstream headers (`mod/*.h`, consistent with 1.3.0 vtable), Android NDK (r2x; version TBD), Discord Social SDK >= 1.10 Android package, the Nezuko `.so` files for analysis, a Discord Application ID + uploaded assets.

## 6. Known restrictions
- 32-bit only; arm64 plugin will never load.
- No 1.4.0 AML APIs.
- Plugin runs inside the game process: Java-side Discord SDK pieces cannot be added by the plugin.
- Presence is only visible while Discord app is installed + signed in on the same device.
- Abrupt process kill will skip cleanup; Discord should expire the activity itself (behavior UNVERIFIED).

## 7. Unresolved questions
1. Does the Social SDK Android package include `armeabi-v7a` native libs?
2. Can the SDK's RPC path initialize without its Java/AAR parts (pure-native from an injected `.so`)?
3. Which file/symbol/Lua API gives Nezuko connection state, server name, address, nickname?
4. Exact AML config/INI path on-device.
5. Does the Modloader loader accept additional `.so` dependencies (e.g. `libdiscord_partner_sdk.so`) placed in `mods/`, and in what link/search path? (`dlopen` by full path from `OnModLoad` is a candidate.)

## 8. Risks
High: SDK ABI mismatch; missing Java pieces. High: no known state source. Medium: doc inconsistency on auth requirement. Medium: AML threading/crash behavior unknown. Low: config location.

## 9. Fallback options (trade-offs; none adopted yet)
A. **Lua-side bridge:** the plugin is skipped for state; a MonetLoader Lua script reads state (APIs the user's scripts already use) and passes it to the native plugin via FFI. Reduces native-offset risk, adds Lua dependency.
B. **Discord IPC/gateway without the SDK:** unofficial, ToS/account-risk, token handling issues; **not recommended** and not to be done silently.
C. **Companion approach** (separate app, e.g. community tools like Kizzy-style apps): violates "no companion app" requirement; mention only as last resort.

## 10. Stage plan summary
See `STAGE1_ARCHITECTURE.md`. Go/No-go: **GO** Stage 2 (minimal AML plugin) once AML headers are supplied. **CONDITIONAL / NO-GO** Stage 3 until questions 1–2 are answered. **NO-GO** Stage 4 until a state source (question 3) is identified.
