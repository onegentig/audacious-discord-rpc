/**
 * @file audacious-discord-rpc.hpp
 * @brief Discord Rich Presence plugin for Audacious (header)
 * @version 2.2
 * @author onegen <onegen@onegen.dev>
 * @author Derzsi Dániel <daniel@tohka.us>
 * @date 2025-11-22 (last modified)
 *
 * @license MIT
 * @copyright Copyright (c) 2024–2025 onegen
 *                          2018–2022 Derzsi Dániel
 *
 */

#pragma once
#include <discord-rpc.hpp>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>
#include <stdint.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef _WIN32
#     include <shellapi.h>
#     include <windows.h>
#else
#     include <cstdlib>
#endif

#if (!(defined(DISABLE_RPC_CAF)) && !(DISABLE_RPC_CAF))
#     include "covers.hpp"
#endif

#define EXPORT __attribute__((visibility("default")))

#define PLUGIN_NAME "Discord RPC"
#define PLUGIN_ID "discord-rpc"
#define PLUGIN_URL "https://github.com/onegentig/audacious-discord-rpc"
#define DISCORD_APP_ID "1428914566795890738"

static bool is_connected = false;  // A guard, just in case
static std::atomic<unsigned long long> req_id_last{0};
static std::atomic<bool> cover_fetch_running{false};
inline bool cover_fetch_stop(unsigned long long req_id) {
     return req_id != req_id_last.load();
}

/* === Discord Functions === */

void init_discord();
void clear_discord();
void cleanup_discord();
void update_presence();
void init_presence();

void playback_to_presence();  // Audacious metadata -> Discord RPC (main)
void cover_to_presence(
    const std::string &artist,
    const std::string &album);  // Attempts to fetch cover if enabled

void on_playback_update_rpc(void *, void *) { playback_to_presence(); }

/* === Utilities === */

std::string field_sanitise(const std::string &field) {
     if (field.empty()) {
          return "[unknown]";
     } else if (field.length() > 128) {
          return field.substr(0, 125) + "...";
     } else if (field.length() < 2) {
          return field + " ";
     }
     return field;
}

bool audstr_empty(const String &str) {
     const char *str_c = str;
     return (!str_c || !*str_c || strlen(str_c) == 0);
}

String field_sanitise(const String &field) {
     if (audstr_empty(field)) {
          return String("[unknown]");
     }

     const char *field_c = field;
     auto field_len = strlen(field_c);
     if (field_len > 128) {
          StringBuf buf(128 + 1);
          memcpy(buf, field_c, 125);
          memcpy(buf + 125, "...", 4);
          return String(buf.settle());
     } else if (field_len < 2) {
          StringBuf buf(field_len + 2);
          memcpy(buf, field_c, field_len);
          buf[field_len] = ' ';
          buf[field_len + 1] = '\0';
          return String(buf.settle());
     }

     return field;
}

void open_github() {
#ifdef _WIN32
     auto ret
         = ShellExecuteW(NULL, L"open", L"" PLUGIN_URL, NULL, NULL, SW_NORMAL);
     if ((intptr_t)ret <= 32) AUDERR("Failed to open URL: %s\r\n", PLUGIN_URL);
          /** @cite
           * https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew#return-value
           */
#else
     const char *cmd = "xdg-open " PLUGIN_URL;
     int ret = system(cmd);
     if (ret == -1 || ret == 127)
          AUDERR("Failed to open URL: %s\r\n", PLUGIN_URL);
          /* -1 is fork/exec error, 127 is command not found.
           * Other error codes from the command itself have to
           * do with the user’s environment and are not worth
           * blocking over. */
#endif
}
