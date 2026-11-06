/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief Counters on the CULL hash tables, descriptors and packing (CS-2993)
 *
 * Each thread counts into a table of its own, without locking. When the thread
 * ends, its table is merged under a mutex into the table of its thread class
 * (the thread name without its trailing number, so that all worker threads
 * add up). Counts a thread makes after that, from the destructors of other
 * thread local data, go to the merged table directly. The exit handler merges
 * the calling thread and writes everything out.
 *
 * Threads still running at exit are not merged; the daemons join their
 * threads before they exit.
 */

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "uti/sge_component.h"

#include "cull/cull_list.h"
#include "cull/cull_state.h"
#include "cull/cull_stats.h"
#include "cull/ocs_cull_descr.h"

namespace {
   using ocs::cull_stats::Counter;
   using ocs::cull_stats::NUM_COUNTERS;

   /** @brief Names of the counters in the dump, in the order of #Counter */
   constexpr std::array<const char *, NUM_COUNTERS> counter_names = {
      "pos_calls", "pos_reduced", "pos_iters",
      "dcopy_elem", "dcopy_elem_b", "dcopy_other", "dcopy_other_b",
      "dcopy_unpack", "dcopy_unpack_b",
      "hash_create", "hash_bucket_b", "hash_insert", "hash_lookup", "hash_free_used", "hash_free_unused",
      "pack_descr", "pack_descr_b", "pack_elems", "pack_value_b",
      "unpack_lists", "unpack_elems", "unpack_ns", "unpack_hash_ns",
      "hash_build_lazy", "lookup_linear", "lookup_len_lt4", "lookup_len_lt8", "lookup_len_lt16",
      "lookup_len_lt32", "lookup_len_lt64", "lookup_len_lt128", "lookup_len_ge128",
      "descr_intern_hit", "descr_intern_new", "descr_intern_new_b", "descr_private", "descr_private_b",
      "descr_pack_type_ref", "descr_pack_table_def", "descr_pack_table_ref"
   };

   using Counters = std::array<uint64_t, NUM_COUNTERS>;

   /** @brief Counters by key: the field number, with #TYPE_BIT set for a type counter */
   using Table = std::unordered_map<uint64_t, Counters>;

   constexpr uint64_t TYPE_BIT = uint64_t{1} << 32; ///< marks the key of a type counter

   constexpr int DUMP_FORMAT = 2; ///< version of the dump layout, raised when a reader has to tell layouts apart; 2: shared descriptors (CS-2997)

   /** @brief The counters of one thread class, as they are dumped */
   struct ThreadDump {
      std::map<std::string, Counters> types;                            ///< by type
      std::map<std::string, std::map<std::string, Counters>> fields;    ///< by type, then field
   };

   /**
    * @brief Build the table key of a counter
    * @param type true for a type counter
    * @param nm the field number
    * @return the key
    */
   constexpr uint64_t make_key(const bool type, const int nm) {
      return (type ? TYPE_BIT : 0) | static_cast<uint32_t>(nm);
   }

   std::mutex merged_mutex; ///< guards #merged_tables

   /**
    * @brief The merged tables, by thread class
    *
    * Allocated once and never freed, so that the exit handler and late counts
    * from thread local destructors never meet a destroyed object.
    *
    * @return the merged tables
    */
   std::map<std::string, Table> &merged_tables() {
      static auto *tables = new std::map<std::string, Table>;
      return *tables;
   }

   thread_local Table *own_table = nullptr;  ///< the calling thread's table, created on first use
   thread_local bool own_retired = false;    ///< the calling thread's table was merged already
   thread_local char own_class[64] = "";     ///< the calling thread's class, set when merged

   /**
    * @brief Merge a table into another one
    * @param to the table to add to
    * @param from the table to add
    */
   void merge_into(Table &to, const Table &from) {
      for (const auto &[key, counters] : from) {
         Counters &target = to[key];
         for (int c = 0; c < NUM_COUNTERS; c++) {
            target[c] += counters[c];
         }
      }
   }

