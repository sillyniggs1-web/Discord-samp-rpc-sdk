#pragma once
#include "presence_backend.h"
namespace srpc {
// Optional fallback JavaVM* (from AML's GetJavaVM). The plugin first tries JNI_GetCreatedJavaVMs itself.
void SetJavaVmFallback(void* vm);
// Backend that, on its first Init() (worker thread, retried with backoff), does the Java bootstrap:
// find Activity -> stage files -> DexClassLoader -> Boot.init() -> fetch the C API table of the backend lib.
IPresenceBackend* CreateDeferredDiscordBackend();
}
