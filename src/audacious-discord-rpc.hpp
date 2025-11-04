/**
 * @file audacious-discord-rpc.hpp
 * @brief Discord Rich Presence plugin for Audacious (header)
 * @version 2.1
 * @author onegen <onegen@onegen.dev>
 * @author Derzsi Dániel <daniel@tohka.us>
 * @date 2025-10-17 (last modified)
 *
 * @license MIT
 * @copyright Copyright (c) 2024–2025 onegen
 *                          2018–2022 Derzsi Dániel
 *
 */

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
#include <string.h>

#include <chrono>
#include <iostream>

#define EXPORT __attribute__((visibility("default")))

#define PLUGIN_NAME "Discord RPC"
#define PLUGIN_ID "discord-rpc"
#define PLUGIN_URL "https://github.com/onegentig/audacious-discord-rpc"
#define DISCORD_APP_ID "1428914566795890738"

static bool is_connected = false;  // A guard, just in case

/* === Discord Functions === */

void init_discord();
void clear_discord();
void cleanup_discord();
void update_presence();
void init_presence();

void playback_to_presence();
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

void open_github() { system("xdg-open " PLUGIN_URL); }
