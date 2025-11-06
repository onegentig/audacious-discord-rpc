/**
 * @file cover-cache.hpp
 * @brief Cache for album cover arts for Audacious Discord RPC (experimental)
 * @author onegen <onegen@onegen.dev>
 * @date 2025-11-05 (last modified)
 *
 * @note Currently implemtended with FIFO eviction policy.
 *       I’d like to later swap it for LRU.
 *
 * @license MIT
 * @copyright Copyright (c) 2025 onegen
 *
 */

#pragma once
#include <chrono>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>

class CoverArtCache {
   public:
     using clk = std::chrono::steady_clock;

     struct CacheOptions {
          std::size_t max_items = 128;
          std::size_t max_bytes = 4 * 1024 * 1024;  // 4 MiB
          std::chrono::seconds ttl{0};
     };

     explicit CoverArtCache(const CacheOptions& options) : opts(options) {}

     static std::string key(const std::string& artist,
                            const std::string& album) {
          std::string k;
          k.reserve(artist.size() + 1 + album.size());
          k.append(artist);
          k.push_back('\x1F');  // unit separator – arbitrary but unlikely
          k.append(album);
          return k;
     }

     std::optional<std::string> get(const std::string& artist,
                                    const std::string& album) {
          std::string k = key(artist, album);
          auto it = map.find(k);
          if (it == map.end()) return std::nullopt;

          if (opts.ttl.count()
              && (clk::now() - it->second.timestamp) > opts.ttl) {
               drop(k, it);
               return std::nullopt;
          }

          return it->second.val;
     }

     void put(const std::string& artist, const std::string& album,
              const std::string& val) {
          std::string k = key(artist, album);
          auto it = map.find(k);
          if (it != map.end()) {
               filled_bytes -= it->second.val.size();
               it->second.val = val;
               it->second.timestamp = clk::now();
          } else {
               map.emplace(k, Entry{val, clk::now()});
               order.push_back(k);
          }
          filled_bytes += val.size();
          enforce();
     }

     void clear() {
          map.clear();
          order.clear();
          filled_bytes = 0;
     }

   private:
     struct Entry {
          std::string val;
          clk::time_point timestamp;
     };

     void drop(const std::string& key,
               std::unordered_map<std::string, Entry>::iterator it) {
          filled_bytes -= it->second.val.size();
          map.erase(it);
          order.erase(std::find(order.begin(), order.end(), key));
     }

     void enforce() {
          while (!order.empty()
                 && (map.size() > opts.max_items
                     || (opts.max_bytes && filled_bytes > opts.max_bytes))) {
               const std::string& oldest = order.front();
               auto it = map.find(oldest);
               if (it != map.end()) drop(oldest, it);
               order.pop_front();
          }
     }

     CacheOptions opts;
     std::unordered_map<std::string, Entry> map;
     std::deque<std::string> order;
     std::size_t filled_bytes = 0;
};