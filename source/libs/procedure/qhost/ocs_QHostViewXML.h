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
 * @brief XML rendering of `qhost`
 */

#include <ostream>

#include "ocs_QHostViewBase.h"
#include "ocs_QHostParameter.h"

namespace ocs {
   /** @brief Renders `qhost` output as XML
    *
    * Each `_start` hook opens an element and the matching `_end` closes it, so
    * the nesting of the traversal becomes the nesting of the document.
    *
    * @ingroup libprocedure
    */
   class QHostViewXML : public QHostViewBase {
   public:
      /** @brief Build the XML view
       * @param parameter the call's parameters
       */
      explicit QHostViewXML(const QHostParameter &parameter) : QHostViewBase(parameter) {};
      ~QHostViewXML() override = default;

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
