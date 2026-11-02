#pragma once
#include <string>
#include <vector>
namespace srpc {
struct ButtonCfg { std::string label, url; };
struct Config {
    // [General]
    bool enabled = true;
    bool showMenuActivity = true;
    bool showSessionTimer = true;
    int  updateIntervalMs = 5000;      // clamped 1000..60000
    // [Discord]
    std::string applicationId;         // decimal snowflake; empty => no Discord backend
    // [Privacy]  defaults match the desired look (server, nickname, ip:port). Set to 0 to hide.
    bool shareServerAddress = true;
    bool shareServerName = true;
    bool sharePlayerNickname = true;
    // [Assets] keys must be uploaded in the Discord Developer Portal (lowercase)
    std::string largeImage = "samp_logo", largeText = "San Andreas Multiplayer";
    std::string smallImage = "android",    smallText = "Android";
    // [Buttons] at most 2, https:// only
    std::vector<ButtonCfg> buttons;
    // [Debug]
    bool enableLogging = true;
};
// Parses INI text. Never throws; invalid values fall back to defaults; numbers are clamped.
Config ParseConfig(const std::string& text);
bool LoadConfigFile(const std::string& path, Config& out);
const char* DefaultConfigText();
}
