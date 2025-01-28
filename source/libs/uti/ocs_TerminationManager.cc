/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <algorithm>
#include <csignal>
#include <exception>
#include <iostream>
#include <ranges>
#include <string>
#include <sstream>
#include <vector>
#include <sys/resource.h>
#include <unistd.h>

#if defined(LINUX) || defined(FREEBSD)
#   include <execinfo.h>
#   include <cxxabi.h>
#endif

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "ocs_TerminationManager.h"

/** @brief Get a stacktrace of the current thread.
 *
 * This method might fail if no additional heap memory can be allocated. In this case,
 * an empty string is returned. If it succeeds, the stacktrace is returned as a string:
 *
 * @example
 *    ./gcs-extensions/test/libs/sgeobj/test_Stacktrace : ocs::get_stacktrace[abi:cxx11](bool)  +0x50 => 0x41b629
 *    ./gcs-extensions/test/libs/sgeobj/test_Stacktrace : Y::test_function_a(int)  +0x1d => 0x41b243
 *    ./gcs-extensions/test/libs/sgeobj/test_Stacktrace : X::TestClassA::method(int)  +0x19 => 0x41b353
 *    ./gcs-extensions/test/libs/sgeobj/test_Stacktrace : X::TestClassB::method(int)  +0x20 => 0x41b3d0
 *    ./gcs-extensions/test/libs/sgeobj/test_Stacktrace : main  +0x25 => 0x41b2c1
 *    /lib64/libc.so.6 : __libc_start_main  +0xf3 => 0x7f4f16763493
 *    ./gcs-extensions/test/libs/sgeobj/test_Stacktrace : _start  +0x2e => 0x41b16e
 *
 * If `demangle_names` is set to *true*, function names are demangled (like in the output above).
 * If demangling fails or is turned off then the raw output is shown which might mean that namespace
 * and parameter information might be missing (especially for C++ functions and class methods).
 *
 * +0xXX is the offset from the beginning of the function. The last column is the address of the
 * location where the next function in the callstack was called or backtrace was taken. The first
 * line always shows the location where ocs::get_stacktrace() calls backtrace() to get the stacktrace.
 *
 * If the binary was compiled with debug information then the address information can be used with
 * the `addr2line` tool to get the exact line number in the source code where the function was called
 * or `gdb` can be used to get the same information.
 *
 * @example
 *    > addr2line -e ./gcs-extensions/test/libs/sgeobj/test_Stacktrace -f -C 0x41b353
 *    /home/ebablick/CS/cs2-0/gcs-extensions/test/libs/sgeobj/test_Stacktrace.cc:27
 *
 * @example
 *    > gdb ./gcs-extensions/test/libs/sgeobj/test_Stacktrace
 *    (gdb) list *(0x41b353)
 *    0x41b353 is in X::TestClassA::method(int) (/home/ebablick/CS/cs2-0/gcs-extensions/test/libs/sgeobj/test_Stacktrace.cc:27).
 *
 * @note
 *    The stacktrace is only available on Linux systems. On other systems, the function returns an empty string.
 *
 * @todo
 *    CS-967: Add support for BSD where backtrace is also available
 *    CS-966: Add support for Solaris where backtrace is also available
 *
 * @param demangle_names If true, demangle function names.
 * @return A string containing the stacktrace.
 */
