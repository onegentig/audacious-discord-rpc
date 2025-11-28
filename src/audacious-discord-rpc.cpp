/**
 * @file audacious-discord-rpc.cpp
 * @brief Discord Rich Presence plugin for Audacious
 * @version 2.2
 * @author onegen <onegen@onegen.dev>
 * @author Derzsi Dániel <daniel@tohka.us>
 * @date 2025-11-28 (last modified)
 *
 * @license MIT
 * @copyright Copyright (c) 2024–2025 onegen
 *                          2018–2022 Derzsi Dániel
 *
 */

#include "audacious-discord-rpc.hpp"

/* === Audacious Plugin Setup === */

class RPCPlugin : public GeneralPlugin {
   public:
     static const char about[];
     static const PreferencesWidget widgets[];
     static const PluginPreferences prefs;
     static const char *const defaults[];

     static constexpr PluginInfo info
         = {N_(PLUGIN_NAME), PLUGIN_ID, about, &prefs, 0};

     constexpr RPCPlugin() : GeneralPlugin(info, false) {}

     bool init();
     void cleanup();
};

const char RPCPlugin::about[]
    = "Discord Rich Presence (RPC) playing status plugin\n"
      "https://github.com/onegen-dev/audacious-discord-rpc\n"
      " © onegen <onegen@onegen.dev> (2024–2025)\n"
      " © Derzsi Dániel <daniel@tohka.us> et al. (2018–2022)\n\n"
      "Displays the current playing track as your Discord status.\n"
      "(Discord should be running before this plugin is loaded.)";

static const ComboItem status_display_items[]
    = {ComboItem(N_("Music player"),
                 static_cast<int>(discord::StatusDisplayType::Name)),
       ComboItem(N_("Song title"),
                 static_cast<int>(discord::StatusDisplayType::Details)),
       ComboItem(N_("Artist name"),
                 static_cast<int>(discord::StatusDisplayType::State))};

const PreferencesWidget RPCPlugin::widgets[] = {
#if (!(defined(DISABLE_RPC_CAF)) && !(DISABLE_RPC_CAF))
    WidgetCheck(N_("(UNSTABLE) Fetch album covers from MusicBrainz/CAA"),
                WidgetBool(PLUGIN_ID, "fetch_covers")),
#endif
    WidgetCheck(N_("Hide presence when paused"),
                WidgetBool(PLUGIN_ID, "hide_when_paused")),
    WidgetCombo(N_("Status display"),
                WidgetInt(PLUGIN_ID, "status_display_type"),
                {{status_display_items}, nullptr}),
    WidgetButton(N_("Show on GitHub"), {open_github, nullptr})};

const char *const RPCPlugin::defaults[] = {
#if (!(defined(DISABLE_RPC_CAF)) && !(DISABLE_RPC_CAF))
    "fetch_covers",
    "FALSE",
#endif
    "hide_when_paused",
    "FALSE",
    "status_display_type",
    int_to_str(static_cast<int>(discord::StatusDisplayType::Name)),
    nullptr};

const PluginPreferences RPCPlugin::prefs
    = {{widgets}, nullptr, nullptr, nullptr};

EXPORT RPCPlugin aud_plugin_instance;

/* === Discord RPC Setup === */

static discord::RPCManager &rpc = discord::RPCManager::get();
static discord::Presence presence;

void init_discord() {
     rpc.setClientID(DISCORD_APP_ID).initialize();
     rpc.onReady([](const discord::User &) {
             is_connected.store(true);
             AUDINFO("Discord RPC Connected.\r\n");
        })
         .onDisconnected([](int, std::string_view) {
              is_connected.store(false);
              AUDINFO("Discord RPC Disconnected.\r\n");
         })
         .onErrored([](int, std::string_view msg) {
              AUDERR("Discord RPC Error: %s\r\n", msg.data());
         });
}

void clear_discord() {
     if (!is_connected.load()) return;
     presence = discord::Presence{};  // Full reset
     presence.setLargeImageKey("logo").setLargeImageText("Audacious");
     rpc.clearPresence();
}

void cleanup_discord() {
     if (!is_connected.load()) return;
     rpc.clearPresence();
     rpc.shutdown();
}

void update_presence() {
     if (!is_connected.load()) return;
     rpc.setPresence(presence).refresh();
}

void init_presence() {
     presence = discord::Presence{};
     presence.setLargeImageKey("logo").setLargeImageText("Audacious");
     update_presence();
}

/* === Audacious playback -> Discord RPC (main function) === */