   /**
    * @brief Merge the calling thread's table into the table of its class
    *
    * Afterwards the thread counts into the merged table directly.
    */
   void retire_own_table() {
      if (own_retired) {
         return;
      }

      const char *name = component_get_thread_name();
      if (name == nullptr || *name == '\0') {
         name = "unknown";
      }
      size_t len = strnlen(name, sizeof(own_class) - 1);
      memcpy(own_class, name, len);
      while (len > 0 && own_class[len - 1] >= '0' && own_class[len - 1] <= '9') {
         len--;
      }
      own_class[len] = '\0';

      std::lock_guard guard(merged_mutex);
      if (own_table != nullptr) {
         merge_into(merged_tables()[own_class], *own_table);
         delete own_table;
         own_table = nullptr;
      }
      own_retired = true;
   }

   /** @brief Merges the thread's table when the thread ends */
   struct Retirer {
      bool armed = false; ///< the thread has a table to merge

      ~Retirer() {
         if (armed) {
            retire_own_table();
         }
      }
   };

   thread_local Retirer retirer; ///< constructed with the calling thread's table

   /** @brief Write the counters at exit, if `OCS_CULL_STATS_DIR` is set */
   void exit_handler() {
      const char *dir = getenv("OCS_CULL_STATS_DIR");
      if (dir != nullptr && *dir != '\0') {
         ocs::cull_stats::write(dir);
      }
   }

   /**
    * @brief Add to a counter of the calling thread
    * @param key the table key
    * @param c the counter
    * @param v the amount to add
    */
   void add(const uint64_t key, const Counter c, const uint64_t v) {
      if (own_retired) {
         std::lock_guard guard(merged_mutex);
         merged_tables()[own_class][key][c] += v;
         return;
      }
      if (own_table == nullptr) {
         static std::once_flag registered;
         std::call_once(registered, [] { atexit(exit_handler); });
         own_table = new Table;
         retirer.armed = true;
      }
      (*own_table)[key][c] += v;
   }

   /**
    * @brief The name space block a field number belongs to
    * @param nm the field number
    * @return the block, nullptr if there is no name space or no block holds it
    */
   const lNameSpace *find_block(const int nm) {
      const lNameSpace *ns = cull_state_get_name_space();
      for (const lNameSpace *block = ns; block != nullptr && block->lower != 0; block++) {
         if (nm >= block->lower && nm < block->lower + block->size) {
            return block;
         }
      }
      return nullptr;
   }

   /**
    * @brief Name the object type a field belongs to
    *
    * The name is the prefix of the type's first field plus "_Type", e.g.
    * `JB_Type` for `JB_job_number`.
    *
    * @param nm the field number
    * @return the type name, or the number when the name space does not know it
    */
   std::string type_name(const int nm) {
      if (nm == NoName) {
         return "NoName";
      }
      const lNameSpace *block = find_block(nm);
      if (block == nullptr) {
         return std::to_string(nm);
      }
      const std::string first = block->namev[0];
      return first.substr(0, first.find('_')) + "_Type";
   }

   /**
    * @brief Name a field
    * @param nm the field number
    * @return the field name, or the number when the name space does not know it
    */
   std::string field_name(const int nm) {
      const lNameSpace *block = find_block(nm);
      return block == nullptr ? std::to_string(nm) : block->namev[nm - block->lower];
   }

   /**
    * @brief The name of the running program
    * @return the program name
    */
   const char *program_name() {
#if defined(LINUX)
      return program_invocation_short_name;
#elif defined(SOLARIS)
      const char *path = getexecname();
      const char *slash = path != nullptr ? strrchr(path, '/') : nullptr;
      return slash != nullptr ? slash + 1 : (path != nullptr ? path : "unknown");
#else
      return getprogname();
#endif
   }

   /**
    * @brief Open a new dump file, never overwriting one
    * @param dir the directory
    * @param[out] path receives the path of the opened file
    * @return the open file, nullptr on failure
    */
   FILE *open_dump_file(const char *dir, std::string &path) {
      const std::string base = std::string(dir) + "/" + program_name() + "." + std::to_string(getpid());
      for (int n = 0; n < 100; n++) {
         path = n == 0 ? base + ".json" : base + "." + std::to_string(n) + ".json";
         const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
         if (fd >= 0) {
            return fdopen(fd, "w");
         }
         if (errno != EEXIST) {
            break;
         }
      }
      path.clear();
      return nullptr;
   }

   /**
    * @brief Add one counter array to another
    * @param to the array to add to
    * @param from the array to add
    */
   void add_counters(Counters &to, const Counters &from) {
      for (int c = 0; c < NUM_COUNTERS; c++) {
         to[c] += from[c];
      }
   }

