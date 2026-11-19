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
 * @brief Reading and writing the packed spool file of one object, with the results the old functions gave
 *
 * The classic spooling runs in qmaster threads, where an exception that
 * escapes terminates the daemon, and its callers test a result. The typed
 * forms throw, so these wrappers turn a failure back into the result
 * lReadElemFromDisk() and lWriteElemToDisk() gave; the failure itself is
 * logged by the core, as before.
 */

#include "uti/ocs_Exception.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldException.h"

namespace ocs::spool::classic {

   /**
    * @brief Read an object from its spool file, with the result lReadElemFromDisk() gave
    * @tparam T the object type the file holds
    * @param prefix the directory, or nullptr
    * @param name the file name, or nullptr
    * @param obj_name the kind of object, for the messages logged on a failure
    * @return the object, or an empty handle if no file is named, it cannot be read, does not hold an element or
    *         holds an element of another type
    * @see Old form: lReadElemFromDisk(); typed form: ocs::tom::ElementPtr<T>::read_from_disk()
    */
   template <ocs::tom::ObjectType T>
   [[nodiscard]] ocs::tom::ElementPtr<T>
   ocs_spool_read_elem(const char *prefix, const char *name, const char *obj_name) {
      try {
         return ocs::tom::ElementPtr<T>::read_from_disk(prefix, name, obj_name);
      } catch (const ocs::RuntimeException &) {
         // logged by the core; nullptr, as lReadElemFromDisk() gave
      } catch (const ocs::tom::FieldException &) {
         // an element of another type: refused like an unreadable file
      }
      return {};
   }

   /**
    * @brief Write an object to its spool file, with the result lWriteElemToDisk() gave
    * @tparam T the object type
    * @param element the object
    * @param prefix the directory, or nullptr
    * @param name the file name, or nullptr
    * @param obj_name the kind of object, for the messages logged on a failure
    * @return 0 on success; 1 if the handle is empty, no file is named, the object cannot be packed or the file
    *         cannot be written
    * @see Old form: lWriteElemToDisk(); typed form: ocs::tom::ConstElement<T>::write_to_disk()
    */
   template <ocs::tom::ObjectType T>
   [[nodiscard]] int
   ocs_spool_write_elem(const ocs::tom::ConstElement<T> element, const char *prefix, const char *name,
                        const char *obj_name) {
      try {
         element.write_to_disk(prefix, name, obj_name);
      } catch (const ocs::RuntimeException &) {
         // logged by the core; 1, as lWriteElemToDisk() gave
         return 1;
      } catch (const ocs::tom::FieldException &) {
         // an empty handle, where lWriteElemToDisk() failed on the null element
         return 1;
      }
      return 0;
   }

} // namespace ocs::spool::classic
