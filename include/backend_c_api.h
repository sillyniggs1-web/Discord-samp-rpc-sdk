#pragma once
// Plain-C boundary between libSampDiscordRPC.so (AML plugin, default linker namespace) and
// libSampDiscordBackend.so (Discord SDK; loaded by JAVA through the dex class loader so that the SDK's
// native code and its Java glue share ONE copy of the SDK). Only C types cross this line: the two
// libraries have separate C++ runtimes, so no std:: objects may be passed.
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const char* label; const char* url; } SrpcButton;
typedef struct {
    const char* details; const char* state;
    const char* largeImage; const char* largeText;
    const char* smallImage; const char* smallText;
    long long startTimestamp;            /* unix seconds, 0 = none */
    int buttonCount;                     /* 0..2 */
    SrpcButton buttons[2];
} SrpcActivity;
#define SRPC_BACKEND_API_VERSION 2
typedef struct {
    int version;                         /* must equal SRPC_BACKEND_API_VERSION */
    int  (*init)(const char* applicationId, const char* dataDir);   /* dataDir: app-private folder for the token file */
    int  (*update)(const SrpcActivity* a);
    int  (*clear)(void);
    void (*pump)(void);
    int  (*healthy)(void);
    void (*shutdown)(void);
} SrpcBackendApi;
#ifdef __cplusplus
}
#endif
