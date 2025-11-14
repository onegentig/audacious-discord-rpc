/**
 * @file cover-cache.hpp
 * @brief Cache for album cover arts for Audacious Discord RPC (experimental)
 * @author Нурлан Кърамызов <onegen@onegen.dev>
 * @date 2025-11-14 (last modified)
 *
 * @note Custom solution for minimalism and not having to tackle with deps.
 *       Uses LRU eviction policy + no admission policy.
 *
 * @license MIT
 * @copyright Copyright (c) 2025 onegen
 *
 */

#pragma once
#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <deque>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#ifndef AUDDBG
#     define AUDDBG(...) ((void)0)
#endif
#ifndef AUDINFO
#     define AUDINFO(...) ((void)0)
#endif
#ifndef AUDERR
#     define AUDERR(...) ((void)0)
#endif

constexpr std::size_t TIMESTAMP_SIZE
    = sizeof(std::chrono::steady_clock::time_point);

class CoverArtCache {
   public:
     using clk = std::chrono::steady_clock;

     struct CacheOptions {
          std::size_t max_items = 128;  // Capacity of n items (0 = unlimited)
          std::size_t max_bytes
              = 4 * 1024 * 1024;  // Capacity of n bytes (0 = unlimited)
          std::chrono::seconds ttl{
              0};  // Entry TTL in seconds (0 = keep forever)
     };

     CoverArtCache(std::size_t max_items, std::size_t max_bytes,
                   std::chrono::seconds ttl)
         : opts{max_items, max_bytes, ttl} {}

     static std::string key(const std::string& artist,
                            const std::string& album) {
          std::string k;
          k.reserve(artist.size() + 1 + album.size());
          k.append(artist);
          k.push_back('\x1F');  // Unit separator – arbitrary but unlikely
          k.append(album);
          return k;
     }

     std::optional<std::string> get(const std::string& artist,
                                    const std::string& album) {
          std::string k = key(artist, album);
          auto map_it = cachemap.find(k);
          if (map_it == cachemap.end()) return std::nullopt;

          if (opts.ttl.count()
              && (clk::now() - map_it->second.timestamp) > opts.ttl) {
               drop(k, map_it);
               return std::nullopt;
          }

          auto list_it = std::find(uselist.begin(), uselist.end(), k);
          if (list_it != uselist.end()) {
               uselist.erase(list_it);
               uselist.push_front(k);
          }

          return map_it->second.val;
     }

     void put(const std::string& artist, const std::string& album,
              const std::string& val) {
          std::string k = key(artist, album);
          if ((2 * k.size()) + val.size() + TIMESTAMP_SIZE > opts.max_bytes) {
               AUDERR("RPC CACache: Entry too big!\r\n");
               return;
          }

          auto map_it = cachemap.find(k);
          if (map_it != cachemap.end()) {
               // Key exists => update val & timestamp + move to front
               bytes_used -= map_it->second.val.size();  // - old val size
               map_it->second.val = val;
               map_it->second.timestamp = clk::now();
               bytes_used += val.size();  // + new val size

               auto list_it = std::find(uselist.begin(), uselist.end(), k);
               if (list_it != uselist.end()) uselist.erase(list_it);

               uselist.push_front(k);
               AUDDBG("RPC CACache: Updated URL of an entry\r\n");
          } else {
               // New key => insert
               cachemap.emplace(k, CacheEntry{val, clk::now()});
               uselist.push_front(k);
               bytes_used
                   += k.size() + TIMESTAMP_SIZE;  // + key & timestamp sizes
               bytes_used += val.size();          // + new val size
               AUDDBG("RPC CACache: Added new entry %s (%zu items; %zu B)\r\n",
                      k.c_str(), uselist.size(), bytes_used);
          }

          enforce();
     }

     void clear() {
          cachemap.clear();
          uselist.clear();
          bytes_used = 0;
          AUDINFO("RPC CACache: Cleared (0 items; 0 B)\r\n");
     }

   private:
     struct CacheEntry {
          std::string val;  //< Value (image URL)
          clk::time_point
              timestamp;  //< Timestamp of insertion or update (for TTL)
     };

     void drop(const std::string& k,
               std::unordered_map<std::string, CacheEntry>::iterator it) {
          bytes_used -= it->second.val.size() + k.size() + TIMESTAMP_SIZE;
          uselist.remove(k);
          cachemap.erase(it);
          AUDDBG("RPC CACache: Dropped LRU entry %s (%zu items; %zu B)\r\n",
                 k.c_str(), uselist.size(), bytes_used);
     }

     bool is_overflowing() const {
          if (uselist.empty()) return false;
          if ((opts.max_items != 0) && (uselist.size() > opts.max_items))
               return true;
          if ((opts.max_bytes != 0) && (bytes_used > opts.max_bytes))
               return true;
          return false;
     }

     void enforce() {
          if (cachemap.size() != uselist.size())
               AUDERR(
                   "RPC CACache: Cachemap size (%zu) and uselist size (%zu) "
                   "are not equal!\r\n",
                   cachemap.size(),
                   uselist.size());  // Just a sanity check, should NEVER happen
          AUDDBG("RPC CACache: enforce() on B=%zu Bmax=%zu N=%zu Nmax=%zu\r\n",
                 bytes_used, opts.max_bytes, uselist.size(), opts.max_items);

          while (this->is_overflowing()) {
               // Evict the least frequently used item (LRU)
               const std::string& k_lru = uselist.back();
               auto it = cachemap.find(k_lru);
               if (it != cachemap.end()) drop(k_lru, it);
          }
     }

     CacheOptions opts;  //< Cache settings, like capacity and TTL.
     std::unordered_map<std::string, CacheEntry>
         cachemap;                    // TODO: remove when LRU impl.
     std::list<std::string> uselist;  //< List of keys ordered by use recency.
     std::size_t bytes_used = 0;      //< Size of cache in bytes.
};