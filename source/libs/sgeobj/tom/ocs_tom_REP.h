#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/REP.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Report: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of REP, Report
 *
 * The enumeration is the type of every field number of REP, so a parameter
 * of this type takes the field numbers of REP only.
 */
enum REP_Field {
   /**
    * @brief Report Type
    *
    * The type of the report, defined in source/libs/sgeobj/sge_report.h, e.g.
    * - NUM_REP_REPORT_LOAD: a load report
    * - NUM_REP_REPORT_JOB: a job report
    * - ...
    */
   REP_type = REP_LOWERBOUND,
   /**
    * @brief Host Name
    *
    * Hostname as it is seen by sender of report.
    */
   REP_host,
   /**
    * @brief Report List
    *
    * A list of report items, depending on the report type, e.g.
    * a list of load values (HL) objects for load reports or
    * a list of job usage values (UA) objects for job reports.
    */
   REP_list,
   /**
    * @brief Software Version
    *
    * Used to report software version (the GDI version) of execd.
    * Only components with the same GDI version can communicate.
    */
   REP_version,
   /**
    * @brief Report Sequence Number
    *
    * Used to recognize old reports sent by execd.
    */
   REP_seqno,
   /**
    * @brief Complete Job Report
    *
    * Only used in job reports (NUM_REP_REPORT_JOB): true if the report list contains
    * the reports of all jobs on the host, false if it contains only the reports
    * which were flushed after job events.
    * A job report of an older sge_execd does not have this attribute.
    */
   REP_complete,
};

namespace ocs::tom {

   /**
    * @brief Report
    *
    * A report from sge_execd to sge_qmaster, e.g. a load report, a job report, ...
    */
   struct REP {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {REP_type, "REP_type", FieldType::Uint32, Flag::None},
         {REP_host, "REP_host", FieldType::Host, Flag::None},
         {REP_list, "REP_list", FieldType::List, Flag::None},
         {REP_version, "REP_version", FieldType::Uint32, Flag::None},
         {REP_seqno, "REP_seqno", FieldType::Uint32, Flag::None},
         {REP_complete, "REP_complete", FieldType::Bool, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of REP belongs to REP */
   template <>
   struct TypeOf<REP_Field> {
      using type = REP;   ///< the object type
   };

} // namespace ocs::tom
