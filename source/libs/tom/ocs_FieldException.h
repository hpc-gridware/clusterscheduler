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
 * @brief The exception of a failed field access in the typed object model
 */

#include <cstdint>
#include <string>

#include "cull/cull_list.h"
#include "uti/ocs_Exception.h"

namespace ocs::tom {

   /**
    * @brief A field access that violated its precondition
    *
    * A logic exception, because every case is a programming error: a field
    * read from an element that does not hold it, a pointer of the wrong type
    * turned into a handle. The old interface catches it and needs to know which
    * case it was, to reproduce exactly what it did before (an abort with its old
    * message, or a return value and an error code), so the reason is kept.
    */
   class FieldException : public ocs::LogicException {
   public:
      /** @brief Why a field access failed */
      enum class Reason : std::uint8_t {
         NullElement,   ///< the element is nullptr
         EmptyHandle,   ///< the handle is empty
         ForeignField,  ///< the field does not belong to the element's type
         AbsentField,   ///< the field belongs to the type, but the reduced element does not hold it
         WrongType,     ///< the element is of another type than the handle, or the field of another field type
         WrongSubType,  ///< a list or object field holds elements of another type than the model names
         OutOfMemory    ///< a string could not be copied
      };

      /**
       * @brief Describe a failed field access
       * @param reason why it failed
       * @param nm the field number, or NoName if no field was involved
       * @param message the full message, naming the field and the type
       */
      FieldException(Reason reason, int nm, const std::string &message);

      /** @brief Destroy the exception; out of line, so that its type information exists once */
      ~FieldException() override;

      /**
       * @brief Why the access failed
       * @return the reason
       */
      [[nodiscard]] Reason reason() const noexcept {
         return reason_;
      }

      /**
       * @brief The field the access was for
       * @return the field number, or NoName
       */
      [[nodiscard]] int nm() const noexcept {
         return nm_;
      }

   private:
      Reason reason_;   ///< why the access failed
      int nm_;          ///< the field number, or NoName
   };

   /**
    * @brief Throw a FieldException whose message names the field and the type
    *
    * Out of line and cold, so that an inline accessor holds only a compare and
    * a branch to it.
    *
    * @param reason why the access failed
    * @param nm the field number, or NoName
    * @param descr the descriptor of the element, or nullptr if there is none
    * @param expected the descriptor of the type that was expected, or nullptr
    */
   [[noreturn, gnu::cold]] void throw_field_exception(FieldException::Reason reason, int nm, const lDescr *descr,
                                                      const lDescr *expected = nullptr);

} // namespace ocs::tom
