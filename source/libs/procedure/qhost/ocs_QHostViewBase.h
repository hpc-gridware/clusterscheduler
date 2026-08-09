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
 * @brief Base view of `qhost`, and the interface the three output formats implement
 */

#include <cinttypes>

#include "cull/cull.h"

#include "ocs_ProcedureView.h"
#include "ocs_QHostParameter.h"
#include "ocs_QHostModelBase.h"

namespace ocs {
   /** @brief Base view for `qhost`, and the interface the three formats implement
    *
    * The rendering is written once, here, as a walk over the model: for each
    * host its resources, then its queues, then the jobs in them. What the walk
    * *emits* is not written here - it calls the pure virtual `*_start`,
    * `*_value` and `*_end` hooks below, and #QHostViewPlain, #QHostViewXML and
    * #QHostViewJSON turn those events into their own syntax.
    *
    * That is why adding an output format means implementing a set of hooks and
    * touching none of the traversal, and why a change to what `qhost` reports
    * appears in all three formats at once.
    *
    * The hooks come in matched `_start` / `_value` / `_end` triples, one per
    * nesting level: document, host, queue, job. A format that needs no bracket
    * at some level - plain text needs none - implements the `_start` and `_end`
    * as no-ops.
    *
    * @ingroup libprocedure
    */
   class QHostViewBase : public ProcedureView {
      uint32_t full_listing_ = 0;      ///< Which optional columns the user asked for
      bool show_job_header_ = true;    ///< Whether the job section still needs its header line
   protected:
      size_t indent_ = 0;              ///< Current nesting depth, for the formats that indent
   public:
      static void reformat_double_string(char *new_string, size_t result_size, const char *format, const char *old_string);

      virtual void show_host(std::ostream &os, const lListElem *hep, const QHostParameter &parameter, const QHostModelBase &model, QHostViewBase &report_handler);
      virtual void show_host_resources(std::ostream &os, lListElem *host, const QHostParameter &parameter, const QHostModelBase &model, QHostViewBase &report_handler);
      virtual void show_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, int print_jobid, const char *master,
                                 dstring *dyn_task_str, uint32_t full_listing, int slots, int slot, const char *indent, uint32_t group_opt, int slots_per_line,
                                 int queue_name_length, QHostParameter &parameter, QHostModelBase &model, QHostViewBase &report_handler);
      virtual void show_jobs_per_queue(std::ostream &os, lListElem *qep, int print_jobs_of_queue, uint32_t full_listing, const char *indent,
                                       uint32_t group_opt, int queue_name_length, QHostParameter &parameter, QHostModelBase &model, QHostViewBase &report_handler);
      virtual void show_host_queues(std::ostream &os, lListElem *host, QHostParameter &parameter, QHostModelBase &model, QHostViewBase &report_handler);

   public:
      explicit QHostViewBase(const QHostParameter &parameter);

      ~QHostViewBase() override = default;

      /** @name Output hooks
       *
       * The traversal above calls these; the format implements them. They come
       * in `_start` / `_value` / `_end` triples, one per nesting level, and a
       * format that needs no bracket at a level implements its `_start` and
       * `_end` as no-ops.
       *
       * The `_value` hooks are overloaded on the value's type rather than
       * taking pre-formatted text, so that XML and JSON can emit a number as a
       * number while plain text applies `format_str`.
       * @{
       */

      /** @brief Begin the document
       * @param os stream to write to
       */
      virtual void start(std::ostream &os) = 0;

      /** @brief End the document
       * @param os stream to write to
       */
      virtual void end(std::ostream &os) = 0;

      /** @brief Begin one host
       * @param os stream to write to
       * @param host_name the host
       */
      virtual void host_start(std::ostream &os, const char *host_name) = 0;

      /** @brief End the current host
       * @param os stream to write to
       */
      virtual void host_end(std::ostream &os) = 0;

      /** @brief Report a string valued attribute of the current host
       * @param os stream to write to
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void host_value(std::ostream &os, const char *format_str, const char *name, const char *value) = 0;

      /** @brief Report an integer valued attribute of the current host
       * @param os stream to write to
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void host_value(std::ostream &os, const char *format_str, const char* name, uint64_t value) = 0;

      /** @brief Report a floating point attribute of the current host
       * @param os stream to write to
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void host_value(std::ostream &os, const char *format_str, const char* name, double value) = 0;

      /** @brief Begin one queue instance of the current host
       * @param os stream to write to
       * @param format_str printf format for the plain text layout
       * @param qname the queue instance
       */
      virtual void queue_start(std::ostream &os, const char *format_str, const char* qname) = 0;

      /** @brief End the current queue instance
       * @param os stream to write to
       */
      virtual void queue_end(std::ostream &os) = 0;

      /** @brief Report a string valued attribute of a queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void queue_value(std::ostream &os, const char *qname, const char *format_str, const char* name, const char *value) = 0;

      /** @brief Report an integer valued attribute of a queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void queue_value(std::ostream &os, const char* qname, const char *format_str, const char* name, uint32_t value) = 0;

      /** @brief Begin one job
       * @param os stream to write to
       * @param format_str printf format for the plain text layout
       * @param jid the job id
       */
      virtual void job_start(std::ostream &os, const char *format_str, uint32_t jid) = 0;

      /** @brief End the current job
       * @param os stream to write to
       */
      virtual void job_end(std::ostream &os) = 0;

      /** @brief Report a string valued attribute of a job
       * @param os stream to write to
       * @param jid the job id
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, const char *value) = 0;

      /** @brief Report an integer valued attribute of a job
       * @param os stream to write to
       * @param jid the job id
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       * @param as_timestamp whether the value is a time and has to be rendered as one
       */
      virtual void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, uint64_t value, bool as_timestamp) = 0;

      /** @brief Report a floating point attribute of a job
       * @param os stream to write to
       * @param jid the job id
       * @param format_str printf format for the plain text layout
       * @param name the attribute
       * @param value its value
       */
      virtual void job_value(std::ostream &os, uint32_t jid, const char *format_str, const char* name, double value) = 0;

      /** @brief Report a string valued resource of the current host
       * @param os stream to write to
       * @param dominance where the value comes from, e.g. `hl` for a host load value
       * @param name the resource
       * @param value its value
       * @param details the value's origin, printed when the user asked for it
       * @param as_string whether the value is to be reported verbatim rather than reformatted
       */
      virtual void resource_value(std::ostream &os, const char* dominance, const char* name, const char* value, const char *details, bool as_string) = 0;

      /** @brief Report an integer valued resource of the current host
       * @param os stream to write to
       * @param dominance where the value comes from
       * @param name the resource
       * @param value its value
       * @param details the value's origin, printed when the user asked for it
       * @param as_string whether the value is to be reported verbatim rather than reformatted
       */
      virtual void resource_value(std::ostream &os, const char* dominance, const char* name, uint64_t value, const char *details, bool as_string) = 0;

      /** @brief Report a floating point resource of the current host
       * @param os stream to write to
       * @param dominance where the value comes from
       * @param name the resource
       * @param value its value
       * @param details the value's origin, printed when the user asked for it
       * @param as_string whether the value is to be reported verbatim rather than reformatted
       */
      virtual void resource_value(std::ostream &os, const char* dominance, const char* name, double value, const char *details, bool as_string) = 0;
      /** @} */
   };
}
