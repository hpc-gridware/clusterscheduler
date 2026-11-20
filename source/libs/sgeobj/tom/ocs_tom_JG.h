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
 * This code was generated from file source/libs/sgeobj/json/JG.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Granted Destination Identifier: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JG, Job Granted Destination Identifier
 *
 * The enumeration is the type of every field number of JG, so a parameter
 * of this type takes the field numbers of JG only.
 */
enum JG_Field {
   /**
    * @brief Queue Instance Name
    *
    * The queue instance name.
    */
   JG_qname = JG_LOWERBOUND,
   /**
    * @brief Queue Version
    *
    * The qinstance's version. Is used to detect when a qinstance was changed during scheduling.
    */
   JG_qversion,
   /**
    * @brief Qualified Hostname
    *
    * The qualified host name of the queue instance.
    * It is redundand (cached) information also contained in qname.
    */
   JG_qhostname,
   /**
    * @brief Number of Slots
    *
    * The amount of slots the job occupies in the queue instance.
    * Always 1 for sequential jobs / tasks of array jobs, >= 1 for parallel jobs
    */
   JG_slots,
   /**
    * @brief Queue Object
    *
    * The queue instance definition with information required in sge_execd,
    * like limits, the tmp directory, ...
    * It holds a reduction of a queue instance (QU) to those fields, so the field
    * names no sub-type: the structure check of an object compares a sub-object
    * with the full descriptor of its sub-type, which a reduction does not match.
    */
   JG_queue,
   /**
    * @brief Tag for Slave Job Delivery
    *
    * Tag used in the job delivery protocol, is set when a slave host acknowledged receipt of the start order.
    */
   JG_tag_slave_job,
   /**
    * @brief Total Tickets
    *
    * Total amount of tickets assigned to slots.
    */
   JG_ticket,
   /**
    * @brief Override Tickets
    *
    * Override tickets assigned to slots.
    */
   JG_oticket,
   /**
    * @brief Functional Tickets
    *
    * Functional tickets assigned to slots.
    */
   JG_fticket,
   /**
    * @brief Sharetree Tickets
    *
    * Sharetree tickets assigned to slots.
    */
   JG_sticket,
   /**
    * @brief Job Class Override Tickets
    *
    * Job class override tickets.
    */
   JG_jcoticket,
   /**
    * @brief Job Class Functional Tickets
    *
    * Job class functional tickets.
    */
   JG_jcfticket,
   /**
    * @brief Processor Set
    *
    * Processor set the job is supposed to run on (Solaris only?)
    */
   JG_processors,
   /**
    * @brief Binding that should be used
    *
    * One entry for sequential jobs or multiple entries for PE jobs in case of host/task specific binding
    */
   JG_binding_to_use,
   /**
    * @brief Granted RSMAP IDs
    *
    * List of RSMAP IDs granted to this job / specific tasks of a parallel job,
    * which are running on the specific queue instance / host described by this object.
    */
   JG_granted_rsmaps,
};

namespace ocs::tom {

   struct RESL;
   struct ST;

   /**
    * @brief Job Granted Destination Identifier
    *
    * A list of such objects defines to which queue instance(s) a job has been scheduled.
    * For sequential jobs / tasks of an array job there is a single object of JG.
    * In case of tightly integrated parallel jobs there is an object per queue instance.
    * The first object defines where the master task of the job is running.
    */
   struct JG {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JG_qname, "JG_qname", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {JG_qversion, "JG_qversion", FieldType::Uint32, Flag::None},
         {JG_qhostname, "JG_qhostname", FieldType::Host, Flag::Hash | Flag::SpoolSublist},
         {JG_slots, "JG_slots", FieldType::Uint32, Flag::SpoolSublist},
         {JG_queue, "JG_queue", FieldType::Object, Flag::None},
         {JG_tag_slave_job, "JG_tag_slave_job", FieldType::Uint32, Flag::None},
         {JG_ticket, "JG_ticket", FieldType::Double, Flag::None},
         {JG_oticket, "JG_oticket", FieldType::Double, Flag::None},
         {JG_fticket, "JG_fticket", FieldType::Double, Flag::None},
         {JG_sticket, "JG_sticket", FieldType::Double, Flag::None},
         {JG_jcoticket, "JG_jcoticket", FieldType::Double, Flag::None},
         {JG_jcfticket, "JG_jcfticket", FieldType::Double, Flag::None},
         {JG_processors, "JG_processors", FieldType::String, Flag::None},
         {JG_binding_to_use, "JG_binding_to_use", FieldType::List, Flag::SpoolSublist},
         {JG_granted_rsmaps, "JG_granted_rsmaps", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JG belongs to JG */
   template <>
   struct TypeOf<JG_Field> {
      using type = JG;   ///< the object type
   };

   /** @brief JG_binding_to_use holds elements of ST */
   template <>
   struct SubType<JG_binding_to_use> {
      using type = ST;   ///< the sub-type
   };

   /** @brief JG_granted_rsmaps holds elements of RESL */
   template <>
   struct SubType<JG_granted_rsmaps> {
      using type = RESL;   ///< the sub-type
   };

} // namespace ocs::tom
