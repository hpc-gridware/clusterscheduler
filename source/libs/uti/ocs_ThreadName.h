#pragma once
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
 * @brief The canonical names of the daemon's threads
 */

#include <array>
#include <string_view>
#include <optional>
#include <cstddef>

/// The canonical thread names, as an X-macro list: `X(enumerator, name)`
#define THREAD_NAME_LIST(X)                \
   X(MAIN_THREAD,         "main")          \
   X(LISTENER_THREAD,     "listener")      \
   X(EVENT_MASTER_THREAD, "event-master")  \
   X(TIMER_THREAD,        "timer")         \
   X(WORKER_THREAD,       "worker")        \
   X(SIGNAL_THREAD,       "signal")        \
   X(SCHEDD_THREAD,       "scheduler")     \
   X(EVENT_MIRROR_THREAD, "mirror")        \
   X(READER_THREAD,       "reader")

/// One enumerator per entry of #THREAD_NAME_LIST, in that order
enum ThreadName {
/// @cond   transient X-macro expansion, undefined again below
#define X(name, str) name,
/// @endcond
   THREAD_NAME_LIST(X)
#undef X
   THREAD_TYPE_COUNT ///< number of thread names, and the size of #threadnames
};

/// The name of each #ThreadName, indexed by the enumerator
constexpr std::array<std::string_view, THREAD_TYPE_COUNT> threadnames = {
/// @cond   transient X-macro expansion, undefined again below
#define X(name, str) str,
/// @endcond
   THREAD_NAME_LIST(X)
#undef X
};

/**
 * @brief The name of a thread kind
 *
 * @param t the thread kind
 * @return its name, or an empty view when @p t is out of range
 */
constexpr std::string_view to_string_view(const ThreadName t) {
   const auto idx = static_cast<std::size_t>(t);
   return idx < threadnames.size() ? threadnames[idx] : std::string_view{};
}

/**
 * @brief The name of a thread kind, as a C string
 *
 * @param t the thread kind
 * @return its name; the pointer refers to static storage, do not free it
 */
constexpr const char *to_cstr(const ThreadName t) {
   return to_string_view(t).data();
}

/**
 * @brief The name of a thread kind, as a `std::string`
 *
 * @param t the thread kind
 * @return its name, copied into a new string
 */
constexpr std::string to_string(const ThreadName t) {
   return std::string(to_string_view(t));
}

/**
 * @brief Look a thread kind up by name
 *
 * @param s the name to look for, matched exactly
 * @return the matching kind, or an empty optional when the name is unknown
 */
constexpr std::optional<ThreadName> from_string_to_ThreadName(const std::string_view s) {
   for (std::size_t i = 0; i < threadnames.size(); ++i) {
      if (threadnames[i] == s) {
         return static_cast<ThreadName>(i);
      }
   }
   return std::nullopt;
}