std::string
ocs::TerminationManager::get_stacktrace(bool demangle_names) {
   std::stringstream ss;

#if defined(LINUX) || defined(FREEBSD)
   constexpr int bt_max_size = 1024;
   void *bt_addresses[bt_max_size];

   // store stacktrace addresses in array
   const int bt_size = backtrace(bt_addresses, bt_max_size);

   // translate addresses into strings of the form filename(function+address)
   char **bt_symbols = backtrace_symbols(bt_addresses, bt_size);
   if (bt_symbols == nullptr) {
      return "";
   }

   // raw output or demangled function names?
   if (demangle_names) {
      size_t fnc_size = 256;
      auto fnc_name = static_cast<char *>(sge_malloc(fnc_size)); // IMPORTANT: use sge_malloc() instead of new. __cxa_demangle() might realloc() the buffer
      if (fnc_name == nullptr) {
         sge_free(&bt_symbols);
         return "";
      }

      // handle all function names
      for (int i = 0; i < bt_size; i++) {
         char *begin_name = nullptr;
         char *begin_offset = nullptr;
         char *end_offset = nullptr;
         char *begin_addr = nullptr;
         char *end_addr = nullptr;

         // find parentheses and +address offset surrounding the mangled name: ./file(function+0xXXXX) [0xXXXXXXXX]
         // 0x8217e7e2c <_fini+0x8213a7c30> at /lib/libthr.so.3
         for (char *p = bt_symbols[i]; *p; ++p) {
            if (*p == '(') {
               begin_name = p;
            } else if (*p == '+') {
               begin_offset = p;
            } else if (*p == ')' && begin_offset) {
               end_offset = p;
            } else if (*p == '[') {
               begin_addr = p;
            } else if (*p == ']' && begin_addr) {
               end_addr = p;
               break;
            }
         }

         if (begin_name != nullptr &&
             begin_offset != nullptr && end_offset != nullptr && begin_name < begin_offset &&
             begin_addr != nullptr && end_addr != nullptr && begin_name < end_addr) {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';
            *begin_addr++ = '\0';
            *end_addr = '\0';

            // demangle the function name
            int status;
            char* ret = abi::__cxa_demangle(begin_name, fnc_name, &fnc_size, &status);
            if (status == 0) {
               // ret might be realloc()-ed buffer
               fnc_name = ret;
               ss << bt_symbols[i] << " : " << fnc_name << " " << " +" << begin_offset << " => " << begin_addr;
            } else {
               // un-demangled name is better that nothing
               ss << bt_symbols[i] << " : " << begin_name << " " << " +" << begin_offset << " => " << begin_addr;
            }
         } else {
            // couldn't parse the line? print the original line.
            ss << bt_symbols[i];
         }
         ss << std::endl;
      }

      // free the function name buffer
      sge_free(&fnc_name);
   } else {
      // unmodified stacktrace
      for (int i = 0; i < bt_size; i++) {
         ss << bt_symbols[i] << std::endl;
      }
   }

   // free the bt buffer
   sge_free(&bt_symbols);
#endif

   return ss.str();
}

/** @brief Allow core dumps to be generated.
 *
 * This method removes the core file size limit and enables core dumps in the kernel.
 * Should be called as `root` in order to succeed.
 *
 * @todo
 *    CS-964: Add support for at least LINUX and BSD
 *
 * @return True if core dumps are allowed, false otherwise.
 */
bool
ocs::TerminationManager::allow_core_dumps() {
#if 0
   // Remove core file size limit
   struct rlimit limit;
   limit.rlim_cur = RLIM_INFINITY;
   limit.rlim_max = RLIM_INFINITY;
   int lret = setrlimit(RLIMIT_CORE, &limit);
   if (lret == -1) {
      return false;
   }

   // Ensure core dumps are enabled in the kernel
   lret = system("echo 1 > /proc/sys/kernel/core_uses_pid");
   if (lret == -1) {
      return false;
   }
#endif
   return true;
}

/** @brief Install signal handlers for signals that should trigger a stacktrace.
 *
 * This method installs signal handlers for the following signals:
 * - SIGABRT (Abort)
 * - SIGSEGV (Segmentation fault)
 * - SIGFPE (Floating point exception)
 * - SIGILL (Illegal instruction)
 * - SIGBUS (Bus error)
 */
