#include <chrono>
#include <iostream>
#include <stdint.h>
#include <string.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/tuple.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include <discord_rpc.h>

#define EXPORT __attribute__((visibility("default")))
#define APPLICATION_ID "484736379171897344"

static const char *SETTING_EXTRA_TEXT = "extra_text";

class RPCPlugin : public GeneralPlugin {

public:
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Discord RPC"),
        "audacious-plugin-rpc",
        about,
        &prefs
    };

    constexpr RPCPlugin() : GeneralPlugin (info, false) {}

    bool init();
    void cleanup();
};

EXPORT RPCPlugin aud_plugin_instance;

DiscordEventHandlers handlers;
DiscordRichPresence presence;
std::string fullTitle;
std::string playingStatus;

void init_discord() {
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);
}

void update_presence() {
    Discord_UpdatePresence(&presence);
}

void init_presence() {
    memset(&presence, 0, sizeof(presence));
    presence.largeImageKey = "logo";
    update_presence();
}

void clear_discord() {
    Discord_ClearPresence();
}

void cleanup_discord() {
    Discord_ClearPresence();
    Discord_Shutdown();
}

void title_changed() {
    if (!aud_drct_get_playing() || !aud_drct_get_ready()) {
        clear_discord();
        return;
    }

    const bool paused = aud_drct_get_paused();
    const Tuple tuple = aud_drct_get_tuple();

    std::string title(tuple.get_str(Tuple::Title));
    std::string artist(tuple.get_str(Tuple::Artist));
    if (title.empty()) title = "[unknown]";
    if (artist.empty()) artist = "[unknown]";
    artist = "by " + artist;

    if (!paused) {
        int64_t start = aud_drct_get_time() / 1000;
        const auto clock = std::chrono::system_clock::now();
        const int64_t now_s = std::chrono::duration_cast<std::chrono::seconds>(clock.time_since_epoch()).count();
        const int64_t elapsed = now_s - start;
        presence.startTimestamp = elapsed;
    } else {
        presence.startTimestamp = 0;
    }

    presence.details = title.c_str();
    presence.state = artist.c_str();
    presence.smallImageKey = paused ? "pause" : "play";
    update_presence();
}

void update_title_presence(void*, void*) {
    title_changed();
}

void open_github() {
   system("xdg-open https://github.com/darktohka/audacious-plugin-rpc");
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

const char RPCPlugin::about[] = N_("Discord RPC music status plugin\n\nWritten by: Derzsi Daniel <daniel@tohka.us>");

const PreferencesWidget RPCPlugin::widgets[] =
{
  WidgetEntry(
      N_("Extra status text:"),
      WidgetString("audacious-plugin-rpc", SETTING_EXTRA_TEXT, title_changed)
  ),
  WidgetButton(
      N_("Fork on GitHub"),
      {open_github}
  )
};

const PluginPreferences RPCPlugin::prefs = {{ widgets }};
