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
 * @brief The exception of a failed field access in the typed object model
 */

#include "tom/ocs_FieldException.h"

#include "tom/cull_name.h"

namespace ocs::tom {

   FieldException::FieldException(const FieldException::Reason reason, const int nm, const std::string &message)
      : LogicException(message), reason_(reason), nm_(nm) {
   }

   FieldException::~FieldException() = default;

   /**
    * @brief The name of an object type, for messages
    *
    * A type has no name of its own in the list library; its fields are named
    * with the type's prefix, so the prefix of the first field names the type.
    *
    * @param descr the descriptor, or nullptr
    * @return the prefix, e.g. "JB", or "unknown"
    */
   static std::string type_name(const lDescr *descr) {
      if (descr == nullptr || descr[0].nm == NoName) {
         return "unknown";
      }
      std::string name = lNm2Str(descr[0].nm);
      const std::string::size_type underscore = name.find('_');
      if (underscore != std::string::npos) {
         name.resize(underscore);
      }
      return name;
   }

   /**
    * @brief The name of a field, for messages
    * @param nm the field number, or NoName
    * @return the name, e.g. "JB_job_number"
    */
   static std::string field_name(const int nm) {
      return nm == NoName ? std::string{"no field"} : std::string{lNm2Str(nm)};
   }

   /**
    * @brief The layout of a descriptor, for messages
    * @param descr the descriptor, or nullptr
    * @return "full", "reduced to <n> of <m> fields" or "unknown"
    */
   static std::string layout_name(const lDescr *descr) {
      if (descr == nullptr) {
         return "unknown";
      }
      int length = 0;
      int present = 0;
      for (; mt_get_type(descr[length].mt) != lEndT; length++) {
         if (descr[length].off != OCS_CULL_ABSENT) {
            present++;
         }
      }
      if (present == length) {
         return "full";
      }
      return "reduced to " + std::to_string(present) + " of " + std::to_string(length) + " fields";
   }

   void throw_field_exception(const FieldException::Reason reason, const int nm, const lDescr *descr, const lDescr *expected) {
      std::string message;
      switch (reason) {
         case FieldException::Reason::NullElement:
            message = "access to field " + field_name(nm) + " of a null element";
            break;
         case FieldException::Reason::EmptyHandle:
            message = "access to field " + field_name(nm) + " through an empty handle";
            break;
         case FieldException::Reason::ForeignField:
            message = "field " + field_name(nm) + " does not belong to an element of type " + type_name(descr);
            break;
         case FieldException::Reason::AbsentField:
            message = "field " + field_name(nm) + " is absent in a reduced element of type " + type_name(descr);
            break;
         case FieldException::Reason::WrongType:
            message = "an element of type " + type_name(descr) + " is taken as type " + type_name(expected);
            if (nm != NoName) {
               message += ", field " + field_name(nm);
            }
            break;
         case FieldException::Reason::WrongSubType:
            message = "field " + field_name(nm) + " holds elements of type " + type_name(descr) + ", declared " +
                      type_name(expected);
            break;
         case FieldException::Reason::OutOfMemory:
            message = "out of memory copying field " + field_name(nm) + " of type " + type_name(descr);
            break;
         case FieldException::Reason::DuplicateKey:
            message = "duplicate key in unique field " + field_name(nm) + " of type " + type_name(descr);
            break;
         case FieldException::Reason::ForeignList:
            message = "an element of type " + type_name(descr) + " is not in the list the operation was called on";
            break;
         case FieldException::Reason::NullList:
            message = "a list operation got a null list";
            break;
         case FieldException::Reason::NullDescriptor:
            message = "an element or a list is to be created without a descriptor";
            break;
         case FieldException::Reason::NullKey:
            message = "an element of type " + type_name(descr) + " is to be added or removed by a null key in field " +
                      field_name(nm);
            break;
         case FieldException::Reason::BoundElement:
            message = "an element of type " + type_name(descr) + " to be added is bound in a list or an object already";
            break;
         case FieldException::Reason::WrongLayout:
            message = "an element of type " + type_name(descr) + " (descriptor " + layout_name(descr) +
                      ") is to be added to a list of another layout (descriptor " + layout_name(expected) + ")";
            break;
      }
      throw FieldException(reason, nm, message);
   }

   void throw_duplicate_key(const int nm, const lDescr *descr, const std::string &key, const char *list_name) {
      throw FieldException(FieldException::Reason::DuplicateKey, nm,
                           "duplicate key \"" + key + "\" in unique field " + field_name(nm) + " of type " +
                           type_name(descr) + " in list \"" + (list_name != nullptr ? list_name : "unnamed") + "\"");
   }

} // namespace ocs::tom
