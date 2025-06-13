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

#include <cstring>

#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"

#include "config_file.h"
#include "err_trace.h"
#include "ocs_shepherd_systemd.h"

namespace ocs {
   bool g_use_systemd = true;
   ocs::uti::SystemdProperties_t g_systemd_properties;

   void shepherd_systemd_init() {
#if defined (OCS_WITH_SYSTEMD)
      // we can enable/disable systemd integration via execd_param ENABLE_SYSTEMD
      g_use_systemd = std::stoi(get_conf_val("enable_systemd")) != 0;
      if (g_use_systemd) {
         // try to initialize the Systemd integration,
         // create an instance of Systemd and try to connect to the system bus,
         // figure out if we are running as Systemd service
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (ocs::uti::Systemd::initialize(ocs::uti::Systemd::shepherd_scope_name, &error_dstr)) {
            shepherd_trace("initialized systemd library, systemd version %d, cgroups version %d",
                           ocs::uti::Systemd::get_systemd_version(), ocs::uti::Systemd::get_cgroup_version());
            if (ocs::uti::Systemd::is_running_as_service()) {
               // we are running under systemd control
               shepherd_trace("shepherd is running under systemd control in scope %s",
                              ocs::uti::Systemd::shepherd_scope_name.c_str());
            }
         } else {
            shepherd_trace("initializing systemd library failed: %s", sge_dstring_get_string(&error_dstr));
            g_use_systemd = false;
         }
      } else {
         shepherd_trace("systemd integration is disabled");
      }
#endif
   }

#if defined (OCS_WITH_SYSTEMD)
   static void
   add_accounting_settings() {
      if (g_use_systemd) {
         // @todo there would also be
         //   - BlockAccounting (deprecated by IOAccounting)
         //   - IPAccounting
         //   - TasksAccounting
         g_systemd_properties["CPUAccounting"] = true;
         g_systemd_properties["MemoryAccounting"] = true;
         if (ocs::uti::Systemd::get_cgroup_version() == 2) {
            g_systemd_properties["IOAccounting"] = true;
         }
      }
   }

#define DEVICES_DELIMITOR ";"
#define DEVICES_DEFAULT_MODE "r"
   //       DeviceAllow, array of structs having two strings: device name and access mode: a(ss)
   //          use config file entry devices_allow to specify devices which are allowed
   //          @todo have an execd_params for devices which shall always be allowed?
   //       DevicePolicy, string:
   //          "strict" - no devices allowed except what is specified in DeviceAllow
   //          "closed" - like strict, but also allows /dev/null, /dev/zero, /dev/full, /dev/random, /dev/urandom
   //          "auto" - allows all devices, unless DeviceAllow is set, then it behaves like closed (?)
   //          @todo have an execd_params for this?
   static void
   add_devices_allow() {
      if (g_use_systemd) {
         char *devices_allow = get_conf_val("devices_allow");
         if (devices_allow != nullptr && strlen(devices_allow) > 0) {
            // switch to closed device policy
            g_systemd_properties["DevicePolicy"] = "closed";
            std::vector<ocs::uti::SystemdDevice_t> devices;
            saved_vars_s *context = nullptr;
            char *device = sge_strtok_r(devices_allow, DEVICES_DELIMITOR, &context);
            ocs::uti::SystemdDevice_t systemd_device{};
            while (device != nullptr) {
               // device is a string of the form "device_name=access_mode"
               // where access_mode can contain "r", "w", "rw"
               char *access_mode = strchr(device, '=');
               if (access_mode == nullptr || *access_mode == '\0') {
                  shepherd_trace("no mode specifice for device %s, using \"rw\" as default", device);
                  systemd_device.second = DEVICES_DEFAULT_MODE; // default access mode
               } else {
                  *access_mode = '\0'; // split device name and access mode
                  access_mode++;
                  systemd_device.second = access_mode;
               }
               systemd_device.first = device; // device name
               shepherd_trace("adding device %s with access mode %s to systemd properties DeviceAllow",
                              systemd_device.first.c_str(), systemd_device.second.c_str());
               devices.push_back(systemd_device);

               // optionally next device
               device = sge_strtok_r(nullptr, DEVICES_DELIMITOR, &context);
            }
            g_systemd_properties["DeviceAllow"] = devices;
         }
      }
   }
#endif

   // needs to be called before switching to the job user, when we can still become start_user (root)
   void
   move_shepherd_child_to_job_scope(pid_t pid) {
   // move the shepherd child to the job scope
   // we do this only for the job, not for prolog, epilog, pe_start, pe_stop
   // @todo we might want to add an execd_params whether to account prolog etc. to the job
#if defined (OCS_WITH_SYSTEMD)
   if (g_use_systemd) {
      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
      // @todo there would also be
      //   - BlockAccounting (deprecated by IOAccounting)
      //   - IPAccounting
      //   - TasksAccounting
      g_systemd_properties["CPUAccounting"] = true;
      g_systemd_properties["MemoryAccounting"] = true;
      if (ocs::uti::Systemd::get_cgroup_version() == 2) {
         g_systemd_properties["IOAccounting"] = true;
      }

      add_accounting_settings();
      add_devices_allow();

      const u_long64 start_time = sge_get_gmt64();
      const char *slice = get_conf_val("systemd_slice");
      const char *scope = get_conf_val("systemd_scope");
      if (slice != nullptr && scope != nullptr) {
         ocs::uti::Systemd systemd;
         sge_switch2start_user();
         bool connected = systemd.connect(&error_dstr);
         sge_switch2admin_user();
         if (connected) {
            shepherd_trace("moving shepherd child " pid_t_fmt " to job scope '%s' in slice '%s'", pid, scope, slice);
            bool success = systemd.create_scope_with_pid(scope, slice, g_systemd_properties, pid, &error_dstr);
            if (success) {
               shepherd_trace("moving shepherd child took " sge_u64 " µs", sge_get_gmt64() - start_time);
            } else {
               shepherd_error(1, "moving shepherd child to job scope failed: %s", sge_dstring_get_string(&error_dstr));
            }
         } else {
            // we treat a connect-error as fatal, connecting worked before
            shepherd_error(1, "connecting to systemd failed: %s", sge_dstring_get_string(&error_dstr));
         }
      } else {
         shepherd_error(1, "systemd_slice and/or systemd_scope missing in config file, cannot move shepherd child to job scope");
      }
   }
#endif
}


   /**
    * @brief Adds the CPU binding to the systemd properties.
    *
    * Fills in a vector of uint8_t with the CPU mask as bits and adds it to
    * the systemd properties under the key "AllowedCPUs".
    *
    * @param cpuset - cpuset from hwloc which contains the CPU binding (logical cpus).
    * @return
    */
#if defined(OCS_HWLOC)
   void
   add_binding_to_systemd_properties(const hwloc_const_bitmap_t cpuset) {
      if (g_use_systemd) {
         unsigned i;

         // create a vector of uint8_t containing the CPU mask as bits
         std::vector<uint8_t> cpu_mask(hwloc_bitmap_last(cpuset) / 8 + 1, 0);
         hwloc_bitmap_foreach_begin(i, cpuset) {
            shepherd_trace("adding CPU %d to AllowedCPUs", i);
            cpu_mask[i/8] |= 1 << (i % 8);
         }
         hwloc_bitmap_foreach_end();
         for (i = 0; i < cpu_mask.size(); ++i) {
            shepherd_trace("==> byte %d: %08b", i, cpu_mask[i]);
         }

         g_systemd_properties["AllowedCPUs"] = cpu_mask;
      }
   }
#endif

} // namespace ocs
