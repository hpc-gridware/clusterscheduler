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

#if defined(OCS_WITH_SYSTEMD)
#include <fstream>
#include <dlfcn.h>

#include "sge_bootstrap_env.h"
#include "sge_log.h"
#include "sge_rmon_macros.h"
#include "sge_time.h"

#include "msg_utilib.h"
#include "ocs_Systemd.h"

namespace ocs::uti {
   //================================================================================
   // static members
   void *ocs::uti::Systemd::lib_handle = nullptr;
   sd_bus_open_system_func_t Systemd::sd_bus_open_system_func = nullptr;
   sd_bus_unref_func_t Systemd::sd_bus_unref_func = nullptr;
   sd_bus_call_method_func_t Systemd::sd_bus_call_method_func = nullptr;
   sd_bus_message_read_func_t Systemd::sd_bus_message_read_func = nullptr;
   sd_bus_message_new_method_call_func_t Systemd::sd_bus_message_new_method_call_func = nullptr;
   sd_bus_message_unref_func_t Systemd::sd_bus_message_unref_func = nullptr;
   sd_bus_message_append_func_t Systemd::sd_bus_message_append_func = nullptr;
   sd_bus_message_open_container_func_t Systemd::sd_bus_message_open_container_func = nullptr;
   sd_bus_message_close_container_func_t Systemd::sd_bus_message_close_container_func = nullptr;
   sd_bus_call_func_t Systemd::sd_bus_call_func = nullptr;
   sd_bus_add_match_func_t Systemd::sd_bus_add_match_func = nullptr;
   sd_bus_match_signal_func_t Systemd::sd_bus_match_signal_func = nullptr;
   sd_bus_slot_unref_func_t Systemd::sd_bus_slot_unref_func = nullptr;
   sd_bus_process_func_t Systemd::sd_bus_process_func = nullptr;
   sd_bus_wait_func_t Systemd::sd_bus_wait_func = nullptr;
   sd_bus_message_get_member_func_t Systemd::sd_bus_message_get_member_func = nullptr;
   sd_bus_message_get_sender_func_t Systemd::sd_bus_message_get_sender_func = nullptr;

   std::string Systemd::slice_name;
   std::string Systemd::service_name;
   const std::string Systemd::execd_service_name{"execd.service"};
   bool Systemd::running_as_service = false;

   // @todo move somewhere else
   static std::string
   get_slice_file_name() {
      std::string ret = bootstrap_get_sge_root();
      ret += "/";
      ret += bootstrap_get_sge_cell();
      ret += "/common/slice_name";
      return ret;
   }

   // @todo move somewhere else
   // @todo add a dstring for reporting errors?
   static bool
   read_one_line_file(const std::string &file_name, std::string &line) {
      bool ret = false;
      std::ifstream file(file_name);
      if (file.is_open()) {
         if (std::getline(file, line)) {
            ret = true;
         }
         file.close();
      }
      return ret;
   }

