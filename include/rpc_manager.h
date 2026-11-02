#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include "config.h"
#include "game_state.h"
#include "presence_backend.h"
namespace srpc {
struct RpcTiming {
    int tickMs = 250;                  // worker wake-up period (callbacks pump + dirty check)
    int backoffInitialMs = 2000;
    int backoffMaxMs = 300000;
    int maxConsecutiveFailures = 10;   // then give up until the game state changes
};
class RpcManager {
public:
    RpcManager(const Config& cfg, std::unique_ptr<IPresenceBackend> backend, RpcTiming t = RpcTiming());
    ~RpcManager();
    bool Start();                      // idempotent
    void Stop();                       // idempotent; clears presence, shuts backend down
    void SetSnapshot(const Snapshot& s);   // thread-safe, cheap, never blocks on Discord
    unsigned PublishCount() const { return publishes_.load(); }
    bool GaveUp() const { return gaveUp_.load(); }
private:
    void Run();
    Config cfg_; RpcTiming timing_;
    std::unique_ptr<IPresenceBackend> backend_;
    std::mutex m_; std::condition_variable cv_;
    Snapshot snap_; bool snapChanged_ = false;
    bool stop_ = false; std::thread th_; bool started_ = false;
    std::atomic<unsigned> publishes_{0};
    std::atomic<bool> gaveUp_{false};
};
}
