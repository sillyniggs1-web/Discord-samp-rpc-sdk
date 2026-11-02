# STAGE 1 — Proposed Architecture (target design, NOT proven)

```
Nezuko SA-MP --(state source TBD)--> IGameStateProvider --snapshot--> PresenceBuilder --Activity data--> RpcManager --> Discord Social SDK --> Discord Android
                                            ^ AML lifecycle (OnModLoad/OnModUnload), Config (AML cfg), Logger
```

## Modules (adjusted for AML 1.3.0)
| File | Responsibility | Notes |
|---|---|---|
| `main.cpp` | `MYMODCFG`, `ON_MOD_PRELOAD/ON_MOD_LOAD/ON_MOD_UNLOAD`; no heavy work, spawn nothing blocking | upstream macros only; never hand-roll `ModInfo` |
| `lifecycle.*` | Start/stop order, idempotent init, try/catch wall so no exception crosses AML | |
| `config.*` | Wraps AML `Config` (`Bind/GetBool/GetInt`), clamps values; INI file path pending | UpdateIntervalMs clamp 1000–60000 |
| `logger.*` | Wraps AML `Logger`, rate-limited, never logs address/nick unless opted in | |
| `game_adapter.*` | `IGameStateProvider` -> `Snapshot{state, serverName, host, port, nick, sessionStart, validity flags}` | **only implemented after Stage 4 evidence**; until then `UNKNOWN` |
| `presence_builder.*` | Pure function snapshot+config -> `PresenceData` (plain struct, SDK-independent); length limits, sanitization, privacy | unit-testable on host (no Android) |
| `rpc_manager.*` | Owns SDK client; single owner thread calling `RunCallbacks`; bounded exponential backoff (e.g. 2s..5min, max attempts configurable); dedupe identical presence; clear on shutdown | thread model must follow SDK docs |

## Threading
One dedicated RPC thread owns all SDK calls and `RunCallbacks` (tick ~250 ms, sleeping). Game-side code only writes the latest snapshot into a mutex-guarded slot (no SDK calls on the game thread). Needs confirmation against SDK thread-safety docs.

## State machine
UNKNOWN -> GAME_MENU -> CONNECTING -> CONNECTED -> DISCONNECTING -> DISCONNECTED -> GAME_MENU. Session timestamp set once on entering CONNECTED for a (host,port) pair; unchanged by refreshes; reset on server change.

## Config keys (AML `Config`, section names kept from the brief)
General: Enabled, ShowMenuActivity, ShowSessionTimer, UpdateIntervalMs. Privacy: ShareServerAddress=0, SharePlayerNickname (default 1 per brief; consider default 0). Assets: LargeImage, LargeText, SmallImage. Debug: EnableLogging. (Brief had duplicate ShowServerAddress/ShareServerAddress keys; unify to the Privacy keys.) Application ID is not a secret and goes in config; no tokens stored.

## Build (planned, to be confirmed)
`armeabi-v7a`, `android-21`, `c++_static`, C++17, `-fno-exceptions` only if AML headers allow; `-llog`; link Discord SDK shared lib only if ABI exists. Output `libs/armeabi-v7a/libSampDiscordRPC.so`. Build must fail if SDK/AML headers are missing (`$(error ...)` in Android.mk).

## Stage deliverables
- S2: `main.cpp`, `Android.mk`, `Application.mk`; logs one line on load. Needs AML headers.
- S3: `rpc_manager.*` + fixed test activity. Blocked on SDK ABI/Java questions.
- S4: `game_adapter.*` from verified state source.
- S5: wiring, backoff, privacy, tests.
- S6: README/BUILDING/checklist; no fake artifacts.