void playback_to_presence() {
     if (!is_connected.load()) return;
     if (!aud_drct_get_playing() || !aud_drct_get_ready()) {
          clear_discord();
          return;
     }

     const bool playing = !aud_drct_get_paused();
     if (aud_get_bool(PLUGIN_ID, "hide_when_paused") && !playing) {
          clear_discord();
          return;
     }

     AUDDBG("RPC main: updating presence...\r\n");
     const Tuple tuple = aud_drct_get_tuple();
     String title = tuple.get_str(Tuple::Title);
     String artist = tuple.get_str(Tuple::Artist);
     String album = tuple.get_str(Tuple::Album);
     if (!title) {
          // Fallback to filename
          title = tuple.get_str(Tuple::Basename);
          if (!title) {
               // Give up
               AUDINFO(
                   "RPC main: no title or filename found, clearing "
                   "presence.\r\n");
               clear_discord();
               return;
          }
     }

     title = field_sanitise(title);
     artist = field_sanitise(artist);
     bool has_album = !!album;
     album = has_album ? field_sanitise(album) : album;

     int status_display_type = aud_get_int(PLUGIN_ID, "status_display_type");

     presence.setLargeImageKey("logo")
         .setActivityType(discord::ActivityType::Listening)
         .setStatusDisplayType(
             static_cast<discord::StatusDisplayType>(status_display_type))
         .setDetails((const char *)title)
         .setState((const char *)artist)
         .setLargeImageText(has_album ? (const char *)album : "")
         .setSmallImageKey(playing ? "play" : "pause")
         .setSmallImageText("Audacious");

     if (playing) {
          const auto clock = std::chrono::system_clock::now();
          const int64_t now_ts
              = std::chrono::duration_cast<std::chrono::seconds>(
                    clock.time_since_epoch())
                    .count();
          int64_t start_ts = now_ts - (aud_drct_get_time() / 1000);
          presence.setStartTimestamp(start_ts);
          if (tuple.get_value_type(Tuple::Length) == Tuple::Int
              && tuple.get_int(Tuple::Length) > 0)
               presence.setEndTimestamp(
                   start_ts + (tuple.get_int(Tuple::Length) / 1000));
     } else {
          presence.setStartTimestamp(0).setEndTimestamp(0);
     }

     AUDINFO("RPC main: updated presence!\r\n");
     update_presence();

     if (has_album && aud_get_bool(PLUGIN_ID, "fetch_covers")) {
          String album_artist = tuple.get_str(Tuple::AlbumArtist);
          bool has_album_artist = !audstr_empty(album_artist);

          // Cover fetching still works on std::strings
          AUDINFO("RPC main: starting cover art fetch (CAF) thread...\r\n");
          cover_to_presence(has_album_artist ? album_artist : artist, album);
     }
}

/* == Attempt to fetch cover art, if enabled */

void cover_to_presence(const String &artist, const String &album) {
#if (defined(DISABLE_RPC_CAF) && DISABLE_RPC_CAF)
     return;
#else
     unsigned long long req_id = ++req_id_now;
     std::thread([req_id, artist, album] {
          if (req_id != req_id_now.load(std::memory_order_relaxed)) return;
          auto url = cover_lookup((const char *)artist, (const char *)album,
                                  &req_id_now, req_id);
          if (url && !url->empty()
              && req_id == req_id_now.load(std::memory_order_relaxed)) {
               presence.setLargeImageKey(*url);
               update_presence();
               AUDINFO("RPC CAF: Applied cover (req %llu)\r\n", req_id);
          } else {
               AUDINFO("RPC CAF: Aborted cover fetch (stale req %llu)\r\n",
                       req_id);
          }
     }).detach();
#endif
}

/* === Hook RPC to Audacious === */

bool RPCPlugin::init() {
     aud_config_set_defaults(PLUGIN_ID, defaults);
     init_discord();
     init_presence();
     hook_associate("playback ready", on_playback_update_rpc, nullptr);
     hook_associate("playback end", on_playback_update_rpc, nullptr);
     hook_associate("playback stop", on_playback_update_rpc, nullptr);
     hook_associate("playback pause", on_playback_update_rpc, nullptr);
     hook_associate("playback unpause", on_playback_update_rpc, nullptr);
     hook_associate("title change", on_playback_update_rpc, nullptr);
     return true;
}

void RPCPlugin::cleanup() {
     hook_dissociate("playback ready", on_playback_update_rpc);
     hook_dissociate("playback end", on_playback_update_rpc);
     hook_dissociate("playback stop", on_playback_update_rpc);
     hook_dissociate("playback pause", on_playback_update_rpc);
     hook_dissociate("playback unpause", on_playback_update_rpc);
     hook_dissociate("title change", on_playback_update_rpc);
     cleanup_discord();
}
