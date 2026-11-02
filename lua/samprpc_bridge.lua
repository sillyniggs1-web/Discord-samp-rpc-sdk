script_name("SampDiscordRPC Bridge")
script_author("HEXED Studio")
script_version("1.0")
-- Feeds server name / IP:port / nickname to the SampDiscordRPC plugin (MonetLoader / LuaJIT).
-- STATUS: written from the calls that Mimgui_ScoreBoard.lua already uses on your device
--         (sampGetCurrentServerName, sampGetCurrentServerAddress, sampGetPlayerIdByCharHandle,
--         sampGetPlayerNickname). NOT run on a device. Still UNVERIFIED: that this script can
--         reach the plugin's exported SampRPC_* symbols (see load_plugin below; check the log lines).
require "lib.moonloader"
local ffi = require("ffi")
local encoding = require("encoding")
encoding.default = "CP1251"
local u8 = encoding.UTF8

ffi.cdef[[
void SampRPC_SetMenu();
void SampRPC_SetConnecting(const char* host, int port);
void SampRPC_SetConnected(const char* name, const char* host, int port, const char* nick);
void SampRPC_SetDisconnected();
]]

local function log(msg) print("[samprpc_bridge] " .. msg) end

-- Try, in order: symbols already visible process-wide, then the loaded library by name.
local function load_plugin()
    local ok, res = pcall(function() local f = ffi.C.SampRPC_SetMenu return f and ffi.C end)
    if ok and res then log("plugin reached through ffi.C") return res end
    for _, name in ipairs({ "libSampDiscordRPC.so", "SampDiscordRPC" }) do
        local ok2, lib = pcall(ffi.load, name)
        if ok2 and lib then
            local ok3 = pcall(function() return lib.SampRPC_SetMenu end)
            if ok3 then log("plugin reached through ffi.load(" .. name .. ")") return lib end
        end
    end
    return nil
end

-- Server names from SA-MP are CP1251; Discord needs UTF-8.
local function to_utf8(s)
    if type(s) ~= "string" or s == "" then return "" end
    local ok, r = pcall(u8, s)
    return ok and r or s
end

local function get_nickname()
    local ok, id = sampGetPlayerIdByCharHandle(PLAYER_PED)
    if ok and type(id) == "number" and id >= 0 and id <= sampGetMaxPlayerId() then
        local nick = sampGetPlayerNickname(id)
        if type(nick) == "string" and nick ~= "" then return nick end
    end
    return nil
end

local function get_server()
    local okN, name = pcall(sampGetCurrentServerName)
    local okA, ip, port = pcall(sampGetCurrentServerAddress)
    if not okN then name = nil end
    if not okA then ip, port = nil, nil end
    return name, ip, tonumber(port)
end

function main()
    if not isSampLoaded() or not isSampfuncsLoaded() then return end
    while not isSampAvailable() do wait(0) end

    local lib = load_plugin()
    if not lib then
        log("plugin symbols not reachable; Discord status will not update (is libSampDiscordRPC.so loaded by the modloader?)")
        return
    end

    local last = ""
    while true do
        wait(1000)
        local nick = get_nickname()
        local name, ip, port = get_server()
        local state
        if nick and ip and ip ~= "" and port and port > 0 then
            state = ("C|%s|%s|%d|%s"):format(tostring(name), ip, port, nick)
            if state ~= last then
                lib.SampRPC_SetConnected(to_utf8(name), ip, port, nick)
                log("connected: " .. tostring(name) .. " " .. ip .. ":" .. port)
            end
        else
            state = "M"
            if state ~= last then lib.SampRPC_SetMenu() log("menu") end
        end
        last = state
    end
end
