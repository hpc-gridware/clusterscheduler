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
 * This code was generated from file source/libs/sgeobj/json/MR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Mail Receiver: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of MR, Mail Receiver
 *
 * The enumeration is the type of every field number of MR, so a parameter
 * of this type takes the field numbers of MR only.
 */
enum MR_Field {
   /**
    * @brief User Name
    *
    * User name of the mail receipient
    */
   MR_user = MR_LOWERBOUND,
   /**
    * @brief Host Name
    *
    * Host / Domain part of a mail receipient.
    */
   MR_host,
};

namespace ocs::tom {

   /**
    * @brief Mail Receiver
    *
    * One object of this type specifies a mail receiver, meaning an email address
    * in the form `user@host`.
    * It is used e.g. when submitting jobs via `qsub -M user[@host][,user[@host],...]`
    * @todo why do we split it into user and host? We could just have a single string holding an email address.
    */
   struct MR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {MR_user, "MR_user", FieldType::String, Flag::PrimaryKey | Flag::SpoolSublist},
         {MR_host, "MR_host", FieldType::Host, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of MR belongs to MR */
   template <>
   struct TypeOf<MR_Field> {
      using type = MR;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of MR for the list library: a synonym for ocs::tom::MR::descriptor */
inline constexpr const lDescr *MR_Type = ocs::tom::MR::descriptor.e;
