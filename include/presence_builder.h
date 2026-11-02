#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "config.h"
#include "game_state.h"
namespace srpc {
struct PresenceData {
    bool clear = true;                       // true => remove activity
    std::string details, state;
    std::string largeImage, largeText, smallImage, smallText;
    int64_t startTimestamp = 0;              // 0 = none
    std::vector<ButtonCfg> buttons;
    bool operator==(const PresenceData& o) const {
        return clear == o.clear && details == o.details && state == o.state && largeImage == o.largeImage &&
               largeText == o.largeText && smallImage == o.smallImage && smallText == o.smallText &&
               startTimestamp == o.startTimestamp && buttons.size() == o.buttons.size() &&
               std::equal(buttons.begin(), buttons.end(), o.buttons.begin(),
                   [](const ButtonCfg& a, const ButtonCfg& b){ return a.label == b.label && a.url == b.url; });
    }
};
// Removes control chars, collapses whitespace, truncates to maxBytes on a UTF-8 boundary.
std::string SanitizeText(const std::string& in, size_t maxBytes);
PresenceData BuildPresence(const Snapshot& s, const Config& c);
}
