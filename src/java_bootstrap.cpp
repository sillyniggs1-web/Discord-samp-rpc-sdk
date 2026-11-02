// Java bootstrap for the Discord Social SDK inside the Nezuko process.
// STATUS: WRITTEN WITHOUT AN NDK OR A DEVICE. Compiled only by the GitHub workflow; never run.
// Every step logs its result under logcat tag "SampDiscordRPC" so the first device run shows where it stops.
#ifdef SAMPRPC_WITH_DISCORD_SDK
#include "java_bootstrap.h"
#include <dlfcn.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "backend_c_api.h"
#include "logger.h"

namespace srpc {
namespace {
void* g_vmFallback = nullptr;

JavaVM* GetVm() {
    typedef jint (*GetVmsFn)(JavaVM**, jsize, jsize*);
    if (GetVmsFn fn = (GetVmsFn)dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs")) {
        JavaVM* vm = nullptr; jsize n = 0;
        if (fn(&vm, 1, &n) == JNI_OK && n > 0 && vm) return vm;
    }
    return (JavaVM*)g_vmFallback;
}

// Logs and clears a pending Java exception. Returns true if there was one.
bool Exc(JNIEnv* env, const char* where) {
    if (!env->ExceptionCheck()) return false;
    jthrowable t = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string text = "(no description)";
    if (t) {
        jclass tc = env->GetObjectClass(t);
        jmethodID ts = tc ? env->GetMethodID(tc, "toString", "()Ljava/lang/String;") : nullptr;
        if (ts) {
            jstring s = (jstring)env->CallObjectMethod(t, ts);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (s) { const char* c = env->GetStringUTFChars(s, nullptr); if (c) { text = c; env->ReleaseStringUTFChars(s, c); } }
        } else if (env->ExceptionCheck()) env->ExceptionClear();
    }
    SRPC_ERR("java exception at %s: %s", where, text.c_str());
    return true;
}

std::string JString(JNIEnv* env, jstring s) {
    std::string r;
    if (!s) return r;
    const char* c = env->GetStringUTFChars(s, nullptr);
    if (c) { r = c; env->ReleaseStringUTFChars(s, c); }
    return r;
}

// Finds a live Activity through ActivityThread.mActivities (hidden API; reflection may be blocked on some
// Android versions -> logged). Prefers a non-paused activity. Returns a local ref or nullptr.
jobject FindActivity(JNIEnv* env) {
    jclass at = env->FindClass("android/app/ActivityThread");
    if (!at) { Exc(env, "FindClass ActivityThread"); return nullptr; }
    jmethodID cur = env->GetStaticMethodID(at, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!cur) { Exc(env, "currentActivityThread"); return nullptr; }
    jobject thread = env->CallStaticObjectMethod(at, cur);
    if (Exc(env, "currentActivityThread()") || !thread) return nullptr;
    jfieldID fm = env->GetFieldID(at, "mActivities", "Landroid/util/ArrayMap;");
    if (!fm) { Exc(env, "mActivities field"); return nullptr; }
    jobject map = env->GetObjectField(thread, fm);
    if (!map) return nullptr;
    jclass mc = env->GetObjectClass(map);
    jmethodID sizeM = env->GetMethodID(mc, "size", "()I");
    jmethodID valueAt = env->GetMethodID(mc, "valueAt", "(I)Ljava/lang/Object;");
    if (!sizeM || !valueAt) { Exc(env, "ArrayMap methods"); return nullptr; }
    jint n = env->CallIntMethod(map, sizeM);
    jobject best = nullptr; bool bestPaused = true;
    for (jint i = 0; i < n; ++i) {
        jobject rec = env->CallObjectMethod(map, valueAt, i);
        if (!rec) continue;
        jclass rc = env->GetObjectClass(rec);
        jfieldID fa = env->GetFieldID(rc, "activity", "Landroid/app/Activity;");
        jfieldID fp = env->GetFieldID(rc, "paused", "Z");
        if (!fa) { Exc(env, "record.activity field"); continue; }
        if (!fp && env->ExceptionCheck()) env->ExceptionClear();
        jobject act = env->GetObjectField(rec, fa);
        if (!act) continue;
        bool paused = fp ? env->GetBooleanField(rec, fp) == JNI_TRUE : false;
        if (!best || (bestPaused && !paused)) { best = act; bestPaused = paused; }
    }
    SRPC_INFO("activity search: %d record(s), %s", (int)n, best ? "found one" : "none yet");
    return best;
}

std::string DirOfThisLib() {
    Dl_info info;
    if (dladdr((void*)&DirOfThisLib, &info) && info.dli_fname) {
        std::string p = info.dli_fname; size_t s = p.rfind('/');
        if (s != std::string::npos) return p.substr(0, s);
    }
    return std::string();
}

bool FileExists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode); }

