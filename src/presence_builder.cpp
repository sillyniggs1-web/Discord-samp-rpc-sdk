#include "presence_builder.h"
namespace srpc {
std::string SanitizeText(const std::string& in, size_t maxBytes) {
    std::string out; out.reserve(in.size());
    bool lastSpace = true;
    for (unsigned char ch : in) {
        if (ch < 0x20 || ch == 0x7F) ch = ' ';
        if (ch == ' ') { if (lastSpace) continue; lastSpace = true; } else lastSpace = false;
        out.push_back((char)ch);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.size() > maxBytes) {
        size_t n = maxBytes;
        while (n > 0 && ((unsigned char)out[n] & 0xC0) == 0x80) --n;   // don't cut a UTF-8 sequence
        out.resize(n);
        while (!out.empty() && out.back() == ' ') out.pop_back();
    }
    return out;
}
// Text-field cap 128 bytes (conservative vs Discord's 128 chars); image keys 256; button label 32.
static const size_t kText = 128;
PresenceData BuildPresence(const Snapshot& s, const Config& c) {
    PresenceData p;
    if (!c.enabled) return p;                           // clear
    auto fillAssets = [&]() {
        p.largeImage = SanitizeText(c.largeImage, 256); p.largeText = SanitizeText(c.largeText, kText);
        p.smallImage = SanitizeText(c.smallImage, 256); p.smallText = SanitizeText(c.smallText, kText);
    };
    switch (s.state) {
    case GameState::Unknown:
    case GameState::GameMenu:
    case GameState::Disconnected:
    case GameState::Disconnecting:
        if (!c.showMenuActivity) return p;              // clear
        p.clear = false;
        p.details = "Playing San Andreas Multiplayer";
        p.state = (s.state == GameState::Unknown) ? "In game" : "In menu";   // never claims a server
        fillAssets();
        break;
    case GameState::Connecting:
        p.clear = false;
        p.details = "Playing San Andreas Multiplayer";
        p.state = "Connecting to a server";
        fillAssets();
        break;
    case GameState::Connected: {
        // Layout (matches the desktop SA-MP look):
        //   details: <server name>            state: <nickname> • <ip:port>
        p.clear = false;
        p.details = (c.shareServerName && s.hasServerName && !s.serverName.empty())
                        ? s.serverName : "Playing San Andreas Multiplayer";
        std::string st;
        if (c.sharePlayerNickname && s.hasNickname && !s.nickname.empty()) st = s.nickname;
        if (c.shareServerAddress && s.hasAddress && !s.host.empty() && s.port > 0 && s.port < 65536) {
            if (!st.empty()) st += " \xE2\x80\xA2 ";       // " • "
            st += s.host + ":" + std::to_string(s.port);
        }
        p.state = st.empty() ? "Connected to a multiplayer server" : st;
        fillAssets();
        if (c.showSessionTimer && s.sessionStart > 0) p.startTimestamp = s.sessionStart;
        for (const auto& bt : c.buttons) {
            if (p.buttons.size() >= 2) break;
            p.buttons.push_back({SanitizeText(bt.label, 32), bt.url});
        }
        break; }
    }
    p.details = SanitizeText(p.details, kText);
    p.state = SanitizeText(p.state, kText);
    // Discord requires >=2 chars for these fields; omit rather than send invalid data.
    if (p.details.size() < 2) p.details.clear();
    if (p.state.size() < 2) p.state.clear();
    return p;
}
}
