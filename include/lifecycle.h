#pragma once
#include <memory>
#include <string>
#include <vector>
#include "config.h"
#include "game_adapter.h"
#include "rpc_manager.h"
namespace srpc {
class Plugin {
public:
    static Plugin& Get();
    // candidateDirs: directories to look for SampDiscordRPC.ini, in priority order.
    void Start(const std::vector<std::string>& candidateDirs);
    void Stop();
    GameBridge& Bridge() { return bridge_; }
private:
    std::mutex m_;
    bool started_ = false;
    GameBridge bridge_;
    std::unique_ptr<RpcManager> mgr_;
};
}
