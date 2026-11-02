#pragma once
// Minimal logger. Android: logcat tag "SampDiscordRPC". Host: stderr. Never log addresses/nicknames here.
namespace srpc {
void SetLoggingEnabled(bool on);
void Log(int level /*0=info 1=warn 2=error*/, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
}
#define SRPC_INFO(...)  ::srpc::Log(0, __VA_ARGS__)
#define SRPC_WARN(...)  ::srpc::Log(1, __VA_ARGS__)
#define SRPC_ERR(...)   ::srpc::Log(2, __VA_ARGS__)
