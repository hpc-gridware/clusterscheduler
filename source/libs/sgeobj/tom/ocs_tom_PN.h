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
 * This code was generated from file source/libs/sgeobj/json/PN.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Path Name: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of PN, Path Name
 *
 * The enumeration is the type of every field number of PN, so a parameter
 * of this type takes the field numbers of PN only.
 */
enum PN_Field {
   /**
    * @brief Path
    *
    * Path to a file or directory.
    */
   PN_path = PN_LOWERBOUND,
   /**
    * @brief Host Name
    *
    * Name of the host where this specification is valid.
    * If it is left empty (nullptr), then the path is valid on all hosts (default).
    */
   PN_host,
   /**
    * @brief File Host
    *
    * @todo for file staging, the host where the file is available?
    */
   PN_file_host,
   /**
    * @brief Do File Staging
    *
    * Do file staging when set to true. @todo status of filestaging?
    */
   PN_file_staging,
};

namespace ocs::tom {

   /**
    * @brief Path Name
    *
    * An object of the PathName type specifies a certain path on a host.
    * It is used for specifying stdin/stdout/stderr paths via submit options -i/-o/-e/-j.
    * For different hosts different paths can be specified in the form [host:]path[,[host:]path...].
    * There is some simple file staging facility, to be evaluated.
    */
   struct PN {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PN_path, "PN_path", FieldType::String, Flag::PrimaryKey | Flag::SpoolSublist},
         {PN_host, "PN_host", FieldType::Host, Flag::None},
         {PN_file_host, "PN_file_host", FieldType::Host, Flag::None},
         {PN_file_staging, "PN_file_staging", FieldType::Bool, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PN belongs to PN */
   template <>
   struct TypeOf<PN_Field> {
      using type = PN;   ///< the object type
   };

} // namespace ocs::tom
