#include "presence_backend.h"
#include "logger.h"
namespace srpc {
namespace {
// Not a Discord implementation: it only records what WOULD be sent. Name() says so explicitly.
class LogOnlyBackend : public IPresenceBackend {
public:
    const char* Name() const override { return "log-only (no Discord)"; }
    bool Init(const std::string&) override { return true; }
    bool Update(const PresenceData& p) override { SRPC_INFO("[log-only] activity: details='%s' state='%s' ts=%lld btn=%zu", p.details.c_str(), p.state.c_str(), (long long)p.startTimestamp, p.buttons.size()); return true; }
    bool Clear() override { SRPC_INFO("[log-only] clear"); return true; }
    void Pump() override {}
    bool Healthy() const override { return true; }
    void Shutdown() override {}
};
}
IPresenceBackend* CreateLogOnlyBackend() { return new LogOnlyBackend(); }
}
