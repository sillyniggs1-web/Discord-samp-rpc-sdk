#include "config.h"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
namespace srpc {
static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool ParseBool(const std::string& v, bool def) {
    std::string l = Lower(v);
    if (l == "1" || l == "true" || l == "yes" || l == "on") return true;
    if (l == "0" || l == "false" || l == "no" || l == "off") return false;
    return def;
}
static int ParseInt(const std::string& v, int def, int lo, int hi) {
    if (v.empty() || v.size() > 9) return def;
    char* end = nullptr;
    long x = strtol(v.c_str(), &end, 10);
    if (!end || *end != '\0') return def;
    return x < lo ? lo : x > hi ? hi : (int)x;
}
static bool AllDigits(const std::string& s) {
    if (s.empty() || s.size() > 20) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}
Config ParseConfig(const std::string& text) {
    Config c;
    std::string section, b1l, b1u, b2l, b2u;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            size_t e = line.find(']');
            if (e != std::string::npos) section = Lower(Trim(line.substr(1, e - 1)));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Lower(Trim(line.substr(0, eq))), v = Trim(line.substr(eq + 1));
        if (section == "general") {
            if (k == "enabled") c.enabled = ParseBool(v, c.enabled);
            else if (k == "showmenuactivity") c.showMenuActivity = ParseBool(v, c.showMenuActivity);
            else if (k == "showsessiontimer") c.showSessionTimer = ParseBool(v, c.showSessionTimer);
            else if (k == "updateintervalms") c.updateIntervalMs = ParseInt(v, c.updateIntervalMs, 1000, 60000);
        } else if (section == "discord") {
            if (k == "applicationid") c.applicationId = AllDigits(v) ? v : std::string();
        } else if (section == "privacy") {
            if (k == "shareserveraddress") c.shareServerAddress = ParseBool(v, c.shareServerAddress);
            else if (k == "shareservername") c.shareServerName = ParseBool(v, c.shareServerName);
            else if (k == "shareplayernickname") c.sharePlayerNickname = ParseBool(v, c.sharePlayerNickname);
        } else if (section == "assets") {
            if (k == "largeimage") c.largeImage = v;
            else if (k == "largetext") c.largeText = v;
            else if (k == "smallimage") c.smallImage = v;
            else if (k == "smalltext") c.smallText = v;
        } else if (section == "buttons") {
            if (k == "button1label") b1l = v; else if (k == "button1url") b1u = v;
            else if (k == "button2label") b2l = v; else if (k == "button2url") b2u = v;
        } else if (section == "debug") {
            if (k == "enablelogging") c.enableLogging = ParseBool(v, c.enableLogging);
        }
    }
    auto addBtn = [&](const std::string& l, const std::string& u) {
        // Only user-supplied, https destinations; nothing invented.
        if (!l.empty() && u.rfind("https://", 0) == 0 && u.size() <= 512) c.buttons.push_back({l, u});
    };
    addBtn(b1l, b1u); addBtn(b2l, b2u);
    return c;
}
bool LoadConfigFile(const std::string& path, Config& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    std::string t = ss.str();
    if (t.size() > 64 * 1024) return false;   // refuse absurd files
    out = ParseConfig(t);
    return true;
}
const char* DefaultConfigText() {
    return
"; SampDiscordRPC.ini\n"
"[General]\nEnabled=1\nShowMenuActivity=1\nShowSessionTimer=1\nUpdateIntervalMs=5000\n\n"
"[Discord]\n; Your Discord application ID (digits only). Required for real presence.\nApplicationId=\n\n"
"[Privacy]\nShareServerName=1\nShareServerAddress=1\nSharePlayerNickname=1\n\n"
"[Assets]\nLargeImage=samp_logo\nLargeText=San Andreas Multiplayer\nSmallImage=android\nSmallText=Android\n\n"
"[Buttons]\n; https:// only, max 2\n;Button1Label=Discord Community\n;Button1Url=https://discord.gg/yourinvite\n\n"
"[Debug]\nEnableLogging=1\n";
}
}
