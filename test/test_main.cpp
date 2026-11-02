// Host-side tests (no Android, no Discord). Build: see BUILDING.md  (make -C tests)
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include "config.h"
#include "game_adapter.h"
#include "logger.h"
#include "presence_builder.h"
#include "rpc_manager.h"
static int g_fail = 0, g_n = 0;
#define CHECK(c) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
using namespace srpc;

// ---- config ----
static void ConfigTests() {
    Config d = ParseConfig("");
    CHECK(d.enabled && d.updateIntervalMs == 5000 && d.shareServerAddress && d.sharePlayerNickname);
    Config c = ParseConfig("[General]\nUpdateIntervalMs=5\n[Privacy]\nShareServerAddress=0\nSharePlayerNickname=no\n");
    CHECK(c.updateIntervalMs == 1000);                    // clamped up
    CHECK(!c.shareServerAddress && !c.sharePlayerNickname);
    c = ParseConfig("[General]\nUpdateIntervalMs=99999999\n");
    CHECK(c.updateIntervalMs == 60000);                   // clamped down
    c = ParseConfig("[General]\nUpdateIntervalMs=abc\nEnabled=maybe\n");
    CHECK(c.updateIntervalMs == 5000 && c.enabled);       // malformed => default
    c = ParseConfig("[Discord]\nApplicationId=12ab\n");
    CHECK(c.applicationId.empty());
    c = ParseConfig("[Discord]\nApplicationId=123456789012345678\n");
    CHECK(c.applicationId == "123456789012345678");
    c = ParseConfig("[Buttons]\nButton1Label=Ok\nButton1Url=http://insecure\nButton2Label=Disc\nButton2Url=https://discord.gg/x\n");
    CHECK(c.buttons.size() == 1 && c.buttons[0].label == "Disc");
    c = ParseConfig(DefaultConfigText());
    CHECK(c.enabled && c.applicationId.empty());
    Config z; CHECK(!LoadConfigFile("/nonexistent/path.ini", z));
}
// ---- presence builder ----
static Snapshot Connected() {
    Snapshot s; s.state = GameState::Connected; s.serverName = "My RP Server"; s.hasServerName = true;
    s.host = "1.2.3.4"; s.port = 7777; s.hasAddress = true; s.nickname = "Jay_Test"; s.hasNickname = true; s.sessionStart = 1000;
    return s;
}
static void PresenceTests() {
    Config c;
    PresenceData p = BuildPresence(Connected(), c);
    // target look: details = server name, state = "nick • ip:port", timer, logo
    CHECK(!p.clear && p.details == "My RP Server" && p.state == "Jay_Test \xE2\x80\xA2 1.2.3.4:7777");
    CHECK(p.startTimestamp == 1000 && p.largeImage == "samp_logo");
    c.shareServerAddress = false;
    p = BuildPresence(Connected(), c); CHECK(p.state == "Jay_Test");
    c.shareServerAddress = true; c.sharePlayerNickname = false;
    p = BuildPresence(Connected(), c); CHECK(p.state == "1.2.3.4:7777");
    c.showSessionTimer = false; CHECK(BuildPresence(Connected(), c).startTimestamp == 0);
    c.shareServerName = false; c.shareServerAddress = false;
    p = BuildPresence(Connected(), c);
    CHECK(p.details == "Playing San Andreas Multiplayer" && p.state == "Connected to a multiplayer server");
    Config c2; Snapshot m; m.state = GameState::GameMenu;
    p = BuildPresence(m, c2); CHECK(!p.clear && p.state == "In menu");
    c2.showMenuActivity = false; CHECK(BuildPresence(m, c2).clear);
    Config off; off.enabled = false; CHECK(BuildPresence(Connected(), off).clear);
    Snapshot u; CHECK(BuildPresence(u, Config()).state == "In game");      // never claims a server
    // sanitization
    CHECK(SanitizeText("  a\x01\n b  ", 50) == "a b");
    std::string longS(300, 'x'); CHECK(SanitizeText(longS, 128).size() == 128);
    std::string utf = std::string(127, 'a') + "\xC3\xA9";                  // 127 + 2-byte char = 129 bytes
    std::string t = SanitizeText(utf, 128); CHECK(t.size() == 127);        // not cut mid-sequence
    Snapshot bad = Connected(); bad.port = 70000;
    Config ca; CHECK(BuildPresence(bad, ca).state == "Jay_Test");   // invalid port dropped
    Config cb; cb.buttons.push_back({"A", "https://a"}); cb.buttons.push_back({"B", "https://b"}); cb.buttons.push_back({"C", "https://c"});
    CHECK(BuildPresence(Connected(), cb).buttons.size() == 2);
    CHECK(BuildPresence(m, cb).buttons.empty());                           // buttons only while connected
}
// ---- bridge ----
static int64_t g_now = 5000;
static int64_t FakeClock() { return g_now; }
static void BridgeTests() {
    GameBridge b; b.clock = &FakeClock;
    b.SetConnected("S1", "1.1.1.1", 7777, "nick");
    CHECK(b.Get().sessionStart == 5000);
    g_now = 6000; b.SetConnected("S1 renamed", "1.1.1.1", 7777, "nick");
    CHECK(b.Get().sessionStart == 5000);                  // refresh keeps timer
    g_now = 7000; b.SetConnected("S2", "2.2.2.2", 7777, "nick");
    CHECK(b.Get().sessionStart == 7000);                  // server switch resets timer
    b.SetDisconnected(); CHECK(b.Get().state == GameState::Disconnected && !b.Get().hasServerName);
    g_now = 8000; b.SetConnected("S2", "2.2.2.2", 7777, "nick"); CHECK(b.Get().sessionStart == 8000); // reconnect = new session
    b.SetConnected(nullptr, nullptr, -5, nullptr);
    CHECK(!b.Get().hasAddress && !b.Get().hasServerName && !b.Get().hasNickname);   // null/invalid safe
    std::string huge(5000, 'z'); b.SetConnected(huge.c_str(), "h", 1, huge.c_str());
    CHECK(b.Get().serverName.size() == 128 && b.Get().nickname.size() == 24);
}
// ---- manager with fake backend ----
struct Fake : IPresenceBackend {
    std::atomic<int> inits{0}, updates{0}, clears{0}, shutdowns{0}, initFailsLeft{0}; std::atomic<bool> healthy{true};
    std::vector<PresenceData> seen; std::mutex mu;
    const char* Name() const override { return "fake"; }
    bool Init(const std::string&) override { ++inits; if (initFailsLeft > 0) { --initFailsLeft; return false; } healthy = true; return true; }
    bool Update(const PresenceData& p) override { ++updates; std::lock_guard<std::mutex> g(mu); seen.push_back(p); return true; }
    bool Clear() override { ++clears; return true; }
    void Pump() override {}
    bool Healthy() const override { return healthy.load(); }
    void Shutdown() override { ++shutdowns; }
};
static bool WaitFor(std::function<bool()> f, int ms = 3000) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) { if (f()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    return f();
}
static void ManagerTests() {
    RpcTiming t; t.tickMs = 10; t.backoffInitialMs = 20; t.backoffMaxMs = 80; t.maxConsecutiveFailures = 4;
    Config c; c.updateIntervalMs = 30;
    { // dedupe + publish + clear on stop
        auto* f = new Fake; RpcManager m(c, std::unique_ptr<IPresenceBackend>(f), t); m.Start(); m.Start();
        Snapshot s = Connected(); m.SetSnapshot(s);
        CHECK(WaitFor([&] { return f->updates >= 1; }));
        for (int i = 0; i < 5; ++i) { m.SetSnapshot(s); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
        CHECK(f->updates == 1);                                // identical snapshots => no republish
        CHECK(f->inits == 1);                                  // idempotent Start
        s.serverName = "Other"; m.SetSnapshot(s);
        CHECK(WaitFor([&] { return f->updates == 2; }));
        m.Stop();
        CHECK(f->clears == 1 && f->shutdowns >= 1);            // cleared + shut down at exit
    }
    { // rate limiting
        Config cr = c; cr.updateIntervalMs = 200;
        auto* f = new Fake; RpcManager m(cr, std::unique_ptr<IPresenceBackend>(f), t); m.Start();
        Snapshot s = Connected();
        for (int i = 0; i < 6; ++i) { s.serverName = "S" + std::to_string(i); m.SetSnapshot(s); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
        CHECK(f->updates <= 2);                                // 120ms of changes, 200ms limit
        m.Stop();
    }
    { // init retry with backoff, then success
        auto* f = new Fake; f->initFailsLeft = 2;
        RpcManager m(c, std::unique_ptr<IPresenceBackend>(f), t); m.Start(); m.SetSnapshot(Connected());
        CHECK(WaitFor([&] { return f->updates >= 1; }));
        CHECK(f->inits == 3);
        m.Stop();
    }
    { // bounded retries: give up, resume on state change
        auto* f = new Fake; f->initFailsLeft = 1000;
        RpcManager m(c, std::unique_ptr<IPresenceBackend>(f), t); m.Start(); m.SetSnapshot(Connected());
        CHECK(WaitFor([&] { return m.GaveUp(); }));
        int n = f->inits; std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(f->inits == n && n == 4);                        // stopped retrying
        f->initFailsLeft = 0; Snapshot s = Connected(); s.serverName = "New"; m.SetSnapshot(s);
        CHECK(WaitFor([&] { return f->updates >= 1; }));       // resumed after game-state change
        m.Stop();
    }
    { // unhealthy backend => reinit and republish
        auto* f = new Fake; RpcManager m(c, std::unique_ptr<IPresenceBackend>(f), t); m.Start(); m.SetSnapshot(Connected());
        CHECK(WaitFor([&] { return f->updates >= 1; }));
        f->healthy = false;
        CHECK(WaitFor([&] { return f->inits >= 2 && f->updates >= 2; }));
        m.Stop();
    }
    { // menu: Unknown snapshot publishes generic presence; disabled config publishes nothing
        auto* f = new Fake; RpcManager m(c, std::unique_ptr<IPresenceBackend>(f), t); m.Start(); m.SetSnapshot(Snapshot());
        CHECK(WaitFor([&] { return f->updates >= 1; }));
        { std::lock_guard<std::mutex> g(f->mu); CHECK(!f->seen.empty() && f->seen[0].state == "In game"); }
        m.Stop();
        Config off = c; off.enabled = false;
        auto* g = new Fake; RpcManager m2(off, std::unique_ptr<IPresenceBackend>(g), t); m2.Start(); m2.SetSnapshot(Connected());
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        CHECK(g->updates == 0); m2.Stop();
    }
}
int main() {
    SetLoggingEnabled(false);
    ConfigTests(); PresenceTests(); BridgeTests(); ManagerTests();
    printf("%d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
