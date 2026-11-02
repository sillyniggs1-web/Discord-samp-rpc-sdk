# SampDiscordRPC (AML plugin for Nezuko SA-MP Android) - v0.1.0 pre-beta

Publishes GTA SA-MP activity to Discord Rich Presence. **Read STATUS first: this is not beta-ready.**

## STATUS (honest matrix)
| Feature | Implemented | Compiled | Runtime-verified |
|---|---|---|---|
| AML entry point (`__GetModInfo`, `OnModLoad`, `OnModUnload`) | yes | yes, ARM32 cross-compile with *stub* Android headers (not the NDK, not bionic) | **no** |
| Config parser (clamping, validation, privacy defaults) | yes | yes (host) | host unit tests pass |
| Presence builder (privacy, limits, sanitizing) | yes | yes (host) | host unit tests pass |
| RPC manager (single worker thread, dedupe, rate limit, bounded backoff, clear on stop) | yes | yes (host, ARM32 check) | host tests pass incl. ASan/UBSan + TSan |
| State bridge `SampRPC_Set*` C exports + session timer logic | yes | yes | host tests pass; **nothing calls it yet** |
| Reading server/nick/connection from Nezuko natively | **NO** - no verified source/offset | - | - |
| Lua feeder `lua/samprpc_bridge.lua` | yes: standalone MoonLoader script using the same SA-MP calls as your scoreboard script | not syntax-checked (no Lua in sandbox) | **no** |
| Discord Social SDK backend `src/discord_backend.cpp` (`libSampDiscordBackend.so`, loaded by Java): device-flow login, token file, connect, presence | yes | syntax-checked on host against a stand-in `discordpp.h` written from the docs; the real compile (C++20) happens in the workflow | **no** |
| Java bootstrap `src/java_bootstrap.cpp` + `java/net/sampdiscordrpc/Boot.java` | yes | C++ syntax-checked on host against a stand-in `jni.h`; real compile happens in the GitHub workflow | **no** |
| `classes.dex` build (`tools/make_dex.sh`, run by the workflow) | yes | not run (needs d8 / android.jar from the runner) | **no** |
| Actual presence on your Discord profile | **not achieved** | - | **no** |

Out of the box the plugin runs the "log-only" backend: it logs what it would publish and does **not** reach Discord.


## Pack layout
```
SampDiscordRPC/
  build.sh                 one-command build (both ABIs); uses third_party/ automatically
  Android.mk, Application.mk, main.cpp
  src/  include/           plugin source (+ include/iaml_130_abi.h: Nezuko AML 1.3.0 layout, 32-bit only)
  config/SampDiscordRPC.ini  default config
  lua/samprpc_bridge.lua   MoonLoader script that feeds server/IP/nick to the plugin (untested)
  tests/                   host unit tests (make -C tests run)
  tools/setup_sdk.sh       extracts headers/libs/jars from YOUR Discord SDK download into third_party/discord_sdk
  tools/make_dex.sh        Boot.java + SDK jars -> classes.dex (workflow runs it; needs javac, d8, android.jar)
  java/                    Boot.java: loads libSampDiscordBackend.so and calls DiscordSocialSdkInit.setEngineActivity
  third_party/AndroidModLoader/   AML headers (MIT, included)
  third_party/discord_sdk/        empty: filled by tools/setup_sdk.sh (SDK is proprietary, not bundled)
  dist/install_to_phone/   where the built .so + ini example go on the phone
  docs/                    STAGE1 reports, STAGE3 SDK findings, status + test checklist
  BUILDING.md  README.md
```
## Quick start
```
export ANDROID_NDK_HOME=/path/to/ndk
tools/setup_sdk.sh /path/to/DiscordSocialSdk-1.10.19337.zip   # optional; skip for log-only
./build.sh
```
Output: `libs/armeabi-v7a/libSampDiscordRPC.so` (Nezuko, 32-bit only).

## How the Discord path works (all UNVERIFIED on a device)
```
Lua -> SampRPC_SetConnected() -> RpcManager -> DeferredDiscordBackend (java_bootstrap.cpp, in libSampDiscordRPC.so)
   Init(), on the worker thread, retried with backoff until it works:
   1 find a live Activity (ActivityThread.mActivities, reflection)
   2 copy classes.dex + libSampDiscordBackend.so + libdiscord_partner_sdk.so into <app files>/sampdiscordrpc/
   3 new DexClassLoader(classes.dex, lib path = that folder, parent = the game's class loader)
   4 Boot.init(activity): System.loadLibrary("SampDiscordBackend") -> SDK loaded in the SAME loader as its Java glue,
     then DiscordSocialSdkInit.setEngineActivity(activity)
   5 Boot.nativeApi() returns a plain-C function table -> init / update / clear / pump (backend_c_api.h)
```
The plugin itself never links the SDK, so it always loads; without the SDK files it logs why and keeps retrying.

