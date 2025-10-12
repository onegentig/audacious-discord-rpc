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
#define APPLICATION_ID "484736379171897344"

static bool is_connected = false; // Guard, just in case
static bool hide_when_paused = false;

class RPCPlugin : public GeneralPlugin {
   public:
     static const char about[];
     static const PreferencesWidget widgets[];
     static const PluginPreferences prefs;

     static constexpr PluginInfo info
         = {N_("Discord RPC"), "audacious-plugin-rpc", about, &prefs};

     constexpr RPCPlugin() : GeneralPlugin(info, false) {}

     bool init();
     void cleanup();
};

EXPORT RPCPlugin aud_plugin_instance;

static discord::RPCManager &rpc = discord::RPCManager::get();
static discord::Presence presence;

void init_discord() {
     rpc.setClientID(APPLICATION_ID).initialize();
     rpc.onReady([](const discord::User&) {
          is_connected = true;
     })
     .onDisconnected([](int, std::string_view) {
          is_connected = false;
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
     rpc
          .clearPresence()
          .refresh(); // TODO: Timestamps are very unreliably cleared.
                      // This helps a little, but it’s still very wonky.
     rpc
          .setPresence(presence)
          .refresh();
}

void init_presence() {
     presence = discord::Presence{};
     presence
          .setLargeImageKey("logo")
          .setLargeImageText("Audacious");
     update_presence();
}

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

void title_changed() {
     if (!aud_drct_get_playing() || !aud_drct_get_ready()) {
          clear_discord();
          return;
     }

     const bool playing = !aud_drct_get_paused();
     if (hide_when_paused && !playing) {
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

     presence
          .setLargeImageKey("logo")
          .setActivityType(discord::ActivityType::Listening)
          .setStatusDisplayType(discord::StatusDisplayType::Name)
          .setDetails(title.c_str())
          .setState(artist.c_str())
          .setLargeImageText(album.c_str())
          .setSmallImageKey(playing ? "play" : "pause");

     if (playing) {
          const auto clock = std::chrono::system_clock::now();
          const int64_t now_ts = std::chrono::duration_cast<std::chrono::seconds>(clock.time_since_epoch()).count();
          int64_t start_ts = now_ts - (aud_drct_get_time() / 1000);
          presence.setStartTimestamp(start_ts);
          if (tuple.get_value_type(Tuple::Length) == Tuple::Int && tuple.get_int(Tuple::Length) > 0)
               presence.setEndTimestamp(start_ts + (tuple.get_int(Tuple::Length) / 1000));
     } else {
          presence.setStartTimestamp(0).setEndTimestamp(0);
     }

     update_presence();
}

void update_title_presence(void *, void *) { title_changed(); }

void open_github() {
     system("xdg-open https://github.com/onegentig/audacious-discord-rpc");
}

bool RPCPlugin::init() {
     init_discord();
     init_presence();
     hook_associate("playback ready", update_title_presence, nullptr);
     hook_associate("playback end", update_title_presence, nullptr);
     hook_associate("playback stop", update_title_presence, nullptr);
     hook_associate("playback pause", update_title_presence, nullptr);
     hook_associate("playback unpause", update_title_presence, nullptr);
     hook_associate("title change", update_title_presence, nullptr);
     return true;
}

void RPCPlugin::cleanup() {
     hook_dissociate("playback ready", update_title_presence);
     hook_dissociate("playback end", update_title_presence);
     hook_dissociate("playback stop", update_title_presence);
     hook_dissociate("playback pause", update_title_presence);
     hook_dissociate("playback unpause", update_title_presence);
     hook_dissociate("title change", update_title_presence);
     cleanup_discord();
}

const char RPCPlugin::about[] = N_(
    "Discord Rich Presence (RPC) playing status plugin\n"
    "https://github.com/onegentig/audacious-discord-rpc\n"
    " © onegen <onegen@onegen.dev> (2024–2025)\n"
    " © Derzsi Dániel <daniel@tohka.us> et al. (2018–2022)\n\n"
    "Displays the current playing track as your Discord status.\n"
    "(Discord should be running before this plugin is loaded.)"
);

const PreferencesWidget RPCPlugin::widgets[] = {
     WidgetCheck(N_("Hide presence when paused"), WidgetBool(hide_when_paused, title_changed)),
     WidgetButton(N_("Show on GitHub"), {open_github})
};

const PluginPreferences RPCPlugin::prefs = {{widgets}};