bool
ocs::TerminationManager::install_signal_handler() {
   // Synchronous signals that should be handled by our signal handler
   const std::vector<int> HANDLED_SIGNALS = {
      SIGABRT,    // Abort
      SIGSEGV,    // Segmentation fault
      SIGFPE,     // Floating point exception
      SIGILL,     // Illegal instruction
      SIGBUS,     // Bus error

      // SIGTRAP, Trap should not trigger a dump of the stacktrace
      // explicitly no SIGINT and SIGTERM here because they are not synchronous and will be handled by the signal thread
   };

   // Install signal handler for each signal
   struct sigaction sa{};
   sa.sa_sigaction = signal_handler;
   sa.sa_flags = SA_SIGINFO;

   // Return false if any signal handler could not be installed
   for (const int signal : HANDLED_SIGNALS) {
      if (sigaction(signal, &sa, nullptr) != 0) {
         return false;
      }
   }

   // Unblock signal so that each thread can catch synchronous signals
   sigset_t sig_set;
   sigemptyset(&sig_set);
   for (const int signal : HANDLED_SIGNALS) {
      sigaddset(&sig_set, signal);
   }
   if (pthread_sigmask(SIG_UNBLOCK, &sig_set, nullptr) != 0) {
      return false;
   }

   return true;
}

/** @brief Install terminate handler for uncaught exceptions.
 *
 * This method installs a terminate handler that is called when an uncaught exception is thrown.
 */
bool
ocs::TerminationManager::install_terminate_handler() {
   std::set_terminate(terminate_due_to_exception_handler);
   return true;
}

/** @brief Signal handler for SIGABRT, SIGSEGV, SIGFPE, and SIGILL.
 *
 * This method is called when a signal is received that should trigger a stacktrace.
 * It prints the signal information, the signal origin address, and the stacktrace
 * before re-raising the signal which will then lead to an abort which will generate
 * a core dump if core dumps are enabled.
 *
 * Please note that the exception information in the output example below is written
 * by a termination handler whereas the stacktrace is written by the signal handler.
 * Both information is also written to the message file of the component that
 * executes the code but in between both output sections other messages might be
 * written to the message file by those components/threads tat are still avtive before
 * abort is called.
 *
 * The output for an uncaught exception looks like this. The exception information is
 * printed by the termination handler.
 *
 * @example
 *    terminate called after throwing an instance of 'std::runtime_error'
 *    what():  This is a test exception that was triggered in ocs::TerminationManager::trigger_exception()
 *
 *    *** SIGNAL 6 (SIGABRT: Abort) received ***
 *    Signal originated at address: 0x7d3000ed145
 *    Stacktrace:
 *    ./sge_qmaster.all : ocs::TerminationManager::get_stacktrace[abi:cxx11](bool)  +0x206 => 0x62f796
 *    ./sge_qmaster.all : ocs::TerminationManager::signal_handler(int, siginfo_t*, void*)  +0x3bc => 0x6302ec
 *    /lib64/libpthread.so.0 :   +0x12c20 => 0x7f0e4a82ec20
 *    /lib64/libc.so.6 : gsignal  +0x10f => 0x7f0e4972837f
 *    /lib64/libc.so.6 : abort  +0x127 => 0x7f0e49712db5
 *    /lib64/libstdc++.so.6 :   +0x9009b => 0x7f0e4a0e009b
 *    /lib64/libstdc++.so.6 :   +0x9653c => 0x7f0e4a0e653c
 *    /lib64/libstdc++.so.6 :   +0x96597 => 0x7f0e4a0e6597
 *    /lib64/libstdc++.so.6 :   +0x967f8 => 0x7f0e4a0e67f8
 *    ./sge_qmaster.all : ocs::TerminationManager::trigger_exception()  +0x36 => 0x44ba40
 *    ./sge_qmaster.all : sge_c_gdi_process_in_listener(ocs::gdi::Packet*, ocs::gdi::Task*, _lList**, monitoring_t*, bool)  +0x139d => 0x48402d
 *    ./sge_qmaster.all : sge_qmaster_process_message(monitoring_t*)  +0x115d => 0x4c30bd
 *    ./sge_qmaster.all : sge_listener_main(void*)  +0x19d => 0x4e6bad
 *    /lib64/libpthread.so.0 :   +0x817a => 0x7f0e4a82417a
 *    /lib64/libc.so.6 : clone  +0x43 => 0x7f0e497eddc3
 *
 * @param sig Signal number
 * @param info Signal information
 * @param context Signal context
 */