### Discord login (the SDK only accepts presence once its status is `Ready`)
`libSampDiscordBackend.so` signs in with the **OAuth2 device flow** (`Client::GetTokenFromDevice`): no redirect URI and no
AndroidManifest entry. `Client::Authorize` is deliberately NOT used: on Android it needs `com.discord.socialsdk.AuthenticationActivity`
declared in the game's manifest (Discord's account-linking-on-mobile guide), which this project cannot add.
- First run: the SDK starts the device authorization; approve it at **discord.com/activate** with the code the SDK shows/logs
  (UNVERIFIED how the code is displayed on Android; read logcat tags `SampDiscordRPC` and `SampDiscordSDK`).
- The token is stored in the app-private folder (`discord_token.dat`, mode 0600) and reused; when it expires or is rejected the flow restarts
  automatically (refresh tokens are not used yet). Presence requested before `Ready` is kept and sent when `Ready` arrives.
- Device flow is documented for consoles; whether it is accepted for an Android client is **unverified**. Fallback if it is not: a companion
  approach, or a manifest-level change (not available here).

### Developer Portal checklist
1. Application named exactly **San Andreas Multiplayer** (this is the title on the card).
2. Social SDK enabled for the application (Getting Started under "Discord Social SDK").
3. OAuth2 tab: **Public Client ON** (required by `GetTokenFromDevice` without a backend; Discord says most shipped games should not do this).
4. Rich Presence art asset with key `samp_logo`.
5. Application ID into `[Discord] ApplicationId` of `SampDiscordRPC.ini`.
(The `discord-<APP_ID>:/authorize/callback` redirect is only needed for `Authorize`, which is not used here.)

## Building on GitHub (the only supported way here)
Push to GitHub; the `build` workflow builds the plugin. For the real Discord path it also needs YOUR copy of the Discord Social SDK zip:
either commit it to a **private** repo (`git add -f sdk/DiscordSocialSdk-1.10.19337.zip`; `sdk/` is gitignored on purpose) or add a repository
secret `DISCORD_SDK_URL` with a direct download link. Without it you get the log-only plugin and a warning in the run.
The artifact `SampDiscordRPC-pack` contains the proprietary `libdiscord_partner_sdk.so` when built with the SDK, so use a private repo.

## Install (untested)
1. Launcher settings: Modloader ON; storage permission granted.
2. Copy the contents of the artifact's `mods/` folder to `<Android/data/com.nezukosamp.game>/mods/` (the `SampDiscordRPC/` subfolder keeps the SDK libraries out of AML's top-level scan - whether AML skips subfolders is unverified). Copy `lua/samprpc_bridge.lua` to your MonetLoader scripts folder.
3. Launch; check logcat tag `SampDiscordRPC`. The config file is searched in AML-provided directories and a default
   `SampDiscordRPC.ini` is written to the first writable one (which directory works is unverified; the log prints it).

## Target look (from your screenshot)
```
Playing  San Andreas Multiplayer                     <- the Discord APPLICATION NAME (set in the Developer Portal)
[logo]   HackMySoftware - Cheating Server | Version: 4.1    <- details = server name
         Deprau • 109.69.58.7:7777                          <- state   = nickname • ip:port
         0:11                                               <- start timestamp (elapsed timer)
```
Developer Portal setup: create an application named exactly **San Andreas Multiplayer**, enable the Social SDK, upload your
SA-MP logo as an Rich Presence art asset with key **samp_logo**, put the Application ID in `[Discord] ApplicationId`.
Privacy toggles (all default ON to match the look): `ShareServerName`, `SharePlayerNickname`, `ShareServerAddress`.

## Config
See `config/SampDiscordRPC.ini`. Privacy defaults: server name, nickname and ip:port are shown (to match the target look); set to 0 to hide.
Buttons: only your own `https://` URLs, max 2, shown only while connected. Never stores credentials.

## Known limitations
No native state source; Discord SDK feasibility on 32-bit ARM unknown; Discord must be installed and signed in;
abrupt process kill cannot clear presence; AML crash handling and config path unverified.


See `docs/STAGE3_SDK_FINDINGS.md`: the SDK DOES ship armeabi-v7a, but needs its Java classes loaded into the game process.
