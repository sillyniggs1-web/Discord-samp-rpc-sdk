# STAGE 1 — Missing Requirements

| # | Needed | Why | How to provide |
|---|---|---|---|
| 1 | AML upstream headers (`mod/amlmod.h`, `logger.h`, `config.h`) or the AML repo/zip you want me to use | Stage 2 entry-point macros; must match 1.3.0 vtable | Upload zip |
| 2 | `SA-MP_Nezuko_v1_0_3__TESTING_.apk` (or `lib/armeabi-v7a/*.so`: `libNezukoSA-MP.so`, `libsamp.so`, `libAML.so`) | Re-verify facts and look for exports/strings for connection state | Upload |
| 3 | Discord Social SDK Android package (>= 1.10), with the `.aar` | Check ABIs (`armeabi-v7a`?) and Java/JNI dependencies | Download from your Dev Portal app (needs the Social SDK enabled), upload |
| 4 | Discord Application ID + asset keys | Config defaults, assets | Paste ID (not secret) |
| 5 | One working Lua script that already reads server name/IP/nickname, if any | Possible state source (fallback A) | Paste |
| 6 | Directory listing of `/sdcard/Android/data/com.nezukosamp.game/` and `aml_crashlog.txt`/`android_log_print.txt` from a run | Real mods/config paths | Paste |
| 7 | Build environment: NDK version you will use | Reproducible build | State version |

Can proceed without these: documentation drafts, the SDK-independent `PresenceBuilder`, `Config` clamping logic, host-side unit tests.
