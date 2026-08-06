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

/** @file
 * @brief Talking to systemd over D-Bus: slices, scopes and cgroup limits
 */

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include <cstdint>


#if defined(OCS_WITH_SYSTEMD)
#include <systemd/sd-bus.h>

#include "sge_dstring.h"
#endif

namespace ocs::uti {
   // we use it in the signature of a shepherd function - therefore, we need it outside OCS_WITH_SYSTEMD
   /// One device a cgroup may be granted access to: its path and the permissions
   using SystemdDevice_t =  std::pair<std::string, std::string>;
   /// The value of one systemd property, in any of the types systemd reports
   using SystemdProperty_t = std::variant<std::string, uint64_t, bool,
                                          std::vector<uint8_t>, std::vector<SystemdDevice_t>>;
   /// A set of systemd properties, keyed by property name
   using SystemdProperties_t = std::map<std::string, SystemdProperty_t>;

#if defined(OCS_WITH_SYSTEMD)

   /// @cond   pointer types mirroring the libsystemd sd-bus ABI, resolved with dlsym at runtime
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
   /// @endcond

   // @brief Systemd class
   //
   /**
    * @brief Talks to systemd over the sd-bus API
    *
    * `libsystemd` is opened with `dlopen` at runtime rather than linked, so a
    * binary built with systemd support still starts on a host without it;
    * #is_systemd_available reports which case applies.
    *
    * The static half loads the library and answers questions about the
    * environment. An instance owns a connection to the system bus and is what
    * you make method calls through.
    *
    * Used to place the execd's jobs in their own systemd scopes, so that the
    * cgroup limits systemd enforces apply per job.
    */
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
         static std::string service_name;
         static bool running_as_service;
         static int cgroup_version;
         static int systemd_version;
         static std::map<std::string, bool>unclear_properties;   // properties that are not available on all OSes or Systemd versions

      public:
         // constants
         /// Name of the systemd service the execd runs as
         static const std::string execd_service_name;
         /// Name prefix of the systemd scope created for each job's shepherd
         static const std::string shepherd_scope_name;

         // static methods
         /**
          * @brief Open libsystemd and work out how this process is running
          *
          * Resolves the sd-bus entry points, then determines whether the
          * process is itself running as a systemd service, which cgroup
          * version is in use and which systemd version.
          *
          * @param service_name_in name of the service this process runs as
          * @param[out] error_dstr receives the reason on failure
          * @return true on success
          */
         static bool initialize(const std::string &service_name_in, dstring *error_dstr);
         static bool is_systemd_available();  // we can load the systemd library and connect to systemd
         static bool is_running_as_service(); // the process is running as a systemd service
         /**
          * @brief The top level slice jobs are placed under
          * @return the slice name, read from the cell's `slice_name` file, or
          *         empty when not running under systemd control
          */
         static std::string get_slice_name() { return slice_name; }
         /**
          * @brief Which cgroup hierarchy the host uses
          * @return 1 or 2, or 0 when it could not be determined
          */
         static int get_cgroup_version() { return cgroup_version; }
         /**
          * @brief The systemd version this host runs
          * @return the major version, or 0 when it could not be determined
          */
         static int get_systemd_version() { return systemd_version; }

      private:
         // instance data
         sd_bus *bus;

         // instance methods
         bool sd_bus_method_s_o(const std::string &method, std::string &input, std::string &output, dstring *error_dstr) const;
         bool sd_bus_method_u_o(const std::string &method, uint32_t input, std::string &output, dstring *error_dstr) const;
         sd_bus_slot *sd_bus_wait_for_job_subscribe(const std::string &signal, dstring *error_dstr) const;
         void sd_bus_wait_for_job_unsubscribe(sd_bus_slot **slot) const;
         bool sd_bus_wait_for_job_completion(const std::string &job_path, dstring *error_dstr) const;

      public:
         Systemd();
         ~Systemd();

         bool connect(dstring *error_dstr);
         bool connected() const;

         bool move_shepherd_to_scope(pid_t pid, dstring *error_dstr) const;
         bool create_scope_with_pid(const std::string &scope, const std::string &slice,
                                    const SystemdProperties_t &properties, pid_t pid, bool &scope_already_exists, dstring *error_dstr) const;
         bool
         attach_pid_to_scope(const std::string &scope, pid_t pid, bool &scope_not_exists, dstring *error_dstr) const;

         bool has_property(const std::string &property_name, const std::string &scope_name);
         bool sd_bus_get_property(const std::string &interface, const std::string &unit, const std::string &property,
                                  std::string &value, dstring *error_dstr, bool *not_exists = nullptr) const;
         bool sd_bus_get_property(const std::string &interface, const std::string &unit, const std::string &property,
                                  uint64_t &value, dstring *error_dstr, bool *not_exists = nullptr) const;

         bool stop_unit(const std::string &unit, dstring *error_dstr) const;
         bool freeze_unit(const std::string &unit, dstring *error_dstr) const;
         bool thaw_unit(const std::string &unit, dstring *error_dstr) const;
         bool signal_unit(const std::string &unit, int signal, bool only_main, dstring *error_dstr) const;
   };

#endif

}
