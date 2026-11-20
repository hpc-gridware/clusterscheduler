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
 * This code was generated from file source/libs/sgeobj/json/CK.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Checkpoint: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CK, Checkpoint
 *
 * The enumeration is the type of every field number of CK, so a parameter
 * of this type takes the field numbers of CK only.
 */
enum CK_Field {
   /**
    * @brief Name
    *
    * @todo add description
    */
   CK_name = CK_LOWERBOUND,
   /**
    * @brief Interface
    *
    * The type of checkpointing to be used, e.g.
    *  -hibernator
    *  -cpr @todo no longer supported SGI kernel level checkpointing, remove
    *  -cray-ckpt @todo no longer supported Cray kernel level checkpointing, remove
    *  -transparent, using a checkpointing library like Condor
    *  -userdefined, some user defined method
    *  -application-level, same as userdefined, all interface commands except restart are used, @todo verify
    */
   CK_interface,
   /**
    * @brief Checkpoint Command
    *
    * Commandline to be executed to initiate a checkpoint.
    */
   CK_ckpt_command,
   /**
    * @brief Migration Command
    *
    * Commandline to be executed to initiate a migration of a job (from one host to another one).
    */
   CK_migr_command,
   /**
    * @brief Restart Command
    *
    * Commandline to be executed to restart a checkpointed application.
    */
   CK_rest_command,
   /**
    * @brief Checkpoint Directory
    *
    * A directory to which checkpoints shall be written.
    */
   CK_ckpt_dir,
   /**
    * @brief When
    *
    * When a checkpoint shall be written, one or a combination of:
    *  n: no checkpoint is performed
    *  s: when the execution service is shutdown
    *  m: checkpoint every minimum CPU interval (see queue configuration)
    *  x: when the job gets suspended
    *  an interval: in the specified interval
    */
   CK_when,
   /**
    * @brief Signal
    *
    * A Unix signal to be sent to the jobs process(es) in order to initiate a checkpoint.
    */
   CK_signal,
   /**
    * @brief Job Pid
    *
    * @todo not used and can be removed?
    */
   CK_job_pid,
   /**
    * @brief Clean Command
    *
    * Commandline to be executed after checkpointing to clean up.
    */
   CK_clean_command,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   CK_joker,
};

namespace ocs::tom {

   struct VA;

   /**
    * @brief Checkpoint
    *
    * This is the list type to hold the checkpointing object
    * for the interfaces to the various supported checkpointing mechanisms.
    */
   struct CK {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CK_name, "CK_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {CK_interface, "CK_interface", FieldType::String, Flag::Spool},
         {CK_ckpt_command, "CK_ckpt_command", FieldType::String, Flag::Spool},
         {CK_migr_command, "CK_migr_command", FieldType::String, Flag::Spool},
         {CK_rest_command, "CK_rest_command", FieldType::String, Flag::Spool},
         {CK_ckpt_dir, "CK_ckpt_dir", FieldType::String, Flag::Spool},
         {CK_when, "CK_when", FieldType::String, Flag::Spool},
         {CK_signal, "CK_signal", FieldType::String, Flag::Spool},
         {CK_job_pid, "CK_job_pid", FieldType::Uint32, Flag::None},
         {CK_clean_command, "CK_clean_command", FieldType::String, Flag::Spool},
         {CK_joker, "CK_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CK belongs to CK */
   template <>
   struct TypeOf<CK_Field> {
      using type = CK;   ///< the object type
   };

   /** @brief CK_joker holds elements of VA */
   template <>
   struct SubType<CK_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
