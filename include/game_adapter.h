#pragma once
#include <functional>
#include <mutex>
#include <string>
#include "game_state.h"
namespace srpc {
struct IGameStateProvider {
    virtual ~IGameStateProvider() {}
    virtual Snapshot Get() = 0;
};
// State bridge. It is fed through the extern "C" SampRPC_* functions (see main.cpp), which a Lua FFI script or a
// future verified native hook can call. The plugin itself does NOT read SA-MP memory: no offsets are verified.
class GameBridge : public IGameStateProvider {
public:
    using Listener = std::function<void(const Snapshot&)>;
    void SetListener(Listener l);
    void SetMenu();
    void SetConnecting(const char* host, int port);
    void SetConnected(const char* serverName, const char* host, int port, const char* nick);
    void SetDisconnected();
    Snapshot Get() override;
    static int64_t NowSeconds();
    int64_t (*clock)() = &GameBridge::NowSeconds;     // overridable for tests
private:
    void Publish();
    std::mutex m_; Snapshot s_; Listener l_;
};
}
