// SampDiscordRPC - AML 1.3.0 mod entry point (Nezuko SA-MP 1.0.3, armeabi-v7a).
// Only slots of IAML130 (include/iaml_130_abi.h) are ever called through `aml`; the upstream IAML header is used
// for the MYMOD macro/ModInfo only.
#include <mod/amlmod.h>
#if !defined(__arm__)
#error "SampDiscordRPC targets Nezuko SA-MP (armeabi-v7a, 32-bit) only"
#endif
#include "iaml_130_abi.h"
#include "lifecycle.h"
#include "logger.h"
#ifdef SAMPRPC_WITH_DISCORD_SDK
#include "java_bootstrap.h"
#endif

MYMOD(net.sampdiscordrpc, SA-MP Discord RPC, 0.1.0, SampDiscordRPC)

ON_MOD_LOAD()
{
    try {
        std::vector<std::string> dirs;
        // Nezuko ships AML 1.3.0 -> use the ABI-exact 78-slot layout (verified from its libAML.so).
        auto* a = reinterpret_cast<IAML130*>(aml);
        if (a) {
            // Which of these (if any) is the right writable config location is UNVERIFIED on-device; all are tried.
            const char* c[] = { a->GetConfigPath(), a->GetAndroidDataPath(), a->GetDataPath(), a->GetInternalPath() };
            for (const char* p : c) if (p && *p) dirs.push_back(p);
        }
#ifdef SAMPRPC_WITH_DISCORD_SDK
        if (a) srpc::SetJavaVmFallback(a->GetJavaVM());   // only used if JNI_GetCreatedJavaVMs is not found
#endif
        srpc::Plugin::Get().Start(dirs);
    } catch (...) { /* never let an exception cross the AML boundary */ }
}

ON_MOD_UNLOAD()
{
    try { srpc::Plugin::Get().Stop(); } catch (...) {}
}

// ---- State bridge for Lua (ffi) / future verified native hooks. All are no-ops if the plugin is not running. ----
#define SRPC_EXPORT extern "C" __attribute__((visibility("default")))
SRPC_EXPORT void SampRPC_SetMenu() { try { srpc::Plugin::Get().Bridge().SetMenu(); } catch (...) {} }
SRPC_EXPORT void SampRPC_SetConnecting(const char* host, int port) { try { srpc::Plugin::Get().Bridge().SetConnecting(host, port); } catch (...) {} }
SRPC_EXPORT void SampRPC_SetConnected(const char* name, const char* host, int port, const char* nick) { try { srpc::Plugin::Get().Bridge().SetConnected(name, host, port, nick); } catch (...) {} }
SRPC_EXPORT void SampRPC_SetDisconnected() { try { srpc::Plugin::Get().Bridge().SetDisconnected(); } catch (...) {} }