bool ReadFile(const std::string& p, std::string& out) {
    FILE* f = fopen(p.c_str(), "rb"); if (!f) return false;
    char buf[1 << 16]; size_t n; out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f); return true;
}

// Copies src -> dst unless dst already has identical bytes. Writes to a temp file then renames, so a
// read-only destination from a previous run can be replaced. dex files must be read-only (Android 14+).
bool Stage(const std::string& src, const std::string& dst, mode_t mode) {
    std::string a, b;
    if (!ReadFile(src, a)) { SRPC_ERR("cannot read %s", src.c_str()); return false; }
    if (ReadFile(dst, b) && a == b) { chmod(dst.c_str(), mode); return true; }
    std::string tmp = dst + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { SRPC_ERR("cannot write %s", tmp.c_str()); return false; }
    bool ok = fwrite(a.data(), 1, a.size(), f) == a.size();
    ok = (fclose(f) == 0) && ok;
    if (!ok) { unlink(tmp.c_str()); SRPC_ERR("short write to %s", tmp.c_str()); return false; }
    chmod(tmp.c_str(), mode);
    if (rename(tmp.c_str(), dst.c_str()) != 0) { unlink(tmp.c_str()); SRPC_ERR("rename to %s failed", dst.c_str()); return false; }
    return true;
}

// Counts distinct mappings of the SDK library; 2+ means the process loaded it twice (the failure mode this
// design tries to avoid). Diagnostic only.
void LogSdkCopies() {
    FILE* f = fopen("/proc/self/maps", "r"); if (!f) return;
    char line[512]; int bases = 0;
    while (fgets(line, sizeof line, f)) {
        if (!strstr(line, "libdiscord_partner_sdk.so")) continue;
        unsigned long lo = 0, hi = 0, off = 0; char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s %lx", &lo, &hi, perms, &off) == 4 && off == 0) ++bases;
    }
    fclose(f);
    if (bases == 0) SRPC_WARN("SDK library not mapped (yet?)");
    else if (bases == 1) SRPC_INFO("SDK library mapped once (single copy)");
    else SRPC_WARN("SDK library mapped %d times: DOUBLE COPY, native calls and Java glue may not share state", bases);
}

