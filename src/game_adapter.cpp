#include "game_adapter.h"
#include <ctime>
namespace srpc {
static std::string Cap(const char* p, size_t n = 128) {
    if (!p) return std::string();
    size_t len = 0; while (len < n && p[len]) ++len;       // bounded read, no unbounded strlen
    return std::string(p, len);
}
int64_t GameBridge::NowSeconds() { return (int64_t)time(nullptr); }
void GameBridge::SetListener(Listener l) { std::lock_guard<std::mutex> g(m_); l_ = std::move(l); }
Snapshot GameBridge::Get() { std::lock_guard<std::mutex> g(m_); return s_; }
void GameBridge::Publish() {
    Snapshot copy; Listener l;
    { std::lock_guard<std::mutex> g(m_); copy = s_; l = l_; }
    if (l) l(copy);
}
void GameBridge::SetMenu() {
    { std::lock_guard<std::mutex> g(m_); s_ = Snapshot(); s_.state = GameState::GameMenu; }
    Publish();
}
void GameBridge::SetConnecting(const char* host, int port) {
    { std::lock_guard<std::mutex> g(m_);
      s_ = Snapshot(); s_.state = GameState::Connecting;
      s_.host = Cap(host); s_.hasAddress = !s_.host.empty() && port > 0 && port < 65536; s_.port = s_.hasAddress ? port : 0; }
    Publish();
}
void GameBridge::SetConnected(const char* serverName, const char* host, int port, const char* nick) {
    { std::lock_guard<std::mutex> g(m_);
      Snapshot n; n.state = GameState::Connected;
      n.serverName = Cap(serverName); n.hasServerName = !n.serverName.empty();
      n.host = Cap(host); n.hasAddress = !n.host.empty() && port > 0 && port < 65536; n.port = n.hasAddress ? port : 0;
      n.nickname = Cap(nick, 24); n.hasNickname = !n.nickname.empty();
      // Same server as before => keep the original session start (refreshes must not reset the timer).
      bool same = s_.state == GameState::Connected && s_.sessionStart > 0 &&
                  (n.hasAddress ? (s_.host == n.host && s_.port == n.port) : s_.serverName == n.serverName);
      n.sessionStart = same ? s_.sessionStart : clock();
      s_ = n; }
    Publish();
}
void GameBridge::SetDisconnected() {
    { std::lock_guard<std::mutex> g(m_); s_ = Snapshot(); s_.state = GameState::Disconnected; }
    Publish();
}
}
