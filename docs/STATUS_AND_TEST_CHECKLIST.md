# Stage status
1 Evidence/compat: DONE (see STAGE1_*.md; several items UNKNOWN).
2 Minimal AML plugin: code DONE; ARM32 compile check done with stub headers; **NOT built with NDK, NOT loaded in game**.
3 Discord proof of concept: SDK package inspected (armeabi-v7a present, Java required). Java bootstrap + backend module + dex build WRITTEN; compiled only by the GitHub workflow, **never run**.
4 SA-MP state adapter: **BLOCKED** - no verified native source. A bridge (C exports + a standalone Lua script `lua/samprpc_bridge.lua`, built from calls known to work in Mimgui_ScoreBoard.lua) exists instead.
5 Live presence: manager/builder/bridge logic DONE and host-tested; end-to-end **not tested**.
6 Beta: **NOT reached.**

# Manual test checklist (all UNRUN)
A. Static: `nm -D libSampDiscordRPC.so` shows `OnModLoad`, `__GetModInfo`; `file` says ARM 32-bit.
B. Load: Modloader ON, copy .so, relaunch; AML log shows mod loaded; logcat `SampDiscordRPC` prints "no Discord backend active".
C. Config: edit ini, relaunch; values applied; malformed values fall back.
D. Discord (needs SDK): test activity appears on a second account's view; buttons only visible to others.
E. Game: copy `lua/samprpc_bridge.lua` to the MonetLoader scripts folder; the console/log should print `plugin reached through ...` then `connected: <server> <ip>:<port>`; if it prints `plugin symbols not reachable`, ffi cannot see the plugin and that needs a different route. Then check that connect/disconnect/server switch update presence; timer not reset by refreshes.
F. Stability: Discord not installed / signed out / airplane mode; background/resume; 30+ min session; other AML mods.

# Device test for the Java bootstrap (read logcat tag SampDiscordRPC, in this order)
1. "java bootstrap not complete: no Activity available yet (will retry)" is normal early; it should turn into "activity search ... found one".
   If it never does: reflection on ActivityThread is blocked -> report the Android version.
2. "staged dex + native libs": if not, the error names the missing file/folder.
3. "java exception at ...": copy the full line (UnsatisfiedLinkError = namespace/path problem; NoSuchMethodError = SDK version mismatch).
4. "Boot.init OK" then "SDK library mapped once (single copy)". "DOUBLE COPY" means the design assumption failed.
5. "starting Discord device authorization": approve at discord.com/activate. Read logcat tag `SampDiscordSDK` for what the SDK prints (the code?).
   "device authorization failed: ..." (retries every 60 s) means the flow is not accepted on Android or Public Client is off: send me the text.
6. "SDK status: Ready" then "rich presence updated". "SDK connection error" lines name the failing step.
7. Then Discord should show the activity; if not, the SDK result callback flips healthy() and the manager re-inits with backoff.
