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

#include "cull/cull.h"

#include "sgeobj/cull/sge_ptf_JL_L.h"
#include "sgeobj/cull/sge_ptf_JO_L.h"
#include "sgeobj/sge_usage.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/ocs_Systemd.h"

#include "execd_systemd.h"

#include <sge_profiling.h>

#include "msg_execd.h"
#include "ptf.h"

// from ptf.cc
extern lList *ptf_jobs;

namespace ocs::execd {
#if defined(OCS_WITH_SYSTEMD)

   void execd_systemd_init() {
      // try to initialize the Systemd integration,
      // create an instance of Systemd and try to connect to the system bus,
      // figure out if we are running as Systemd service
      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
      if (ocs::uti::Systemd::initialize(ocs::uti::Systemd::execd_service_name, &error_dstr)) {
         u_long32 old_ll = log_state_get_log_level();
         log_state_set_log_level(LOG_INFO);
         INFO(MSG_SYSTEMD_INITIALIZED_II, ocs::uti::Systemd::get_systemd_version(),
                                          ocs::uti::Systemd::get_cgroup_version());
         if (ocs::uti::Systemd::is_running_as_service()) {
            INFO(MSG_SYSTEMD_RUNNING_AS_SERVICE_S, ocs::uti::Systemd::execd_service_name.c_str());
         }
         log_state_set_log_level(old_ll);
      } else if (sge_dstring_strlen(&error_dstr) > 0) {
         WARNING(SFNMAX, sge_dstring_get_string(&error_dstr));
      }
   }

   bool
   execd_move_shepherd_to_scope() {
      bool ret = true;

      if (ocs::uti::Systemd::is_running_as_service()) {
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM2);
         DSTRING_STATIC(err_dstr, MAX_STRING_SIZE);
         ocs::uti::Systemd systemd;
         // connect as root, we want to have write access
         sge_switch2start_user();
         bool connected = systemd.connect(&err_dstr);
         sge_switch2admin_user();
         if (connected) {
            pid_t pid = getpid();
            bool success = systemd.move_shepherd_to_scope(pid, &err_dstr);
            if (!success) {
               WARNING(MSG_EXECD_SYSTEMD_MOVE_SHEPHERD_TO_SCOPE_S, sge_dstring_get_string(&err_dstr));
               ret = false;
            }
         } else {
            // connect failed
            WARNING(SFNMAX, sge_dstring_get_string(&err_dstr));
            ret = false;
         }
         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM2);
         if (prof_is_active(SGE_PROF_CUSTOM2)) {
            double prof_systemd = prof_get_measurement_wallclock(SGE_PROF_CUSTOM2, true, nullptr);
            PROFILING("PROF: moving shepherd to systemd scope took %.6f seconds", prof_systemd);
         }
      }

      return ret;
   }

   static void
   ptf_get_usage_value_from_systemd(ocs::uti::Systemd &systemd, std::string &scope, lList *usage_list, const char *property_str, const char *usage_attr_str, double factor) {
      DENTER(TOP_LAYER);

      lListElem *usage_elem = lGetElemStrRW(usage_list, UA_name, usage_attr_str);
      // the usage element must already exist in the usage_list, don't create a new one
      if (usage_elem != nullptr) {
         // Get the usage value from systemd
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         uint64_t value{};
         std::string property{property_str};
         if (systemd.sd_bus_get_property("Scope", scope, property, value, &error_dstr)) {
            if (usage_elem != nullptr) {
               double usage_value = value * factor;
               lSetDouble(usage_elem, UA_value, usage_value);
               DPRINTF("==> Updated usage %s for scope '%s': %f", usage_attr_str, scope.c_str(), usage_value);
            }
         } else {
            // I18N, and the message should already contain all necessary information (?)
            WARNING(MSG_CANNOT_TO_GET_PROPERTY_SSS, property.c_str(), scope.c_str(), sge_dstring_get_string(&error_dstr));
         }
      }

      DRETURN_VOID;
   }

#if 0
   static void
   ptf_get_usage_value_from_systemd2(ocs::uti::Systemd &systemd, std::string &scope, lList *usage_list, const char *property1_str, const char *property2_str, const char *usage_attr_str, double factor) {
      DENTER(TOP_LAYER);

      lListElem *usage_elem = lGetElemStrRW(usage_list, UA_name, usage_attr_str);
      // the usage element must already exist in the usage_list, don't create a new one
      if (usage_elem != nullptr) {
         // Get the usage value from systemd
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         uint64_t value1{}, value2{};
         std::string property = property1_str;
         if (!systemd.sd_bus_get_property("Scope", scope, property, value1, &error_dstr)) {
            // I18N, and the message should already contain all necessary information (?)
            WARNING(MSG_CANNOT_TO_GET_PROPERTY_SSS, property.c_str(), scope.c_str(), sge_dstring_get_string(&error_dstr));
         }
         property = property2_str;
         if (!systemd.sd_bus_get_property("Scope", scope, property, value2, &error_dstr)) {
            // I18N, and the message should already contain all necessary information (?)
            WARNING(MSG_CANNOT_TO_GET_PROPERTY_SSS, property.c_str(), scope.c_str(), sge_dstring_get_string(&error_dstr));
         }
         double usage_value = (value1 + value2) * factor;
         lSetDouble(usage_elem, UA_value, usage_value);
         DPRINTF("Updated usage %s: %lu + %s: %lu = %f for scope '%s'", property1_str, value1, property2_str, value2, usage_value, scope.c_str());
      }

      DRETURN_VOID;
   }
