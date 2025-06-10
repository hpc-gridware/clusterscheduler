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
      // @todo have a config option to enable/disable systemd integration
      if (g_use_systemd) {
         // try to initialize the Systemd integration,
         // create an instance of Systemd and try to connect to the system bus,
         // figure out if we are running as Systemd service
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (ocs::uti::Systemd::initialize(ocs::uti::Systemd::shepherd_scope_name, &error_dstr)) {
            shepherd_trace("initialized systemd library");
            if (ocs::uti::Systemd::is_running_as_service()) {
               shepherd_trace("shepherd is running under systemd control in scope %s, systemd version %d, cgroups version %d",
                              ocs::uti::Systemd::shepherd_scope_name.c_str(), ocs::uti::Systemd::get_systemd_version(),
                              ocs::uti::Systemd::get_cgroup_version());
            } else {
               shepherd_trace("shepherd is not running under systemd control");
               g_use_systemd = false;
            }
         } else if (sge_dstring_strlen(&error_dstr) > 0) {
            shepherd_trace("initializing systemd library failed: %s", sge_dstring_get_string(&error_dstr));
            g_use_systemd = false;
         }
      }
#endif
   }

   // needs to be called before switching to the job user, when we can still become start_user (root)
   void
   move_shepherd_child_to_job_scope(int pid) {
   // move the shepherd child to the job scope
   // we do this only for the job, not for prolog, epilog, pe_start, pe_stop
   // @todo we might want to add a execd_params whether to account prolog etc. to the job
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

      // @todo device isolation
      //       DeviceAllow, array of structs having two strings: device name and access mode: a(ss)
      u_long64 start_time = sge_get_gmt64();
      const char *slice = get_conf_val("systemd_slice");
      const char *scope = get_conf_val("systemd_scope");
      if (slice != nullptr && scope != nullptr) {
         ocs::uti::Systemd systemd;
         sge_switch2start_user();
         bool connected = systemd.connect(&error_dstr);
         sge_switch2admin_user();
         if (connected) {
            pid_t pid = getpid();
            shepherd_trace("moving shepherd child " pid_t_fmt "to job scope '%s' in slice '%s'", pid, scope, slice);
            bool success = systemd.create_scope_with_pid(scope, slice, g_systemd_properties, pid, &error_dstr);
            shepherd_trace("moving shepherd child took " sge_u64 " µs", sge_get_gmt64() - start_time);
            if (!success) {
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
   bool
   add_binding_to_systemd_properties(hwloc_const_bitmap_t cpuset) {
      bool ret = true;
      unsigned i;

      // create a vector of uint8_t containing the CPU mask as bits
      std::vector<uint8_t> cpu_mask(hwloc_bitmap_last(cpuset) / 8 + 1, 0);
      hwloc_bitmap_foreach_begin(i, cpuset) {
         shepherd_trace("adding CPU %d to AllowedCPUs", i);
         cpu_mask[i/8] |= (1 << (i % 8));
      }
      hwloc_bitmap_foreach_end();
      for (i = 0; i < cpu_mask.size(); ++i) {
         shepherd_trace("==> byte %d: %08b", i, cpu_mask[i]);
      }

      g_systemd_properties["AllowedCPUs"] = cpu_mask;

      return ret;
   }
#endif


} // namespace ocs
