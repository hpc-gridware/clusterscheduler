#pragma once
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

/** @file
 * @brief The rendering of `qconf -sep` (CS-2820)
 */

#include <cstdlib>
#include <format>

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

#include "sgeobj/cull/sge_host_EH_L.h"
#include "sgeobj/cull/sge_host_HL_L.h"

#include "../msg_qconf.h"

namespace ocs {
   /** @brief `qconf -sep`: the processor table, with a total
    *
    * Not the table sub-procedure of `-sc`, and not a record list either: this
    * one carries a hand written header, two rule lines of fixed length and a
    * SUM row that is an aggregate over the rows rather than one of them. The
    * format strings are carried over character for character, because the
    * widths are the one thing a consumer parses and a characterisation test
    * cannot forgive a change in them.
    *
    * `-fmt json` is deliberately not honoured. The hand written switch never
    * looked at the format, and `object/host/exec_host_processors` pins that; a
    * JSON form is CS-2845, which is also where the question belongs whether the
    * SUM row survives into it.
    *
    * @ingroup libprocedure
    */
   class QConfProcessorView : public QConfViewBase {
   public:
      explicit QConfProcessorView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfProcessorView() override = default;

      /** @brief Not reached: the table is written by show_processors() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      /** @brief Write the processor table of every execution host
       *
       * A value the host has not reported is printed as `-`, which is what makes
       * the row keep its five fields under `-cb` even on a host without topology
       * information - the check relies on that to tell a dropped column from a
       * missing value.
       *
       * @param os      the stream to write to
       * @param list    the execution hosts, already sorted by name
       * @param binding true for `-cb`: also show the socket and core columns
       */
      void show_processors(std::ostream &os, const lList *list, bool binding) {
         uint32_t sum = 0;
         uint32_t socket_sum = 0;
         uint32_t core_sum = 0;

         if (binding) {
            os << std::format("{:<25.24}{:>10.9}{:>6.5}{:>6.5}{:>12.11}\n", MSG_TABLE_HOST,
                              MSG_TABLE_PROCESSORS, MSG_TABLE_SOCKETS, MSG_TABLE_CORES, MSG_TABLE_ARCH);
         } else {
            os << std::format("{:<25.24}{:>10.9}{:>12.11}\n", MSG_TABLE_HOST, MSG_TABLE_PROCESSORS,
                              MSG_TABLE_ARCH);
         }
         write_rule(os, binding);

         for_each_ep_lv(host, list) {
            const lListElem *arch_elem = lGetSubStr(host, HL_name, "arch", EH_load_list);
            const char *hostname = lGetHost(host, EH_name);

            os << std::format("{:<25.24}", (hostname != nullptr) ? hostname : "");
            os << std::format("{:>10}", static_cast<int32_t>(lGetUlong(host, EH_processors)));

            if (binding) {
               socket_sum += write_load_value(os, host, "m_socket");
               core_sum += write_load_value(os, host, "m_core");
            }
            if (arch_elem != nullptr) {
               const char *arch = lGetString(arch_elem, HL_value);
               os << std::format("{:>12.11}", (arch != nullptr) ? arch : "");
            }
            os << "\n";
            sum += lGetUlong(host, EH_processors);
         }

         write_rule(os, binding);
         os << std::format("{:<25.24}{:>10}", MSG_TABLE_SUM_F, static_cast<int32_t>(sum));
         if (binding) {
            write_sum_or_dash(os, socket_sum);
            write_sum_or_dash(os, core_sum);
         }
         os << "\n";
      }

   private:
      /** @brief Write the rule line that frames the rows
       *
       * 47 equal signs, and twelve more under `-cb` - the hand written switch
       * built it from two string literals rather than from the column widths,
       * so the line does not change when a width does.
       *
       * @param os      the stream to write to
       * @param binding true for the wider table of `-cb`
       */
      static void write_rule(std::ostream &os, bool binding) {
         os << "===============================================";
         if (binding) {
            os << "============";
         }
         os << "\n";
      }

      /** @brief Write one load value right aligned, or `-` when the host has none
       * @param os   the stream to write to
       * @param host the execution host
       * @param name the load value to look up
       * @return the value as a number, or 0 when it is missing
       */
      static uint32_t write_load_value(std::ostream &os, const lListElem *host, const char *name) {
         const lListElem *elem = lGetSubStr(host, HL_name, name, EH_load_list);
         if (elem == nullptr) {
            os << std::format("{:>6.5}", "-");
            return 0;
         }
         const char *value = lGetString(elem, HL_value);
         os << std::format("{:>6.5}", (value != nullptr) ? value : "");
         return (value != nullptr) ? static_cast<uint32_t>(atol(value)) : 0;
      }

      /** @brief Write a total, or `-` when nothing contributed to it
       * @param os    the stream to write to
       * @param value the total
       */
      static void write_sum_or_dash(std::ostream &os, uint32_t value) {
         if (value > 0) {
            os << std::format("{:>6}", static_cast<int32_t>(value));
         } else {
            os << std::format("{:>6.5}", "-");
         }
      }
   };
}