#endif

   void
   ptf_get_usage_from_systemd() {
      DENTER(TOP_LAYER);

      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);

      // Initialize the systemd connection and retrieve usage information
      ocs::uti::Systemd systemd;
      bool success = systemd.connect(&error_dstr);
      if (!success) {
         WARNING(MSG_EXECD_CANNOT_CONNECT_TO_SYSTEMD_S, sge_dstring_get_string(&error_dstr));
      } else {
         // Loop over all jobs, ja_tasks, and pe_tasks
         lListElem *ptf_job;
         for_each_rw (ptf_job, ptf_jobs) {
            lListElem *os_job;
            for_each_rw (os_job, lGetList(ptf_job, JL_OS_job_list)) {
               const char *scope_str = lGetString(os_job, JO_systemd_scope);
               if (scope_str != nullptr) {
                  std::string scope{scope_str};
                  std::string state;
                  success = systemd.sd_bus_get_property("Unit", scope, "ActiveState", state, &error_dstr);
                  if (success) {
                      if (state.compare("active") == 0) {
                        DPRINTF("==> Job is active in systemd scope %s", scope.c_str());
                        lList *usage_list = lGetListRW(os_job, JO_usage_list);
                        if (usage_list == nullptr) {
                           usage_list = ptf_build_usage_list("usagelist");
                           lSetList(os_job, JO_usage_list, usage_list);
                        }
                        //u_int64_t sd_vmem, sd_cpu; // @todo rss, max_rss, max_vmem, io
                        // from systemd we do *not* get vmem / maxvmem
                        // @todo: io usage
                        ptf_get_usage_value_from_systemd(systemd, scope, usage_list, "CPUUsageNSec", USAGE_ATTR_CPU, 1.0 / 1000000000.0); // convert nanoseconds to seconds
                        ptf_get_usage_value_from_systemd(systemd, scope, usage_list, "MemoryCurrent", USAGE_ATTR_RSS, 1.0);
                        // With cgroup v2 we can get MemoryPeak, with cgroup v1 we need to calculate it ourselves.
                        // But only with Systemd version >= 247.
                        // And on Ubuntu-22.04 we have cgroup v2, Systemd version 249, but no MemoryPeak property.
                        // @todo How to handle this?
                        if (0 && ocs::uti::Systemd::get_cgroup_version() == 2 && ocs::uti::Systemd::get_systemd_version() >= 247) {
                           // cgroup v2
                           ptf_get_usage_value_from_systemd(systemd, scope, usage_list, "MemoryPeak", USAGE_ATTR_MAXRSS, 1.0);
                           // IO usage
                           // @todo I don't really find information about it.
                           //       IOReadBytes and IOWriteBytes are not shown by introspection,
                           //       but exist e.g. on Ubuntu 24.04 according to `systemctl show <scope>`.
                           //       The values delivered seem not to make sense.
                           // ptf_get_usage_value_from_systemd2(systemd, scope, usage_list, "IOReadBytes", "IOWriteBytes", USAGE_ATTR_IO, 1.0);
                        } else {
                           // cgroup v1 or too old systemd version
                           lListElem *usage_elem = lGetElemStrRW(usage_list, UA_name, USAGE_ATTR_RSS);
                           if (usage_elem != nullptr) {
                              double rss = lGetDouble(usage_elem, UA_value);
                              usage_elem = lGetElemStrRW(usage_list, UA_name, USAGE_ATTR_MAXRSS);
                              if (rss > lGetDouble(usage_elem, UA_value)) {
                                 lSetDouble(usage_elem, UA_value, rss);
                                 DPRINTF("==> Updated MAXRSS for scope '%s': %f", scope.c_str(), rss);
                              }
                           }
                        }
                     } else {
                        DPRINTF("==> Job is not active in systemd scope %s, state: %s", scope.c_str(), state.c_str());
                     }
                  } else {
                     WARNING("Failed to get property '%s' from systemd scope '%s': %s", "ActiveState", scope.c_str(), sge_dstring_get_string(&error_dstr));
                  }
               }
            }
         }

         // get slice and scope for the job/ja_task/pe_task
         // and retrieve the usage information
         //systemd.get_usage_from_systemd(slice, scope, &error_dstr);
      }

      DRETURN_VOID;
   }

#endif
}
