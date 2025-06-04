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
#include "msg_execd.h"
#include "ptf.h"

// from ptf.cc
extern lList *ptf_jobs;

namespace ocs::execd {
#if defined(OCS_WITH_SYSTEMD)

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

   void
   ptf_get_usage_from_systemd() {
      DENTER(TOP_LAYER);

      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);

      // Initialize the systemd connection and retrieve usage information
      ocs::uti::Systemd systemd;
      if (!systemd.connect(&error_dstr)) {
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
                  if (systemd.sd_bus_get_property("Unit", scope, "ActiveState", state, &error_dstr)) {
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
                        if (ocs::uti::Systemd::get_cgroup_version() == 2) {
                           // cgroup v2
                           ptf_get_usage_value_from_systemd(systemd, scope, usage_list, "MemoryPeak", USAGE_ATTR_MAXRSS, 1.0);
                           // IO usage
                           // @todo I don't really find information about it.
                           //       IOReadBytes and IOWriteBytes are not shown by introspection,
                           //       but exist e.g. on Ubuntu 24.04 according to `systemctl show <scope>`.
                           //       The values delivered seem not to make sense.
                           ptf_get_usage_value_from_systemd2(systemd, scope, usage_list, "IOReadBytes", "IOWriteBytes", USAGE_ATTR_IO, 1.0);
                        } else {
                           // cgroup v1
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
