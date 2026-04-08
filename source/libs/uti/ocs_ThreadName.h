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

#include <array>
#include <string_view>
#include <optional>
#include <cstddef>

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

enum ThreadName {
#define X(name, str) name,
   THREAD_NAME_LIST(X)
#undef X
   THREAD_TYPE_COUNT
};

constexpr std::array<std::string_view, THREAD_TYPE_COUNT> threadnames = {
#define X(name, str) str,
   THREAD_NAME_LIST(X)
#undef X
};

constexpr std::string_view to_string_view(const ThreadName t) {
   const auto idx = static_cast<std::size_t>(t);
   return idx < threadnames.size() ? threadnames[idx] : std::string_view{};
}

constexpr const char *to_cstr(const ThreadName t) {
   return to_string_view(t).data();
}

constexpr std::string to_string(const ThreadName t) {
   return std::string(to_string_view(t));
}

constexpr std::optional<ThreadName> from_string_to_ThreadName(const std::string_view s) {
   for (std::size_t i = 0; i < threadnames.size(); ++i) {
      if (threadnames[i] == s) {
         return static_cast<ThreadName>(i);
      }
   }
   return std::nullopt;
}