   // ================================================================================
   // @brief Initialize the Systemd class
   //
   // This function loads the systemd shared library and retrieves the function pointers
   // for the sd-bus API functions. It checks for errors during loading and function
   // retrieval, and returns a boolean indicating success or failure.
   //
   // @param error_dstr A pointer to a dstring where error messages will be stored
   // @return true if initialization was successful, false otherwise
   // @note This function should be called before using any other methods of the Systemd class.
   bool
   Systemd::initialize(std::string service_name_in, dstring *error_dstr) {
      bool ret = true;

      // initialize only once
      if (lib_handle != nullptr) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_SYSTEMD_ALREADY_INITIALIZED);
         ret = false;
      }

      // Load the shared library and the required functions
      if (ret) {
         const char *libsystemd = "libsystemd.so.0";
         lib_handle = dlopen(libsystemd, RTLD_LAZY);
         if (lib_handle == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_OPEN_LIB_SS, libsystemd, dlerror());
            ret = false;
         }
      }

      // load the functions
      const char *func;
      if (ret) {
         func = "sd_bus_open_system";
         sd_bus_open_system_func = reinterpret_cast<sd_bus_open_system_func_t>(dlsym(lib_handle, func));
         if (sd_bus_open_system_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_unref";
         sd_bus_unref_func = reinterpret_cast<sd_bus_unref_func_t>(dlsym(lib_handle, func));
         if (sd_bus_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_call_method";
         sd_bus_call_method_func = reinterpret_cast<sd_bus_call_method_func_t>(dlsym(lib_handle, func));
         if (sd_bus_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_read";
         sd_bus_message_read_func = reinterpret_cast<sd_bus_message_read_func_t>(dlsym(lib_handle, func));
         if (sd_bus_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      if (ret) {
         func = "sd_bus_message_new_method_call";
         sd_bus_message_new_method_call_func = reinterpret_cast<sd_bus_message_new_method_call_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_new_method_call_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_unref";
         sd_bus_message_unref_func = reinterpret_cast<sd_bus_message_unref_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_append";
         sd_bus_message_append_func = reinterpret_cast<sd_bus_message_append_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_append_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_open_container";
         sd_bus_message_open_container_func = reinterpret_cast<sd_bus_message_open_container_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_open_container_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_close_container";
         sd_bus_message_close_container_func = reinterpret_cast<sd_bus_message_close_container_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_close_container_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_call";
         sd_bus_call_func = reinterpret_cast<sd_bus_call_func_t>(dlsym(lib_handle, func));
         if (sd_bus_call_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_add_match";
         sd_bus_add_match_func = reinterpret_cast<sd_bus_add_match_func_t>(dlsym(lib_handle, func));
         if (sd_bus_add_match_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_match_signal";
         sd_bus_match_signal_func = reinterpret_cast<sd_bus_match_signal_func_t>(dlsym(lib_handle, func));
         if (sd_bus_match_signal_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_slot_unref";
         sd_bus_slot_unref_func = reinterpret_cast<sd_bus_slot_unref_func_t>(dlsym(lib_handle, func));
         if (sd_bus_slot_unref_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_process";
         sd_bus_process_func = reinterpret_cast<sd_bus_process_func_t>(dlsym(lib_handle, func));
         if (sd_bus_process_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_wait";
         sd_bus_wait_func = reinterpret_cast<sd_bus_wait_func_t>(dlsym(lib_handle, func));
         if (sd_bus_wait_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_get_sender";
         sd_bus_message_get_sender_func = reinterpret_cast<sd_bus_message_get_sender_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_get_sender_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "sd_bus_message_get_member";
         sd_bus_message_get_member_func = reinterpret_cast<sd_bus_message_get_member_func_t>(dlsym(lib_handle, func));
         if (sd_bus_message_get_member_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      // if we could not load the library or the functions, close the library
      if (!ret && lib_handle != nullptr) {
         dlclose(lib_handle);
         lib_handle = nullptr;
      }

      // we will need the slice name for the service name, read it once
      // once we have a slice name, we can figure out if we are running as service
      service_name = service_name_in;
      std::string slice_file_name = get_slice_file_name();
      if (read_one_line_file(slice_file_name, slice_name)) {
         // build service name, e.g. "ocs6444-execd.service"
         std::string full_service_name = slice_name + "-" + service_name;
         // get unit path of service
         Systemd systemd;
         std::string service_unit_path;
         bool have_service_unit = systemd.sd_bus_method_s_o("GetUnit", full_service_name, service_unit_path, error_dstr);
         // get unit path of pid
         std::string pid_unit_path;
         bool have_pid_unit = systemd.sd_bus_method_u_o("GetUnitByPID", getpid(), pid_unit_path, error_dstr);
         // compare both unit paths, if they are equal then we are running as service
         if (have_service_unit && have_pid_unit && service_unit_path.compare(pid_unit_path) == 0) {
            running_as_service = true;
         }
      }

      return ret;
   }

   bool
   Systemd::is_systemd_available() {
      // Check if systemd is available
      return lib_handle != nullptr;
   }

   bool
   Systemd::is_running_as_service() {
      return running_as_service;
   }

   //================================================================================
   // instance methods
   Systemd::Systemd()
      : bus(nullptr) {
   }

   Systemd::~Systemd() {
      // Destructor implementation
      sd_bus_unref_func(bus);
      bus = nullptr;
   }

   bool
   Systemd::connect(dstring *error_dstr) {
      bool ret = true;

      int r = sd_bus_open_system_func(&bus);
      if (r < 0) {
         sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CONNECT_IS, r, strerror(-r));
         ret = false;
      }

      return ret;
   }

   bool
   Systemd::connected() const {
      return bus != nullptr;
   }

   // @todo no need to have different method names (BUT the name expresses the type of input)
   // @todo how to handle diagnostics output, error messages, ...
   bool
   Systemd::sd_bus_method_s_o(const std::string &method, std::string &input, std::string &output, dstring *error_dstr) const {
      bool ret = true;
      sd_bus_error error = SD_BUS_ERROR_NULL;
      sd_bus_message *m = nullptr;

      int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                  "/org/freedesktop/systemd1",          // object path
                                  "org.freedesktop.systemd1.Manager",   // interface name
                                  method.c_str(),                       // method name
                                  &error,                               // object to return error in
                                  &m,                                   // return message on success
                                  "s",                                  // input signature
                                  input.c_str());                       // first argument
      if (r < 0) {
         sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SIS, method.c_str(), r, error.message);
      } else {
         const char *result = nullptr;
         r = sd_bus_message_read_func(m, "o", &result);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_RESULT_SISS, method.c_str(), r, error.message);
            ret = false;
         } else {
            if (result == nullptr) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_EMPTY_RESULT_S, method.c_str());
               ret = false;
            }
            output = result;
         }
      }

      return ret;
   }

   bool
   Systemd::sd_bus_method_u_o(const std::string &method, uint32_t input, std::string &output, dstring *error_dstr) const {
      bool ret = true;
      sd_bus_error error = SD_BUS_ERROR_NULL;
      sd_bus_message *m = nullptr;

      int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                  "/org/freedesktop/systemd1",          // object path
                                  "org.freedesktop.systemd1.Manager",   // interface name
                                  method.c_str(),                       // method name
                                  &error,                               // object to return error in
                                  &m,                                   // return message on success
                                  "u",                                  // input signature
                                 input);                                // first argument
      if (r < 0) {
         sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SIS, method.c_str(), r, error.message);
      } else {
         const char *result = nullptr;
         r = sd_bus_message_read_func(m, "o", &result);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_RESULT_SISS, method.c_str(), r, error.message);
            ret = false;
         } else {
            if (result == nullptr) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_EMPTY_RESULT_S, method.c_str());
               ret = false;
            }
            output = result;
         }
      }

      return ret;
   }

   bool
   Systemd::move_shepherd_to_scope(pid_t pid, dstring *error_dstr) const {
      DENTER(TOP_LAYER);

      bool ret = true;
      bool create = false;
      std::string full_scope_name = slice_name + "-" + "shepherds.scope";
      std::string full_slice_name = slice_name + ".slice";

      if (ret) {
         DPRINTF("Systemd::move_shepherd_to_scope: Calling GetUnit\n");
         sd_bus_message *m = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         // search the unit (full_scope_name)
         // if it does not exist, then call StartTransientUnit
         // if it exists, then call AttachProcessesToUnit
         // if AttachProcessesToUnit fails with -2 (ENOENT), we might have run into a race condition:
         //    the unit was removed just between GetUnit and AttachProcessesToUnit.
         //    then call StartTransientUnit which will re-create the unit
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                     "/org/freedesktop/systemd1",          // object path
                                     "org.freedesktop.systemd1.Manager",   // interface name
                                     "GetUnit",                            // method name
                                     &error,                               // object to return error in
                                     &m,                                   // return message on success
                                     "s",                                  // input signature
                                     full_scope_name.c_str());             // input argument
         if (r < 0) {
            // ENOENT (-2): scope does not exist
            if (-r == ENOENT) {
               DPRINTF("Systemd::move_shepherd_to_scope: scope does not exist, create it\n");
               create = true;
            } else {
               ret = false;
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS,"GetUnit", full_scope_name.c_str(), r, error.message);
            }
         }
      }

      if (ret && !create) {
         DPRINTF("Systemd::move_shepherd_to_scope: calling AttachProcessesToUnit\n");
         sd_bus_message *m = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         int r = sd_bus_call_method_func(bus, "org.freedesktop.systemd1",  // service to contact
                                     "/org/freedesktop/systemd1",          // object path
                                     "org.freedesktop.systemd1.Manager",   // interface name
                                     "AttachProcessesToUnit",              // method name
                                     &error,                               // object to return error in
                                     &m,                                   // return message on success
                                     "ssau",                               // input signature
                                     full_scope_name.c_str(),              //    -> scope
                                     "",                                   //    -> subcgroup
                                     1, pid);                              // array containing one pid
         if (r < 0) {
            // ENOENT (-2): scope does not yet exist
            // we just checked that above, but there might be a race condition
            // the scope might just have been removed between checking and trying to attach
            // in this case we create the scope (call StartTransientUnit below)
            if (-r == ENOENT) {
               DPRINTF("Systemd::move_shepherd_to_scope: scope no longer exists, calling StartTransientUnit\n");
               create = true;
            } else {
               ret = false;
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS,"AttachProcessesToUnit", full_scope_name.c_str(), r, error.message);
            }
         }
      }

      if (ret && create) {
         DPRINTF("Systemd::move_shepherd_to_scope: calling StartTransientUnit\n");
         sd_bus_message *m = nullptr;
         sd_bus_error error = SD_BUS_ERROR_NULL;
         // build the method step by step as we add arrays
         int r = sd_bus_message_new_method_call_func(bus, &m,
                 "org.freedesktop.systemd1",          // service to contact
                 "/org/freedesktop/systemd1",         // object path
                 "org.freedesktop.systemd1.Manager",  // interface name
                 "StartTransientUnit");       // method name
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CREATE_MESSAGE_CALL_SIS, "StartTransientUnit", r, strerror(-r));
            ret = false;
         }

         if (ret) {
            r = sd_bus_message_append_func(m, "ss", full_scope_name.c_str(), "replace");
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_TO_MESSAGE_SSIS, "name and mode", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }

         if (ret) {
            r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_ARRAY, "(sv)");
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_OPEN_CONTAINER_SSIS, "properties", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }

         if (ret) {
            r = sd_bus_message_append_func(m, "(sv)", "Delegate", "b", 1);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "Delegate", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }
         if (ret) {
            r = sd_bus_message_append_func(m, "(sv)", "Slice", "s", full_slice_name.c_str());
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "Slice", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }
         if (ret) {
            r = sd_bus_message_append_func(m, "(sv)", "PIDs", "au", 1, pid);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_APPEND_PROPERTY_SSIS, "PIDs", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }
         if (ret) {
            r = sd_bus_message_close_container_func(m);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CLOSE_CONTAINER_SSIS, "properties", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }
         if (ret) {
            r = sd_bus_message_open_container_func(m, SD_BUS_TYPE_ARRAY, "(sa(sv))");
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_OPEN_CONTAINER_SSIS, "aux", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }
         if (ret) {
            r = sd_bus_message_close_container_func(m);
            if (r < 0) {
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CLOSE_CONTAINER_SSIS, "aux", "StartTransientUnit", r, strerror(-r));
               ret = false;
            }
         }
         if (ret) {
            sd_bus_message *reply = nullptr;
            r = sd_bus_call_func(bus, m, 0, &error, &reply);
            if (r < 0) {
               // Special handling for -17: EEXIST: scope already exists? No, who would create the scope for us?
               sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_CALL_SSIS, "StartTransientUnit", full_scope_name.c_str(), r, error.message);
               ret = false;
            } else {
               const char *job = nullptr;
               r = sd_bus_message_read_func(reply, "o", &job);
               if (r < 0) {
                  sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_READ_RESULT_SISS, "StartTransientUnit", r, strerror(-r));
                  ret = false;
               } else {
                  if (job == nullptr) {
                     sge_dstring_sprintf(error_dstr, MSG_SYSTEMD_CANNOT_EMPTY_RESULT_S, "StartTransientUnit");
                     ret = false;
                  } else {
                     // wait for the job to finish
                     sd_bus_wait_for_job_completion(job, error_dstr);
                  }
               }
            }
            sd_bus_message_unref_func(reply);
         }

         sd_bus_message_unref_func(m);
         // @todo: do we need to call sd_bus_error_free(&error)?
      }

      DRETURN(ret);
   }


   bool
   Systemd::sd_bus_wait_for_job_completion(const std::string &job_path, dstring *error_dstr) const {
      DENTER(TOP_LAYER);
      DPRINTF("==> sd_bus_wait_for_job_completion(%s)", job_path.c_str());

      bool ret = true;

      // add match for JobRemoved signal
      sd_bus_slot *slot = nullptr;
      if (ret) {
         std::string match_rule = "type='signal',interface='org.freedesktop.systemd1.Manager',member='JobRemoved'";
         int r = sd_bus_add_match_func(bus, &slot, match_rule.c_str(), nullptr, nullptr);
         // @todo should also work but doesn't
         // int r = sd_bus_match_signal_func(bus, &slot, nullptr, nullptr, "JobRemoved", nullptr, nullptr);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, SFN ": adding match func " SFQ " failed: error %d: " SFN, __func__, match_rule.c_str(), r, strerror(-r));
            ret = false;
         }
      }

      u_long64 timeout = sge_get_gmt64() + 1000000; // 1 second timeout
      while (ret == true) {
         if (sge_get_gmt64() > timeout) {
            sge_dstring_sprintf(error_dstr, SFN ": timeout waiting for job completion", __func__);
            ret = false;
            break;
         }

         // wait for the next signal
         sd_bus_message *m = nullptr;
         int r = sd_bus_process_func(bus, &m);
         DPRINTF("sd_bus_process_func(bus, &m) returned %d", r);
         if (r < 0) {
            sge_dstring_sprintf(error_dstr, SFN ": processing bus failed: error %d: " SFN, __func__, r, strerror(-r));
            ret = false;
         } else {
            // 0 means we need to wait before calling sd_bus_process again
            if (r == 0) {
               // we wait with timeout
               // sd_bus_wait() returns 0 on timeout, not an error, final timeout handled above
               // does it actually make sense to use a timeout < our final timeout? We use 100ms for now.
               r = sd_bus_wait_func(bus, 100000);
               DPRINTF("sd_bus_wait_func(bus, nullptr) returned %d", r);
               if (r < 0) {
                  sge_dstring_sprintf(error_dstr, SFN ": waiting for bus failed: error %d: " SFN, __func__, r, strerror(-r));
                  ret = false;
               }
               // do the next sd_bus_process call
               sd_bus_message_unref_func(m);
               continue;
            }
         }

         // sd_bus_process read signal
         if (ret && m != nullptr) {
            if (strcmp(sd_bus_message_get_member_func(m), "JobRemoved") == 0) {
               DPRINTF("got JobRemoved signal");
               const char *completed_job_path = nullptr;
               // message contains for a signal:
               // `u`: job id, e.g. `1234`
               // `o`: job path, e.g. `/org/freedesktop/systemd1/job/1234`
               // `s`: status, e.g. `"done"`
               r = sd_bus_message_read_func(m, "uos", nullptr, &completed_job_path, nullptr);
               DPRINTF("sd_bus_message_read_func(m, \"uos\", &completed_job_path) returned %d", r);
               if (r < 0) {
                  sge_dstring_sprintf(error_dstr, SFN ": reading job path failed: error %d: " SFN, __func__, r, strerror(-r));
                  ret = false;
               } else {
                  DPRINTF("completed_job_path = %s", completed_job_path);
                  if (job_path.compare(completed_job_path) == 0) {
                     sd_bus_message_unref_func(m);
                     break; // job done
                  }
               }
            }
         }

         sd_bus_message_unref_func(m);
      }

      // release the match pattern
      sd_bus_slot_unref_func(slot);

      DRETURN(ret);
   }

} // namespace
#endif