   /**
    * @brief Whether a counter array holds anything
    * @param counters the array
    * @return true if one counter is not 0
    */
   bool any_counted(const Counters &counters) {
      for (const uint64_t value : counters) {
         if (value != 0) {
            return true;
         }
      }
      return false;
   }

   /**
    * @brief Write the counters that are not 0 as a JSON object
    * @param writer the JSON writer
    * @param counters the counters
    */
   void write_counters(rapidjson::Writer<rapidjson::StringBuffer> &writer, const Counters &counters) {
      writer.StartObject();
      for (int c = 0; c < NUM_COUNTERS; c++) {
         if (counters[c] != 0) {
            writer.Key(counter_names[c]);
            writer.Uint64(counters[c]);
         }
      }
      writer.EndObject();
   }

   /**
    * @brief Format the current time as an ISO 8601 UTC timestamp
    * @return the timestamp, e.g. 2026-11-02T14:18:05Z
    */
   std::string utc_timestamp() {
      const time_t now = time(nullptr);
      tm utc{};
      gmtime_r(&now, &utc);
      char buffer[32];
      strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
      return buffer;
   }
}

namespace ocs::cull_stats {
   void add_type(const int nm, const Counter c, const uint64_t v) {
      add(make_key(true, nm), c, v);
   }

   void add_field(const int nm, const Counter c, const uint64_t v) {
      add(make_key(false, nm), c, v);
   }

   uint64_t get(const bool type, const int nm, const Counter c) {
      if (own_table == nullptr) {
         return 0;
      }
      const auto it = own_table->find(make_key(type, nm));
      return it == own_table->end() ? 0 : it->second[c];
   }

   uint64_t now_ns() {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count();
   }

   std::string write(const char *dir) {
      retire_own_table();

      // type counters of a reduced descriptor are keyed by its first field and
      // add up to their type here
      std::map<std::string, ThreadDump> dump;
      {
         std::lock_guard guard(merged_mutex);
         for (const auto &[thread_class, table] : merged_tables()) {
            ThreadDump &thread = dump[thread_class];
            for (const auto &[key, counters] : table) {
               const int nm = static_cast<int>(static_cast<uint32_t>(key));
               if ((key & TYPE_BIT) != 0) {
                  add_counters(thread.types[type_name(nm)], counters);
               } else {
                  add_counters(thread.fields[type_name(nm)][field_name(nm)], counters);
               }
            }
         }
      }

      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      bool counted = false;
      writer.StartObject();
      writer.Key("format");
      writer.Int(DUMP_FORMAT);
      writer.Key("program");
      writer.String(program_name());
      writer.Key("pid");
      writer.Int(static_cast<int>(getpid()));
      writer.Key("written");
      writer.String(utc_timestamp().c_str());
      writer.Key("descr_registry_peak");
      writer.Uint64(ocs_cull_descr_registry_peak());
      writer.Key("threads");
      writer.StartObject();
      for (const auto &[thread_class, thread] : dump) {
         writer.Key(thread_class.c_str());
         writer.StartObject();
         writer.Key("types");
         writer.StartObject();
         for (const auto &[type, counters] : thread.types) {
            if (any_counted(counters)) {
               writer.Key(type.c_str());
               write_counters(writer, counters);
               counted = true;
            }
         }
         writer.EndObject();
         writer.Key("fields");
         writer.StartObject();
         for (const auto &[type, fields] : thread.fields) {
            writer.Key(type.c_str());
            writer.StartObject();
            for (const auto &[field, counters] : fields) {
               if (any_counted(counters)) {
                  writer.Key(field.c_str());
                  write_counters(writer, counters);
                  counted = true;
               }
            }
            writer.EndObject();
         }
         writer.EndObject();
         writer.EndObject();
      }
      writer.EndObject();
      writer.EndObject();
      if (!counted) {
         return {};
      }

      std::string path;
      FILE *f = open_dump_file(dir, path);
      if (f == nullptr) {
         return {};
      }
      const bool written = fwrite(buffer.GetString(), 1, buffer.GetSize(), f) == buffer.GetSize() &&
                           fputc('\n', f) != EOF;
      if (fclose(f) != 0 || !written) {
         return {};
      }
      return path;
   }
}
