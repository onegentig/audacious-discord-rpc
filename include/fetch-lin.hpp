/**
 * @file fetch-lin.hpp
 * @brief cURL-based HTTP fetcher for use on Linux.
 * @note Made for Audacious-Discord-RPC project.
 * @author onegen <onegen@onegen.dev>
 * @date 2025-11-24 (last modified)
 *
 * @license MIT
 * @copyright Copyright (c) 2025 onegen
 *
 */

#pragma once

#include <curl/curl.h>

#include <optional>
#include <string>

#ifndef AUDDBG
#     define AUDDBG(...) ((void)0)
#endif
#ifndef AUDINFO
#     define AUDINFO(...) ((void)0)
#endif
#ifndef AUDERR
#     define AUDERR(...) ((void)0)
#endif

constexpr unsigned long FETCH_TIMEO = 15000;  // [ms]

/* === Helpers === */

/** @brief Write callback for cURL */
static size_t write_cb(void* c, size_t s, size_t n, void* u) {
     ((std::string*)u)->append((char*)c, s * n);
     return s * n;
}

/* === Exported Function === */

/** @brief User-Agent */
static const char* ua
    = "Audacious Discord RPC/2.2 "
      "(+https://github.com/onegen-dev/audacious-discord-rpc)";

static std::optional<std::string> fetch(const std::string& url) noexcept {
     AUDDBG("RPC CAF: fetching %s\r\n", url.c_str());
     CURL* c = curl_easy_init();
     if (!c) return std::nullopt;
     std::string buf;
     curl_easy_setopt(c, CURLOPT_URL, url.c_str());
     curl_easy_setopt(c, CURLOPT_USERAGENT, ua);
     curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
     curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
     curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, FETCH_TIMEO);
     curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, FETCH_TIMEO);
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

static std::optional<std::string> uri_encode(const std::string& str) noexcept {
     CURL* c = curl_easy_init();
     if (!c) return std::nullopt;
     auto enc = std::string(curl_easy_escape(c, str.c_str(), str.size()));
     curl_easy_cleanup(c);
     return enc;
}