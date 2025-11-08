/**
 * @file audacious-discord-rpc.cpp
 * @brief Discord Rich Presence plugin for Audacious
 * @version 2.2
 * @author onegen <onegen@onegen.dev>
 * @author Derzsi Dániel <daniel@tohka.us>
 * @date 2025-11-05 (last modified)
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
      "https://github.com/onegentig/audacious-discord-rpc\n"
      " © onegen <onegen@onegen.dev> (2024–2025)\n"
      " © Derzsi Dániel <daniel@tohka.us> et al. (2018–2022)\n\n"
      "Displays the current playing track as your Discord status.\n"
      "(Discord should be running before this plugin is loaded.)";

const PreferencesWidget RPCPlugin::widgets[]
    = {WidgetCheck(N_("(UNSTABLE) Fetch album covers from MusicBrainz/CAA"),
                   WidgetBool(PLUGIN_ID, "fetch_covers")),
       WidgetCheck(N_("Hide presence when paused"),
                   WidgetBool(PLUGIN_ID, "hide_when_paused")),
       WidgetButton(N_("Show on GitHub"), {open_github, nullptr})};

const char *const RPCPlugin::defaults[]
    = {"fetch_covers", "FALSE", "hide_when_paused", "FALSE", nullptr};

const PluginPreferences RPCPlugin::prefs
    = {{widgets}, nullptr, nullptr, nullptr};

EXPORT RPCPlugin aud_plugin_instance;

/* === Discord RPC Setup === */

static discord::RPCManager &rpc = discord::RPCManager::get();
static discord::Presence presence;

void init_discord() {
     rpc.setClientID(DISCORD_APP_ID).initialize();
     rpc.onReady([](const discord::User &) {
             is_connected = true;
             AUDINFO("Discord RPC Connected.\r\n");
        })
         .onDisconnected([](int, std::string_view) {
              is_connected = false;
              AUDINFO("Discord RPC Disconnected.\r\n");
         })
         .onErrored([](int, std::string_view msg) {
              std::string err = "Discord RPC Error: ";
              err += msg;
              AUDERR("%s\r\n", err.c_str());
         });
}

void clear_discord() {
     if (!is_connected) return;
     rpc.clearPresence();
}

void cleanup_discord() {
     if (!is_connected) return;
     rpc.clearPresence();
     rpc.shutdown();
}

void update_presence() {
     if (!is_connected) return;
     rpc.clearPresence()
         .refresh();  // TODO: Timestamps are very unreliably cleared.
                      // This helps a little, but it’s still very wonky.
     rpc.setPresence(presence).refresh();
}

void init_presence() {
     presence = discord::Presence{};
     presence.setLargeImageKey("logo").setLargeImageText("Audacious");
     update_presence();
}

/* === Audacious playback -> Discord RPC (main function) === */

void playback_to_presence() {
     if (!aud_drct_get_playing() || !aud_drct_get_ready()) {
          clear_discord();
          return;
     }

     const bool playing = !aud_drct_get_paused();
     if (aud_get_bool(PLUGIN_ID, "hide_when_paused") && !playing) {
          clear_discord();
          return;
     }

     const Tuple tuple = aud_drct_get_tuple();
     std::string title(tuple.get_str(Tuple::Title));
     std::string artist(tuple.get_str(Tuple::Artist));
     std::string album(tuple.get_str(Tuple::Album));
     title = field_sanitise(title);
     artist = field_sanitise(artist);
     album = album.empty() ? "" : field_sanitise(album);

     presence.setLargeImageKey("logo")
         .setActivityType(discord::ActivityType::Listening)
         .setStatusDisplayType(discord::StatusDisplayType::Name)
         .setDetails(title.c_str())
         .setState(artist.c_str())
         .setLargeImageText(album.c_str())
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

     if (aud_get_bool(PLUGIN_ID, "fetch_covers")) {
          std::string album_artist(tuple.get_str(Tuple::AlbumArtist));
          AUDINFO("RPC main: starting cover art fetch (CAF) thread...\r\n");
          cover_to_presence(album_artist.empty() ? artist : album_artist,
                            album);
     }
}

/* == Attempt to fetch cover art, if enabled */

void cover_to_presence(const std::string &artist, const std::string &album) {
     const auto req_id = ++req_id_last;
     cover_fetch_running.store(true);
     std::thread([artist, album, req_id] {
          auto thread_req_id = req_id;
          if (thread_req_id != req_id_last.load()) return;
          if (auto u
              = cover_lookup(artist, album, &req_id_last, thread_req_id)) {
               if (thread_req_id == req_id_last.load()) {
                    presence.setLargeImageKey(*u);
                    update_presence();
               }
          }
     }).detach();
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
