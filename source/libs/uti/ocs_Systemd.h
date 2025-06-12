#pragma once
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

#include <map>
#include <string>
#include <variant>
#include <vector>

#if defined(OCS_WITH_SYSTEMD)
#include <systemd/sd-bus.h>

#include "sge_dstring.h"
#endif

namespace ocs::uti {
   // systemd properties type
   // we use it in the signature of a shepherd function - therefore, we need it outside OCS_WITH_SYSTEMD
   using SystemdDevice_t =  std::pair<std::string, std::string>;
   using SystemdProperty_t = std::variant<std::string, uint64_t, bool,
                                          std::vector<uint8_t>, std::vector<SystemdDevice_t>>;
   using SystemdProperties_t = std::map<std::string, SystemdProperty_t>;

#if defined(OCS_WITH_SYSTEMD)

   // function types for the sdbus interface
   using sd_bus_open_system_func_t = int (*)(sd_bus **bus);
   using sd_bus_unref_func_t = sd_bus *(*)(sd_bus *bus);
   using sd_bus_call_method_func_t = int (*)(sd_bus *bus, const char *destination, const char *path,
      const char *interface, const char *member, sd_bus_error *ret_error, sd_bus_message **reply,
      const char *types, ...);
   using sd_bus_message_read_func_t = int (*)(sd_bus_message *m, const char *types, ...);
   using sd_bus_message_new_method_call_func_t = int (*)(sd_bus *bus, sd_bus_message **m,
      const char *destination, const char *path, const char *interface, const char *member);
   using sd_bus_message_unref_func_t = int *(*)(sd_bus_message *m);
   using sd_bus_message_append_func_t = int (*)(sd_bus_message *m, const char *types, ...);
   using sd_bus_message_append_array_func_t = int (*)(sd_bus_message *m, char type, const void *ptr, size_t size);
   using sd_bus_message_open_container_func_t = int (*)(sd_bus_message *m, int type, const char *types);
   using sd_bus_message_close_container_func_t = int (*)(sd_bus_message *m);
   using sd_bus_message_dump_func_t = int (*)(sd_bus_message *m, FILE *f, int flags);
   using sd_bus_message_rewind_func_t = int (*)(sd_bus_message *m, int complete);
   using sd_bus_call_func_t = int (*)(sd_bus *bus, sd_bus_message *m, uint64_t usec, sd_bus_error *error, sd_bus_message **reply);
   using sd_bus_add_match_func_t = int (*)(sd_bus *bus, sd_bus_slot **slot, const char *match, sd_bus_message_handler_t callback, void *userdata);
   using sd_bus_match_signal_func_t = int (*)(sd_bus *bus, sd_bus_slot **slot, const char *path, const char *interface, const char *member, sd_bus_message_handler_t callback, void *userdata);
   using sd_bus_slot_unref_func_t = int (*)(sd_bus_slot *slot);
   using sd_bus_process_func_t = int (*)(sd_bus *bus, sd_bus_message **m);
   using sd_bus_wait_func_t = int (*)(sd_bus *bus, int timeout_usec);
   using sd_bus_message_get_sender_func_t = const char *(*)(sd_bus_message *m);
   using sd_bus_message_get_member_func_t = const char *(*)(sd_bus_message *m);
   using sd_bus_path_encode_func_t = int (*)(const char *prefix, const char *external_id, char **ret_path);
   using sd_bus_get_property_func_t = int (*)(sd_bus *bus, const char *destination, const char *path, const char *interface, const char *member, sd_bus_error *ret_error, sd_bus_message **reply, const char *type);
   using sd_bus_error_free_func_t = void (*)(sd_bus_error *error);

   // @brief Systemd class
   //
   // This class provides an interface to interact with systemd using the sd-bus API.
   // It allows for opening a system bus connection, making method calls, and handling messages.
   // It has static methods for initialization and checking systemd availability.
   // An instance of the class connects to the system bus and provides methods for
   // interacting with systemd services.
   class Systemd {
      private:
         // static data
         // handle and function pointers of the libsystemd.so
         static void *lib_handle;
         static sd_bus_open_system_func_t sd_bus_open_system_func;
         static sd_bus_unref_func_t sd_bus_unref_func;
         static sd_bus_call_method_func_t sd_bus_call_method_func;
         static sd_bus_message_read_func_t sd_bus_message_read_func;
         static sd_bus_message_new_method_call_func_t sd_bus_message_new_method_call_func;
         static sd_bus_message_unref_func_t sd_bus_message_unref_func;
         static sd_bus_message_append_func_t sd_bus_message_append_func;
         static sd_bus_message_append_array_func_t sd_bus_message_append_array_func;
         static sd_bus_message_open_container_func_t sd_bus_message_open_container_func;
         static sd_bus_message_close_container_func_t sd_bus_message_close_container_func;
         static sd_bus_message_dump_func_t sd_bus_message_dump_func;
         static sd_bus_message_rewind_func_t sd_bus_message_rewind_func;
         static sd_bus_call_func_t sd_bus_call_func;
         static sd_bus_add_match_func_t sd_bus_add_match_func;
         static sd_bus_match_signal_func_t sd_bus_match_signal_func;
         static sd_bus_slot_unref_func_t sd_bus_slot_unref_func;
         static sd_bus_process_func_t sd_bus_process_func;
         static sd_bus_wait_func_t sd_bus_wait_func;
         static sd_bus_message_get_sender_func_t sd_bus_message_get_sender_func;
         static sd_bus_message_get_member_func_t sd_bus_message_get_member_func;
         static sd_bus_path_encode_func_t sd_bus_path_encode_func;
         static sd_bus_get_property_func_t sd_bus_get_property_func;
         static sd_bus_error_free_func_t sd_bus_error_free_func;

         // name of toplevel slice (from $SGE_ROOT/$SGE_CELL/common/slice_name, when running under Systemd control)
         static std::string slice_name;
         static std::string service_name; // @todo it is e.g. "execd.service" but should be the full service name
         static bool running_as_service;
         static int cgroup_version;
         static int systemd_version;

      public:
         // constants
         static const std::string execd_service_name;
         static const std::string shepherd_scope_name;

         // static methods
         static bool initialize(std::string service_name_in, dstring *error_dstr);
         static bool is_systemd_available();
         static bool is_running_as_service();
         static std::string get_slice_name() { return slice_name; }
         static int get_cgroup_version() { return cgroup_version; }
         static int get_systemd_version() { return systemd_version; }

      private:
         // instance data
         sd_bus *bus;

         // instance methods
         bool sd_bus_method_s_o(const std::string &method, std::string &input, std::string &output, dstring *error_dstr) const;
         bool sd_bus_method_u_o(const std::string &method, uint32_t input, std::string &output, dstring *error_dstr) const;
         bool sd_bus_wait_for_job_completion(const std::string &job_path, dstring *error_dstr) const;
         std::string get_unit_for_pid();
         std::string get_unit_for_service(std::string &service);

      public:
         Systemd();
         ~Systemd();

         bool connect(dstring *error_dstr);
         bool connected() const;

         bool move_shepherd_to_scope(pid_t pid, dstring *error_dstr) const;
         bool create_scope_with_pid(const std::string &scope, const std::string &slice,
                                    const SystemdProperties_t &properties, pid_t pid, dstring *error_dstr) const;

         bool sd_bus_get_property(const std::string &interface, const std::string &unit, const std::string &property, std::string &value, dstring *error_dstr) const;
         bool sd_bus_get_property(const std::string &interface, const std::string &unit, const std::string &property, uint64_t &value, dstring *error_dstr) const;
   };

#endif

}
