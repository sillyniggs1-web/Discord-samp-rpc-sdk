#pragma once
#include <cstdint>
#include <string>
namespace srpc {
enum class GameState { Unknown, GameMenu, Connecting, Connected, Disconnecting, Disconnected };

// Normalized snapshot. A field is only meaningful when its has* flag is true.
struct Snapshot {
    GameState state = GameState::Unknown;
    std::string serverName; bool hasServerName = false;
    std::string host;       bool hasAddress = false;
    int port = 0;
    std::string nickname;   bool hasNickname = false;
    int64_t sessionStart = 0;   // unix seconds; 0 = unknown
    bool operator==(const Snapshot& o) const {
        return state == o.state && serverName == o.serverName && hasServerName == o.hasServerName &&
               host == o.host && hasAddress == o.hasAddress && port == o.port && nickname == o.nickname &&
               hasNickname == o.hasNickname && sessionStart == o.sessionStart;
    }
};
}
