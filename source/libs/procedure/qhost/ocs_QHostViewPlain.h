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
 * @brief Plain text rendering of `qhost`
 */

#include <ostream>

#include <basis_types.h>

#include "ocs_QHostViewBase.h"
#include "ocs_QHostParameter.h"

namespace ocs {
   /** @brief Renders `qhost` output as the columnar plain text a terminal expects
    *
    * The `_start` and `_end` hooks are largely no-ops - plain text has no
    * brackets - and the `_value` hooks apply the caller's printf format so the
    * columns line up.
    *
    * @ingroup libprocedure
    */
   class QHostViewPlain : public QHostViewBase {
      bool print_host_header = true;   ///< Whether the host table still needs its header line
   public:
      /** @brief Build the plain text view
       * @param parameter the call's parameters
       */
      explicit QHostViewPlain(const QHostParameter &parameter) : QHostViewBase(parameter) {}
      ~QHostViewPlain() override = default;

      void start(std::ostream &os) override;
      void end(std::ostream &os) override;

      void host_start(std::ostream &os, const char *host_name) override;
      void host_end(std::ostream &os) override;
      void host_value(std::ostream &os, const char *format_str, const char *name, const char *value) override;
      void host_value(std::ostream &os, const char *format_str, const char* name, uint64_t value) override;
      void host_value(std::ostream &os, const char *format_str, const char* name, double value) override;

      void queue_start(std::ostream &os, const char *format_str, const char* qname) override;
      void queue_end(std::ostream &os) override;
      void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, const char *value) override;
      void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, uint32_t value) override;

      void job_start(std::ostream &os, const char *format_str, uint32_t jid) override;
      void job_end(std::ostream &os) override;
      void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, const char *value) override;
      void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, uint64_t value, bool as_timestamp) override;
      void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, double value) override;

      void resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details, bool as_string) override;
      void resource_value(std::ostream &os, const char* dominance, const char* name, uint64_t value, const char *details, bool as_string) override;
      void resource_value(std::ostream &os, const char* dominance, const char* name, double value, const char *details, bool as_string) override;
   };
}
