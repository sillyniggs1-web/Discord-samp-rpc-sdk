// libSampDiscordBackend.so: the part that links the proprietary Discord Social SDK.
// Loaded by JAVA (net.sampdiscordrpc.Boot.init -> System.loadLibrary) so the SDK's native code and its Java glue
// share one class loader. The AML plugin only sees the plain-C table returned by nativeApi().
//
// AUTH (required by the SDK: presence is only accepted once the client status is Ready):
//   stored token -> UpdateToken -> Connect                (fast path, token file in the app-private dir)
//   otherwise    -> GetTokenFromDevice (OAuth2 device flow: no redirect URI, no AndroidManifest entry) -> store -> UpdateToken -> Connect
// Client::Authorize is NOT used: on Android it needs com.discord.socialsdk.AuthenticationActivity in the game's manifest.
// Requires the Discord application to have "Public Client" enabled. Calls follow docs.discord.com (C++ getting-started +
// account-linking-on-consoles). STATUS: never linked or run; GetTokenFromDevice on Android is documented for consoles only.
#include <android/log.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>
#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include "backend_c_api.h"

namespace {
void LogF(int prio, const char* fmt, const char* a = "") { __android_log_print(prio, "SampDiscordRPC", fmt, a); }

std::shared_ptr<discordpp::Client> g_client;
std::atomic<bool> g_healthy{false};
bool g_ready = false;              // status == Ready (only touched from the worker thread: Init/Update/Pump/Shutdown)
bool g_authRunning = false;        // a GetTokenFromDevice call is in flight
bool g_usedStoredToken = false;
time_t g_nextAuthAt = 0;           // when to (re)start the device flow; 0 = not scheduled
uint64_t g_appId = 0;
std::string g_tokenPath;

// Latest activity requested while not Ready yet (applied when Ready). Plain copies, no pointers into the caller.
struct Pending {
    bool have = false, clear = false;
    std::string details, state, largeImage, largeText, smallImage, smallText;
    long long ts = 0;
    std::vector<std::pair<std::string, std::string>> buttons;
} g_pending;

std::string S(const char* s) { return s ? std::string(s) : std::string(); }

// ---- token file: "v1\n<expiry unix>\n<access token>\n", mode 0600, never logged ----
bool LoadToken(std::string& token) {
    FILE* f = fopen(g_tokenPath.c_str(), "rb"); if (!f) return false;
    char ver[8] = {0}; long long exp = 0; char tok[4096] = {0};
    bool ok = fscanf(f, "%7s\n%lld\n%4095s", ver, &exp, tok) == 3 && std::string(ver) == "v1";
    fclose(f);
    if (!ok || exp <= (long long)time(nullptr) + 300) return false;     // missing, malformed or about to expire
    token = tok; return true;
}
void SaveToken(const std::string& token, int32_t expiresIn) {
    long long exp = (long long)time(nullptr) + (expiresIn > 0 ? expiresIn : 3600);
    FILE* f = fopen(g_tokenPath.c_str(), "wb"); if (!f) { LogF(ANDROID_LOG_WARN, "cannot store token file"); return; }
    chmod(g_tokenPath.c_str(), 0600);
    fprintf(f, "v1\n%lld\n%s\n", exp, token.c_str());
    fclose(f);
}
void DropToken() { if (!g_tokenPath.empty()) unlink(g_tokenPath.c_str()); }

void ApplyActivity(const Pending& p);

void ConnectWithToken(const std::string& accessToken) {
    auto client = g_client; if (!client) return;
    client->UpdateToken(discordpp::AuthorizationTokenType::Bearer, accessToken, [client](discordpp::ClientResult result) {
        if (result.Successful()) { LogF(ANDROID_LOG_INFO, "token accepted, connecting"); client->Connect(); }
        else { LogF(ANDROID_LOG_WARN, "UpdateToken failed: %s", result.Error().c_str()); DropToken(); g_nextAuthAt = time(nullptr); }
    });
}

void StartDeviceAuth() {
    auto client = g_client; if (!client || g_authRunning) return;
    g_authRunning = true; g_nextAuthAt = 0; g_usedStoredToken = false;
    LogF(ANDROID_LOG_INFO, "starting Discord device authorization (approve it at discord.com/activate)");
    discordpp::DeviceAuthorizationArgs args{};
    args.SetClientId(g_appId);
    args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
    client->GetTokenFromDevice(args, [client](discordpp::ClientResult result, std::string accessToken, std::string refreshToken,
                                              discordpp::AuthorizationTokenType tokenType, int32_t expiresIn, std::string scope) {
        (void)refreshToken; (void)tokenType; (void)scope;
        g_authRunning = false;
        if (!result.Successful()) {
            LogF(ANDROID_LOG_WARN, "device authorization failed: %s (retry in 60 s)", result.Error().c_str());
            g_nextAuthAt = time(nullptr) + 60; return;
        }
        LogF(ANDROID_LOG_INFO, "device authorization succeeded");
        SaveToken(accessToken, expiresIn);
        ConnectWithToken(accessToken);
    });
}

void ApplyActivity(const Pending& p) {
    auto client = g_client; if (!client) return;
    if (p.clear) { client->ClearRichPresence(); return; }
    discordpp::Activity a;
    a.SetType(discordpp::ActivityTypes::Playing);
    if (!p.details.empty()) a.SetDetails(p.details);
    if (!p.state.empty()) a.SetState(p.state);
    if (p.ts > 0) { discordpp::ActivityTimestamps ts; ts.SetStart((uint64_t)p.ts); a.SetTimestamps(ts); }
    discordpp::ActivityAssets as;
    if (!p.largeImage.empty()) { as.SetLargeImage(p.largeImage); as.SetLargeText(p.largeText); }
    if (!p.smallImage.empty()) { as.SetSmallImage(p.smallImage); as.SetSmallText(p.smallText); }
    a.SetAssets(as);
    for (const auto& b : p.buttons) { discordpp::ActivityButton ab; ab.SetLabel(b.first); ab.SetUrl(b.second); a.AddButton(ab); }
    client->UpdateRichPresence(a, [](discordpp::ClientResult r) {
        if (!r.Successful()) { LogF(ANDROID_LOG_WARN, "UpdateRichPresence failed: %s", r.Error().c_str()); g_healthy = false; }
        else LogF(ANDROID_LOG_INFO, "rich presence updated");
    });
}

int Init(const char* appId, const char* dataDir) {
    if (!appId || !*appId) return 0;
    try {
        g_appId = strtoull(appId, nullptr, 10);
        g_tokenPath = S(dataDir) + "/discord_token.dat";
        g_ready = false; g_authRunning = false; g_pending = Pending(); g_nextAuthAt = 0;
        g_client = std::make_shared<discordpp::Client>();
        g_client->SetApplicationId(g_appId);
        g_client->AddLogCallback([](auto message, auto severity) {
            __android_log_print(ANDROID_LOG_INFO, "SampDiscordSDK", "[%s] %s", discordpp::EnumToString(severity), std::string(message).c_str());
        }, discordpp::LoggingSeverity::Info);
        auto client = g_client;
        client->SetStatusChangedCallback([client](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
            __android_log_print(ANDROID_LOG_INFO, "SampDiscordRPC", "SDK status: %s", discordpp::Client::StatusToString(status).c_str());
            if (status == discordpp::Client::Status::Ready) {
                g_ready = true; g_healthy = true;
                if (g_pending.have) { Pending p = g_pending; g_pending.have = false; ApplyActivity(p); }
            } else {
                g_ready = false;
                if (error != discordpp::Client::Error::None) {
                    __android_log_print(ANDROID_LOG_WARN, "SampDiscordRPC", "SDK connection error: %s (detail %d)",
                                        discordpp::Client::ErrorToString(error).c_str(), (int)errorDetail);
                    if (g_usedStoredToken) { DropToken(); g_usedStoredToken = false; g_nextAuthAt = time(nullptr); }   // stored token likely stale
                }
            }
        });
        g_healthy = true;
        std::string stored;
        if (LoadToken(stored)) { g_usedStoredToken = true; LogF(ANDROID_LOG_INFO, "using stored token"); ConnectWithToken(stored); }
        else StartDeviceAuth();
        return 1;
    } catch (...) { g_client.reset(); return 0; }
}

int Update(const SrpcActivity* a) {
    if (!g_client || !a) return 0;
    try {
        Pending p; p.have = true;
        p.details = S(a->details); p.state = S(a->state);
        p.largeImage = S(a->largeImage); p.largeText = S(a->largeText);
        p.smallImage = S(a->smallImage); p.smallText = S(a->smallText);
        p.ts = a->startTimestamp;
        for (int i = 0; i < a->buttonCount && i < 2; ++i) p.buttons.emplace_back(S(a->buttons[i].label), S(a->buttons[i].url));
        if (g_ready) ApplyActivity(p); else g_pending = p;      // not Ready yet (e.g. waiting for approval): keep only the latest
        return 1;                                               // accepted; never a reason to tear the client down
    } catch (...) { return 0; }
}
int Clear() {
    if (!g_client) return 0;
    try { if (g_ready) ApplyActivity([]{ Pending c; c.clear = true; return c; }()); else { g_pending = Pending(); g_pending.have = true; g_pending.clear = true; } return 1; }
    catch (...) { return 0; }
}
void Pump() {
    try {
        discordpp::RunCallbacks();
        if (g_nextAuthAt != 0 && !g_authRunning && time(nullptr) >= g_nextAuthAt) StartDeviceAuth();
    } catch (...) {}
}
int Healthy() { return g_healthy.load() ? 1 : 0; }
void Shutdown() { g_client.reset(); g_ready = false; g_authRunning = false; g_healthy = false; }

const SrpcBackendApi kApi = { SRPC_BACKEND_API_VERSION, &Init, &Update, &Clear, &Pump, &Healthy, &Shutdown };
}  // namespace

extern "C" __attribute__((visibility("default")))
jlong Java_net_sampdiscordrpc_Boot_nativeApi(JNIEnv*, jclass) { return (jlong)(intptr_t)&kApi; }
