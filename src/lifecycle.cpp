#include "lifecycle.h"
#include <cstdio>
#include "logger.h"
#ifdef SAMPRPC_WITH_DISCORD_SDK
#include "java_bootstrap.h"
#endif
namespace srpc {
Plugin& Plugin::Get() { static Plugin p; return p; }

void Plugin::Start(const std::vector<std::string>& dirs) {
    std::lock_guard<std::mutex> g(m_);
    if (started_) return;                           // no duplicate initialization
    try {
        Config cfg; bool loaded = false;
        for (const auto& d : dirs) {
            if (d.empty()) continue;
            if (LoadConfigFile(d + "/SampDiscordRPC.ini", cfg)) { loaded = true; SRPC_INFO("config loaded from %s", d.c_str()); break; }
        }
        if (!loaded) {
            // Try to drop a default file in the first writable candidate; otherwise run on built-in defaults.
            for (const auto& d : dirs) {
                if (d.empty()) continue;
                FILE* f = fopen((d + "/SampDiscordRPC.ini").c_str(), "wb");
                if (f) { fputs(DefaultConfigText(), f); fclose(f); SRPC_INFO("wrote default config to %s", d.c_str()); break; }
            }
            if (dirs.empty()) SRPC_WARN("no config directory available");
            SRPC_WARN("no config file read; using built-in defaults");
        }
        SetLoggingEnabled(cfg.enableLogging);
        if (!cfg.enabled) { SRPC_INFO("disabled by config"); started_ = true; return; }

        std::unique_ptr<IPresenceBackend> backend;
#ifdef SAMPRPC_WITH_DISCORD_SDK
        if (!cfg.applicationId.empty()) backend.reset(CreateDeferredDiscordBackend());
#endif
        if (!backend) {
            SRPC_WARN("no Discord backend active (%s); presence will NOT reach Discord",
                      cfg.applicationId.empty() ? "ApplicationId empty" : "built without Discord SDK");
            backend.reset(CreateLogOnlyBackend());
        }
        mgr_.reset(new RpcManager(cfg, std::move(backend)));
        RpcManager* m = mgr_.get();
        bridge_.SetListener([m](const Snapshot& s) { m->SetSnapshot(s); });
        mgr_->Start();
        // "Game launched": generic presence, never claims a server (state Unknown).
        mgr_->SetSnapshot(bridge_.Get());
        started_ = true;
    } catch (const std::exception& e) {
        SRPC_ERR("start failed: %s", e.what());
    } catch (...) {
        SRPC_ERR("start failed (unknown)");
    }
}
void Plugin::Stop() {
    std::lock_guard<std::mutex> g(m_);
    try {
        bridge_.SetListener(nullptr);
        if (mgr_) { mgr_->Stop(); mgr_.reset(); }
    } catch (...) {}
    started_ = false;
}
}
