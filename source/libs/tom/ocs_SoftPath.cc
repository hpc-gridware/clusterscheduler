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

#include "tom/ocs_SoftPath.h"

#ifdef ENABLE_DEBUG_CHECKS

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cull/cull_name.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_arch.h"
#include "uti/sge_component.h"

namespace ocs::tom {

   namespace {

      /** @brief Lines written per process for one accessor, field and reason at most */
      constexpr int MaxLinesPerKey = 8;

      /** @brief What is known per accessor, field and reason in this process */
      struct KeyState {
         unsigned long hits{0};        ///< how often the soft path was taken
         std::set<std::size_t> stacks; ///< hashes of the stacks already written
      };

      std::once_flag sink_once;  ///< the sink directory is looked up once per process
      std::string sink_dir;      ///< the sink directory, empty if inactive
      std::mutex state_mutex;    ///< guards #key_states
      std::map<std::tuple<std::string, int, SoftPathReason>, KeyState> key_states; ///< per-key state

      /**
       * @brief Look up the sink directory: $OCS_SOFT_PATH_DIR, else soft_paths in the installation root
       *
       * Leaves #sink_dir empty if the directory does not exist, so that a debug
       * build writes nothing unless the sink was prepared on purpose.
       */
      void find_sink() {
         std::string dir;
         if (const char *env = getenv("OCS_SOFT_PATH_DIR"); env != nullptr && *env != '\0') {
            dir = env;
         } else {
            char buffer[PATH_MAX];
            if (const char *root = sge_get_root_dir(0, buffer, sizeof(buffer), 0); root != nullptr) {
               dir = std::string(root) + "/soft_paths";
            }
         }
         struct stat st{};
         if (!dir.empty() && stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            sink_dir = dir;
         }
      }

      /**
       * @brief The component and thread that took the soft path, e.g. "qmaster/worker-1"
       *
       * The names come from the component of the calling thread
       * (component_get_component_name(), component_get_thread_name()). A
       * thread that has not set up its component, as in tools and module
       * tests, reports "unknown"; then the program name of the process is
       * used, and the thread name is left out.
       *
       * @return the component, a slash and the thread, or the program name
       */
      std::string component_label() {
         const std::string component = component_get_component_name();
         if (component != "unknown") {
            const std::string thread = component_get_thread_name();
            return thread != "unknown" ? component + "/" + thread : component;
         }
#if defined(LINUX)
         return program_invocation_short_name;
#elif defined(FREEBSD)
         return getprogname();
#else
         return component;
#endif
      }

      /**
       * @brief The current stack on one line, function and offset per frame
       *
       * Leaves out every frame up to and including report_soft_path(), found by
       * name because inlining changes how many frames lie above it, so the first
       * frame is the old accessor or its helper. A frame with a function name
       * keeps what follows the binary's path; a frame without one, a static
       * function or one in an anonymous namespace, keeps the binary's name and
       * the offset, e.g. "sge_qmaster+0x3a1ebf", which `addr2line -e` resolves.
       *
       * @return the frames separated by " | "
       */
      std::string folded_stack() {
         const std::string stack = ocs::TerminationManager::get_stacktrace(true, 1); // without get_stacktrace
         std::string out;
         bool past_report = false;
         std::size_t start = 0;
         while (start < stack.size()) {
            std::size_t end = stack.find('\n', start);
            if (end == std::string::npos) {
               end = stack.size();
            }
            std::string line = stack.substr(start, end - start);
            start = end + 1;
            if (!past_report) {
               past_report = line.find("report_soft_path(") != std::string::npos;
               continue;
            }
            if (const std::size_t colon = line.find(" : "); colon != std::string::npos) {
               std::string frame = line.substr(colon + 3);
               if (frame.starts_with(" ")) {
                  // no function name: binary name and offset instead
                  const std::string path = line.substr(0, colon);
                  const std::size_t slash = path.rfind('/');
                  const std::size_t plus = frame.find('+');
                  const std::size_t arrow = frame.find(" =>");
                  frame = (slash == std::string::npos ? path : path.substr(slash + 1)) +
                          (plus != std::string::npos ? frame.substr(plus, arrow - plus) : frame);
               }
               line = frame;
            }
            if (!out.empty()) {
               out += " | ";
            }
            out += line;
         }
         return out;
      }

      /**
       * @brief The name of a reason as written to the sink
       * @param reason the reason
       * @return its name
       */
      const char *reason_name(const SoftPathReason reason) {
         switch (reason) {
            case SoftPathReason::NullElement:
               return "null-element";
            case SoftPathReason::MissingField:
               return "missing-field";
            case SoftPathReason::ForeignField:
               return "foreign-field";
            case SoftPathReason::NegativePosition:
               return "negative-position";
            case SoftPathReason::NullArgument:
               return "null-argument";
            case SoftPathReason::BoundElement:
               return "bound-element";
         }
         return "unknown";
      }

      /**
       * @brief Append one line to the sink file of this host
       *
       * The file is created with mode 0666, so that daemons running as root and
       * clients running as a user write to the same file. One write() per line.
       *
       * @param line the line, ending with a newline
       */
      void append_line(const std::string &line) {
         char host[256] = "unknown";
         gethostname(host, sizeof(host) - 1);
         const std::string path = sink_dir + "/" + host + ".log";
         const int fd = open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
         if (fd < 0) {
            return;
         }
         fchmod(fd, 0666);
         [[maybe_unused]] const ssize_t written = write(fd, line.data(), line.size());
         close(fd);
      }

   } // namespace

   void report_soft_path(const char *function, const int name, const SoftPathReason reason) noexcept {
      try {
         std::call_once(sink_once, find_sink);
         if (sink_dir.empty()) {
            return;
         }
         const int saved_errno = errno;
         std::unique_lock lock(state_mutex);
         KeyState &state = key_states[{function, name, reason}];
         const unsigned long hit = ++state.hits;
         // a stack is taken only for the 1st, 2nd, 4th, 8th, ... hit, and not once the key is full
         if (state.stacks.size() >= MaxLinesPerKey || (hit & (hit - 1)) != 0) {
            errno = saved_errno;
            return;
         }
         lock.unlock();
         const std::string stack = folded_stack();
         lock.lock();
         // another thread may have filled the key while the stack was taken
         if (state.stacks.size() >= MaxLinesPerKey || !state.stacks.insert(std::hash<std::string>{}(stack)).second) {
            errno = saved_errno;
            return;
         }
         lock.unlock();

         char when[32];
         const time_t now = time(nullptr);
         struct tm tm_now{};
         localtime_r(&now, &tm_now);
         strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%S", &tm_now);

         std::string line = when;
         line += " | ";
         line += component_label();
         line += " | ";
         line += std::to_string(getpid());
         line += " | ";
         line += function;
         line += " | ";
         line += name == SoftPathNoName ? "-" : lNm2Str(name);
         line += " | ";
         line += reason_name(reason);
         line += " | hit ";
         line += std::to_string(hit);
         line += " | ";
         line += stack;
         line += "\n";
         append_line(line);
         errno = saved_errno;
      } catch (...) {
         // recording must never change the behaviour of the accessor
      }
   }

} // namespace ocs::tom

#endif