const SrpcBackendApi* Bootstrap(JNIEnv* env, std::string& err, std::string& dataDirOut) {
    jobject activity = FindActivity(env);
    if (!activity) { err = "no Activity available yet"; return nullptr; }

    jclass ctxCls = env->FindClass("android/content/Context");
    jmethodID getFiles = env->GetMethodID(ctxCls, "getFilesDir", "()Ljava/io/File;");
    jmethodID getLoader = env->GetMethodID(ctxCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!getFiles || !getLoader) { Exc(env, "Context methods"); err = "Context methods missing"; return nullptr; }
    jobject filesDir = env->CallObjectMethod(activity, getFiles);
    if (Exc(env, "getFilesDir") || !filesDir) { err = "getFilesDir failed"; return nullptr; }
    jclass fileCls = env->FindClass("java/io/File");
    jmethodID absPath = env->GetMethodID(fileCls, "getAbsolutePath", "()Ljava/lang/String;");
    std::string stage = JString(env, (jstring)env->CallObjectMethod(filesDir, absPath));
    if (Exc(env, "getAbsolutePath") || stage.empty()) { err = "no files dir"; return nullptr; }
    stage += "/sampdiscordrpc";
    mkdir(stage.c_str(), 0700);
    std::string odex = stage + "/odex"; mkdir(odex.c_str(), 0700);

    // Source: <dir of libSampDiscordRPC.so>/SampDiscordRPC/ (preferred) or next to the plugin.
    std::string base = DirOfThisLib(), src;
    for (const std::string& c : { base + "/SampDiscordRPC", base }) if (FileExists(c + "/classes.dex")) { src = c; break; }
    if (src.empty()) { err = "classes.dex not found next to the plugin (" + base + ")"; return nullptr; }
    if (!Stage(src + "/classes.dex", stage + "/classes.dex", 0444) ||
        !Stage(src + "/libSampDiscordBackend.so", stage + "/libSampDiscordBackend.so", 0555) ||
        !Stage(src + "/libdiscord_partner_sdk.so", stage + "/libdiscord_partner_sdk.so", 0555)) {
        err = "staging files failed (missing in " + src + "?)"; return nullptr;
    }
    SRPC_INFO("staged dex + native libs into the app's private files dir");
    dataDirOut = stage;

    jobject parent = env->CallObjectMethod(activity, getLoader);
    if (Exc(env, "getClassLoader") || !parent) { err = "no parent class loader"; return nullptr; }
    jclass dexCls = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID dexCtor = dexCls ? env->GetMethodID(dexCls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V") : nullptr;
    if (!dexCtor) { Exc(env, "DexClassLoader ctor"); err = "DexClassLoader unavailable"; return nullptr; }
    jstring jDex = env->NewStringUTF((stage + "/classes.dex").c_str());
    jstring jOpt = env->NewStringUTF(odex.c_str());
    jstring jLib = env->NewStringUTF(stage.c_str());
    jobject loader = env->NewObject(dexCls, dexCtor, jDex, jOpt, jLib, parent);
    if (Exc(env, "new DexClassLoader") || !loader) { err = "DexClassLoader creation failed"; return nullptr; }

    jclass loaderCls = env->FindClass("java/lang/ClassLoader");
    jmethodID loadClass = env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jName = env->NewStringUTF("net.sampdiscordrpc.Boot");
    jclass boot = (jclass)env->CallObjectMethod(loader, loadClass, jName);
    if (Exc(env, "loadClass Boot") || !boot) { err = "Boot class not found in classes.dex"; return nullptr; }

    jmethodID init = env->GetStaticMethodID(boot, "init", "(Landroid/app/Activity;)V");
    jmethodID api = env->GetStaticMethodID(boot, "nativeApi", "()J");
    if (!init || !api) { Exc(env, "Boot methods"); err = "Boot.init/nativeApi missing"; return nullptr; }
    env->CallStaticVoidMethod(boot, init, activity);
    if (Exc(env, "Boot.init (loadLibrary / setEngineActivity)")) { err = "Boot.init threw"; return nullptr; }
    SRPC_INFO("Boot.init OK (backend + SDK libraries loaded by the dex loader)");
    LogSdkCopies();
    jlong addr = env->CallStaticLongMethod(boot, api);
    if (Exc(env, "Boot.nativeApi") || addr == 0) { err = "nativeApi returned nothing"; return nullptr; }
    const SrpcBackendApi* tbl = (const SrpcBackendApi*)(intptr_t)addr;
    if (tbl->version != SRPC_BACKEND_API_VERSION) { err = "backend API version mismatch"; return nullptr; }
    return tbl;
}

class DeferredDiscordBackend : public IPresenceBackend {
public:
    const char* Name() const override { return "discord-social-sdk (java bootstrap)"; }
    bool Init(const std::string& appId) override {
        if (!api_) {
            JavaVM* vm = GetVm();
            if (!vm) { SRPC_ERR("no JavaVM"); return false; }
            JNIEnv* env = nullptr; bool attached = false;
            if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
                if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) { SRPC_ERR("AttachCurrentThread failed"); return false; }
                attached = true;
            }
            if (!env) return false;
            std::string err; const SrpcBackendApi* tbl = nullptr;
            if (env->PushLocalFrame(256) == 0) { tbl = Bootstrap(env, err, dataDir_); env->PopLocalFrame(nullptr); }
            if (attached) vm->DetachCurrentThread();
            if (!tbl) { SRPC_WARN("java bootstrap not complete: %s (will retry)", err.c_str()); return false; }
            api_ = tbl;
        }
        return api_->init(appId.c_str(), dataDir_.c_str()) != 0;
    }
    bool Update(const PresenceData& p) override {
        if (!api_) return false;
        SrpcActivity a; memset(&a, 0, sizeof a);
        a.details = p.details.c_str(); a.state = p.state.c_str();
        a.largeImage = p.largeImage.c_str(); a.largeText = p.largeText.c_str();
        a.smallImage = p.smallImage.c_str(); a.smallText = p.smallText.c_str();
        a.startTimestamp = (long long)p.startTimestamp;
        for (const auto& b : p.buttons) { if (a.buttonCount >= 2) break; a.buttons[a.buttonCount].label = b.label.c_str(); a.buttons[a.buttonCount].url = b.url.c_str(); ++a.buttonCount; }
        return api_->update(&a) != 0;
    }
    bool Clear() override { return api_ && api_->clear() != 0; }
    void Pump() override { if (api_) api_->pump(); }
    bool Healthy() const override { return api_ && api_->healthy() != 0; }
    void Shutdown() override { if (api_) api_->shutdown(); }
private:
    const SrpcBackendApi* api_ = nullptr;
    std::string dataDir_;
};
}  // namespace

void SetJavaVmFallback(void* vm) { g_vmFallback = vm; }
IPresenceBackend* CreateDeferredDiscordBackend() { return new DeferredDiscordBackend(); }
}  // namespace srpc
#endif
