#pragma once

/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include <csignal>
#include <cstdint>
#include <string>

namespace ocs {
   class TerminationManager {
   private:
      static void
      signal_handler(int sig, siginfo_t* info, void* context);

      static void
      terminate_due_to_exception_handler();

      static std::string
      get_exception_description();
   public:
      static bool
      install_signal_handler();

      static bool
      install_terminate_handler();

      static bool
      allow_core_dumps();

      static std::string
      get_stacktrace(bool demangle_names);

      static void
      show_stacktrace(uint32_t level);

      static void
      trigger_segfault();

      static void
      trigger_exception();

      static void
      trigger_abort();

      static void
      trigger_stack_overflow(int iterations = 65535);
   };
}
