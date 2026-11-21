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
 * This code was generated from file source/libs/sgeobj/json/FCAT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Functional Category: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of FCAT, Functional Category
 *
 * The enumeration is the type of every field number of FCAT, so a parameter
 * of this type takes the field numbers of FCAT only.
 */
enum FCAT_Field {
   /**
    * @brief Job Share
    *
    * All jobs in this functional category have this amount of jobs shares.
    */
   FCAT_job_share = FCAT_LOWERBOUND,
   /**
    * @brief User Share
    *
    * All jobs in this functional category have this amount of user shares.
    */
   FCAT_user_share,
   /**
    * @brief User
    *
    * Pointer to the user structure.
    */
   FCAT_user,
   /**
    * @brief Project Share
    *
    * All jobs in this functional category have this amount of project shares.
    */
   FCAT_project_share,
   /**
    * @brief Project
    *
    * Pointer to the project structure.
    */
   FCAT_project,
   /**
    * @brief Department Share
    *
    * All jobs in this functional category have this amount of department shares.
    */
   FCAT_dept_share,
   /**
    * @brief Department
    *
    * Pointer to the department structure.
    */
   FCAT_dept,
   /**
    * @brief First Job Related Ticket
    *
    * Pointer to the first element of job ticket list.
    */
   FCAT_jobrelated_ticket_first,
   /**
    * @brief Last Job Related Ticket
    *
    * Pointer to the last element in the job ticket list.
    */
   FCAT_jobrelated_ticket_last,
};

namespace ocs::tom {

   /**
    * @brief Functional Category
    *
    * Objects of this type are used to sort the job list in the scheduler.
    * All attributes are references to objects in other lists.
    */
   struct FCAT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {FCAT_job_share, "FCAT_job_share", FieldType::Uint32, Flag::None},
         {FCAT_user_share, "FCAT_user_share", FieldType::Uint32, Flag::None},
         {FCAT_user, "FCAT_user", FieldType::Ref, Flag::NoTransfer},
         {FCAT_project_share, "FCAT_project_share", FieldType::Uint32, Flag::None},
         {FCAT_project, "FCAT_project", FieldType::Ref, Flag::NoTransfer},
         {FCAT_dept_share, "FCAT_dept_share", FieldType::Uint32, Flag::None},
         {FCAT_dept, "FCAT_dept", FieldType::Ref, Flag::NoTransfer},
         {FCAT_jobrelated_ticket_first, "FCAT_jobrelated_ticket_first", FieldType::Ref, Flag::NoTransfer},
         {FCAT_jobrelated_ticket_last, "FCAT_jobrelated_ticket_last", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of FCAT belongs to FCAT */
   template <>
   struct TypeOf<FCAT_Field> {
      using type = FCAT;   ///< the object type
   };

} // namespace ocs::tom
