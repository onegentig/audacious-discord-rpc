/**
 * @file cover-fetcher.hpp
 * @brief Fetching logic for album cover arts for Audacious Discord RPC
 * (experimental)
 * @author onegen <onegen@onegen.dev>
 * @date 2025-11-05 (last modified)
 *
 * @license MIT
 * @copyright Copyright (c) 2025 onegen
 *
 */

#pragma once
#define JSON_NOEXCEPTION

#include <curl/curl.h>
#include <libaudcore/runtime.h>  // for logging
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>

#include "cover-cache.hpp"

using json = nlohmann::json;

/* === Cache === */

static CoverArtCache cache({.max_items = 128,
                            .max_bytes = 4 * 1024 * 1024,
                            .ttl = std::chrono::seconds(3600)});

/* === HTTP Fetcher === */

static const char* ua
    = "Audacious Discord RPC/2.2 "
      "(+https://github.com/onegentig/audacious-discord-rpc)";

static size_t write_cb(void* c, size_t s, size_t n, void* u) {
     ((std::string*)u)->append((char*)c, s * n);
     return s * n;
}

static std::optional<std::string> fetch(const std::string& url) noexcept {
     AUDDBG("RPC CAF: fetching %s\r\n", url.c_str());
     CURL* c = curl_easy_init();
     if (!c) return std::nullopt;
     std::string buf;
     curl_easy_setopt(c, CURLOPT_URL, url.c_str());
     curl_easy_setopt(c, CURLOPT_USERAGENT, ua);
     curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
     curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
     curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 5000L);
     curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
     curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
     curl_easy_setopt(c, CURLOPT_NOPROGRESS, 1L);
     curl_easy_setopt(c, CURLOPT_FAILONERROR, 0L);
     curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
     curl_easy_setopt(c, CURLOPT_TCP_FASTOPEN, 1L);
     curl_easy_setopt(c, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
     curl_easy_setopt(c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
     CURLcode r = curl_easy_perform(c);
     curl_easy_cleanup(c);
     if (r != CURLE_OK) {
          AUDERR("RPC CAF: cURL err %d\r\n", r);
          return std::nullopt;
     }

     return buf;
}

/* === Helpers === */

inline std::string esc_quotes(const std::string& s) {
     std::string r;
     r.reserve(s.size());
     for (char c : s)
          if (c == '"' || c == '\\')
               r += '\\', r += c;
          else
               r += c;
     return r;
}

inline bool is_cancelled(const std::atomic<unsigned long long>* active_req_id,
                         unsigned long long this_req_id) {
     return active_req_id && (this_req_id != active_req_id->load());
}

/* === Exported Function === */

std::optional<std::string> cover_lookup(
    const std::string& artist, const std::string& album,
    const std::atomic<unsigned long long>* active_req_id = nullptr,
    unsigned long long this_req_id = 0) {
     // Cache
     auto cache_res = cache.get(artist, album);
     if (cache_res.has_value()) {
          AUDINFO("RPC CAF: Cache hit!\r\n");
          return cache_res;
     } else {
          AUDINFO("RPC CAF: Cache miss.\r\n");
     }

     // 2 second debounce (in case user is mashing NEXT
     // (MB is heavily rate-limited)
     for (int i = 0; i < 20; ++i) {
          // 20 × 100 ms = 2000 ms = 2 s
          if (is_cancelled(active_req_id, this_req_id)) return std::nullopt;
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
     }

     // MB (prepare query)
     CURL* c = curl_easy_init();
     if (!c) {
          AUDERR("RPC CAF: cURL init for esc failed!\r\n");
          return std::nullopt;
     }

     /* The query disregards the track artist, focusing on the album artist a la
      * LastFM. Album title is prioritised over album alias. Artist name is also
      * checked for label (for compilations, like Monstercat, which MB often
      * tags as "Various Artists") but most users use "Monstercat". Artist match
      * is slightly prioritised over label. All formats are accepted (for e.g.
      * CD rips) but digital media has slight priority. Only matches with score
      * >= 90 are considered + only front cover is used. If no match / error /
      * no front => nullopt (Audacious logo shown instead).
      */

     auto esc_album = esc_quotes(album);
     auto esc_artist = esc_quotes(artist);
     std::string q = "(\"" + esc_album + "\"^2 OR alias:\"" + esc_album
                     + "\")^3 AND (artistname:\"" + esc_artist
                     + "\"^2 OR label:\"" + esc_artist
                     + "\") AND (format:\"Digital Media\"^2 OR format:*)"
                     + " AND NOT status:\"Pseudo-Release\"";
     std::string req = "https://musicbrainz.org/ws/2/release?query="
                       + std::string(curl_easy_escape(c, q.c_str(), 0))
                       + "&fmt=json";
     curl_easy_cleanup(c);

     // MB (get release MBID)
     if (is_cancelled(active_req_id, this_req_id)) return std::nullopt;
     auto mb_json = fetch(req);
     if (!mb_json) {
          AUDERR("RPC CAF: MB error!\r\n");
          return std::nullopt;
     }

     auto mb = json::parse(*mb_json, nullptr, false);
     if (mb.is_discarded() || !mb.is_object() || mb["count"] == 0
         || mb["releases"].empty()) {
          AUDERR("RPC CAF: MB no releases found!\r\n");
          return std::nullopt;
     }
     auto release = mb["releases"][0];
     if (release["score"] < 90) return std::nullopt;
     std::string mbid = release["id"];
     AUDINFO("RPC CAF: found MBID %s \r\n", mbid.c_str());

     // CAA (fetch all artwork)
     if (is_cancelled(active_req_id, this_req_id)) return std::nullopt;
     auto caa_json = fetch("https://coverartarchive.org/release/" + mbid);
     if (!caa_json) {
          AUDERR("RPC CAF: CAA error.\r\n");
          return std::nullopt;
     }

     // CAA (parse and find front cover)
     auto caa = json::parse(*caa_json, nullptr, false);
     if (caa.is_discarded() || !caa.is_object() || caa["images"].empty())
          return std::nullopt;
     AUDINFO("RPC CAF: CAA found %d images\r\n", caa["images"].size());
     for (auto& image : caa["images"]) {
          if (image.contains("front") && image["front"] == true
              && image.contains("thumbnails")
              && image["thumbnails"].contains("large")) {
               std::string image_url = image["thumbnails"]["large"];
               cache.put(artist, album, image_url);
               AUDINFO("RPC CAF: returning found img!\r\n");
               return image_url;
          }
     }

     // Nada
     AUDERR("RPC CAF: no front image!\r\n");
     return std::nullopt;
}