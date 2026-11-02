#include "rpc_manager.h"
#include "logger.h"
#include "presence_builder.h"
namespace srpc {
using Clock = std::chrono::steady_clock;

RpcManager::RpcManager(const Config& cfg, std::unique_ptr<IPresenceBackend> b, RpcTiming t)
    : cfg_(cfg), timing_(t), backend_(std::move(b)) {}
RpcManager::~RpcManager() { Stop(); }

bool RpcManager::Start() {
    std::lock_guard<std::mutex> g(m_);
    if (started_ || !backend_) return started_;
    stop_ = false; started_ = true;
    try { th_ = std::thread(&RpcManager::Run, this); }
    catch (...) { started_ = false; SRPC_ERR("failed to start RPC thread"); return false; }
    return true;
}
void RpcManager::Stop() {
    {
        std::lock_guard<std::mutex> g(m_);
        if (!started_) return;
        stop_ = true;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
    std::lock_guard<std::mutex> g(m_);
    started_ = false;
}
void RpcManager::SetSnapshot(const Snapshot& s) {
    {
        std::lock_guard<std::mutex> g(m_);
        if (snap_ == s) return;
        snap_ = s; snapChanged_ = true;
    }
    cv_.notify_all();
}

void RpcManager::Run() {
    bool inited = false, havePublished = false, lastWasClear = true;
    PresenceData last;
    int failures = 0;
    int backoff = timing_.backoffInitialMs;
    auto nextInit = Clock::now();
    auto lastPublish = Clock::time_point::min();
    try {
        for (;;) {
            Snapshot snap; bool changed = false;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait_for(lk, std::chrono::milliseconds(timing_.tickMs), [&] { return stop_ || snapChanged_; });
                if (stop_) break;
                snap = snap_; changed = snapChanged_; snapChanged_ = false;
            }
            const auto now = Clock::now();
            if (changed && gaveUp_.load()) { gaveUp_ = false; failures = 0; backoff = timing_.backoffInitialMs; nextInit = now; }

            if (!inited) {
                if (gaveUp_.load() || now < nextInit) continue;
                if (backend_->Init(cfg_.applicationId)) {
                    inited = true; failures = 0; backoff = timing_.backoffInitialMs; havePublished = false;
                    SRPC_INFO("backend '%s' initialized", backend_->Name());
                } else {
                    ++failures;
                    if (failures >= timing_.maxConsecutiveFailures) {
                        gaveUp_ = true; SRPC_WARN("backend init failed %d times; pausing until game state changes", failures);
                    } else {
                        nextInit = now + std::chrono::milliseconds(backoff);
                        backoff = backoff * 2 > timing_.backoffMaxMs ? timing_.backoffMaxMs : backoff * 2;
                    }
                    continue;
                }
            }
            backend_->Pump();
            if (!backend_->Healthy()) {
                SRPC_WARN("backend unhealthy; re-initializing");
                backend_->Shutdown(); inited = false; ++failures;
                nextInit = now + std::chrono::milliseconds(backoff);
                backoff = backoff * 2 > timing_.backoffMaxMs ? timing_.backoffMaxMs : backoff * 2;
                if (failures >= timing_.maxConsecutiveFailures) gaveUp_ = true;
                continue;
            }
            PresenceData want = BuildPresence(snap, cfg_);
            if (havePublished && want == last) continue;                       // nothing changed: no republish
            if (havePublished && now - lastPublish < std::chrono::milliseconds(cfg_.updateIntervalMs)) continue; // rate limit
            bool ok = want.clear ? (lastWasClear && havePublished ? true : backend_->Clear()) : backend_->Update(want);
            if (ok) { last = want; havePublished = true; lastWasClear = want.clear; lastPublish = now; ++publishes_; }
            else {
                SRPC_WARN("publish failed");
                backend_->Shutdown(); inited = false; ++failures;
                nextInit = now + std::chrono::milliseconds(backoff);
                backoff = backoff * 2 > timing_.backoffMaxMs ? timing_.backoffMaxMs : backoff * 2;
            }
        }
        if (inited) {
            if (havePublished && !lastWasClear) backend_->Clear();
            backend_->Pump();
            backend_->Shutdown();
        }
    } catch (const std::exception& e) {
        SRPC_ERR("RPC thread exception: %s", e.what());
    } catch (...) {
        SRPC_ERR("RPC thread unknown exception");
    }
}
}