void
ocs::TerminationManager::signal_handler(int sig, siginfo_t* info, void* context) {
   DENTER(TOP_LAYER);
   std::stringstream ss;

   // Prevent recursive signals
   static bool handling_signal = false;
   if (handling_signal) {
      std::_Exit(EXIT_FAILURE);
   }
   handling_signal = true;

   // Print signal information
   ss << std::endl << "*** SIGNAL " << sig << " ";
   switch (sig) {
      case SIGABRT: ss << "(SIGABRT: Abort)"; break;
      case SIGSEGV: ss << "(SIGSEGV: Segmentation fault)"; break;
      case SIGFPE:  ss << "(SIGFPE: Floating point exception)"; break;
      case SIGILL:  ss << "(SIGILL: Illegal instruction)"; break;
      case SIGBUS:  ss << "(SIGBUS: Bus error)"; break;
      default:      ss << "(Unknown signal)" << sig; break;
   }
   ss << " received ***" << std::endl;

   // Add signal-specific information
   if (info) {
      ss << "Signal originated at address: " << info->si_addr << std::endl;
   }

   // Print stacktrace
   ss << "Stacktrace:" << std::endl;
   ss << get_stacktrace(true) << std::endl;

   // Try to write into the log file
   std::string line;
   while (std::getline(ss, line)) {
      sge_log(LOG_CRIT, line.c_str(), __FILE__, __LINE__); \
   }

   // Flush all output
   std::cerr.flush();
   std::cout.flush();

   // Sync file systems
   sync();

   // Restore default handler and re-raise signal
   signal(sig, SIG_DFL);
   raise(sig);
   // DRETURN_VOID;
}

/** @brief Get a description of the current exception.
 *
 * This method returns a string containing the description of the current exception.
 * If no exception is active, the string "No active exception" is returned.
 *
 * @return A string containing the description of the current exception.
 */
std::string
ocs::TerminationManager::get_exception_description() {
   try {
      if (auto exc = std::current_exception()) {
         try {
            std::rethrow_exception(exc);  // rethrow the exception to get the type
         } catch (const std::exception& e) {
            return std::string("std::exception: ") + e.what();
         } catch (const std::string& e) {
            return "string exception: " + e;
         } catch (...) {
            return "Unknown exception type";
         }
      }
   } catch (...) {
      return "Error while getting exception details";
   }
   return "No active exception";
}

/** @brief Terminate due to an uncaught exception.
 *
 * This method is called when an uncaught exception is thrown. It prints the exception
 * description and the stacktrace before terminating the program. The same information that
 * is printed to std::cerr is also written to the log file of the executing component.
 * The log file is flushed and the file systems are synced before the program is terminated
 * via abort() so that a core dump is generated.
 */
void
ocs::TerminationManager::terminate_due_to_exception_handler() {
   DENTER(TOP_LAYER);
   std::stringstream ss;

   ss << "Terminate called after throwing an exception:" << std::endl;
   ss << get_exception_description() << std::endl;

   // Try to write into the log file
   std::string line;
   while (std::getline(ss, line)) {
      sge_log(LOG_CRIT, line.c_str(), __FILE__, __LINE__); \
   }

   // Flush all output
   std::cerr.flush();
   std::cout.flush();

   // Sync file systems
   sync();

   // non-caught exception should trigger a core dump
   std::abort();
   DRETURN_VOID;
}

/** @brief Trigger a segmentation fault.
 *
 * This method triggers a segmentation fault by dereferencing a null pointer.
 */
void
ocs::TerminationManager::trigger_segfault() {
   // int* p = reinterpret_cast<int*>(0x12345678);
   int* p = nullptr;
   *p = 42; // This will cause a segmentation fault
}

/** @brief Trigger a stack overflow.
 *
 * This method triggers a stack overflow by calling itself recursively.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
void
ocs::TerminationManager::trigger_stack_overflow(const int iterations) {
   if (iterations > 0) {
      trigger_stack_overflow();
   }
}
#pragma GCC diagnostic pop

/** @brief Trigger an exception.
 *
 * This method triggers an exception by throwing a runtime error.
 */
void
ocs::TerminationManager::trigger_exception() {
   throw std::runtime_error("This is a test exception that was triggered in ocs::TerminationManager::trigger_exception()");
}

