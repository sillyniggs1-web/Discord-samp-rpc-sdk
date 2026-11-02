#pragma once
#include <string>
#include "presence_builder.h"
namespace srpc {
// All methods are called from the single RPC worker thread only.
struct IPresenceBackend {
    virtual ~IPresenceBackend() {}
    virtual const char* Name() const = 0;
    virtual bool Init(const std::string& applicationId) = 0;   // false => recoverable failure, retry with backoff
    virtual bool Update(const PresenceData& p) = 0;            // submit activity
    virtual bool Clear() = 0;
    virtual void Pump() = 0;                                   // run SDK callbacks (non-blocking)
    virtual bool Healthy() const = 0;                          // false => manager re-initializes
    virtual void Shutdown() = 0;
};
// Backend that only logs. It NEVER talks to Discord; used when no SDK is compiled in or no ApplicationId is set.
IPresenceBackend* CreateLogOnlyBackend();
// With SAMPRPC_WITH_DISCORD_SDK the Discord backend is CreateDeferredDiscordBackend() (java_bootstrap.h); the SDK itself
// lives in a separate library (libSampDiscordBackend.so) reached through backend_c_api.h.
}
