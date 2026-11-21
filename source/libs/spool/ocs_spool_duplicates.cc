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
 * @brief Objects found twice while a list is read from the spool (N-H-18)
 */

#include <cstdint>
#include <utility>

#include "uti/sge_hostname.h"
#include "uti/sge_string.h"

#include "spool/ocs_spool_duplicates.h"

namespace {
   /// every duplicate object recorded since the last clear; the spool is read by one thread at a time
   std::vector<ocs::spool::DuplicateObject> recorded;

   /**
    * @brief The key of a unique attribute in the form the hash table of the list compares
    * @param ep the object
    * @param nm the attribute
    * @param[out] text the key as the object holds it, for messages
    * @return the comparable key; empty if the object holds no key in that attribute, or does not hold the
    *         attribute, or the attribute is not of a key type
    * @throw ocs::tom::FieldException with reason NullElement for an empty handle (where the old code crashed)
    */
   std::string
   comparable_key(const ocs::tom::ConstRawElement ep, const int nm, std::string &text) {
      if (!ep) {
         // where lGetType() dereferenced the null element
         ocs::tom::throw_field_exception(ocs::tom::FieldException::Reason::NullElement, nm, nullptr);
      }
      // a foreign attribute or one a reduced object leaves out has no key: lGetType() gave lEndT for both
      if (!ep.has(nm)) {
         text.clear();
         return text;
      }
      const lDescr *descr = ep.descr();
      const int pos = ocs::tom::field_pos(descr, nm);
      switch (ocs::tom::field_type_of(descr[pos].mt)) {
         case ocs::tom::FieldType::String: {
            const char *value = ep.get<const char *>(nm);
            text = value != nullptr ? value : "";
            return text;
         }
         case ocs::tom::FieldType::Host: {
            const char *value = ep.get<ocs::tom::Host>(nm).c_str();
            if (value == nullptr) {
               text.clear();
               return text;
            }
            text = value;
            char host_key[CL_MAXHOSTNAMELEN + 1];
            sge_hostcpy(host_key, value);
            sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
            return host_key;
         }
         case ocs::tom::FieldType::Uint32:
            text = std::to_string(ep.get<std::uint32_t>(nm));
            return text;
         case ocs::tom::FieldType::Uint64:
            text = std::to_string(ep.get<std::uint64_t>(nm));
            return text;
         default:
            text.clear();
            return text;
      }
   }
} // namespace

namespace ocs::spool {
   /**
    * @brief Create the filter for one list read from the spool
    * @param descr the descriptor of the objects read
    * @param type the object type, for the message
    */
   DuplicateFilter::DuplicateFilter(const lDescr *descr, std::string type) : type_(std::move(type)) {
      for (const auto f : ocs::tom::present_fields(descr)) {
         if (mt_is_unique(descr[f.pos].mt)) {
            fields_.push_back(descr[f.pos].nm);
         }
      }
      taken_.resize(fields_.size());
   }

   /**
    * @brief Admit an object read from the spool, unless a key of it is already taken
    *
    * An object whose key in a unique attribute an admitted object holds is not admitted; it is recorded as a
    * duplicate with both spool keys, once for the first attribute that repeats. The caller frees it instead of
    * appending it.
    *
    * @param ep the object read, not nullptr
    * @param spool_key where the object was read from, e.g. its file or its database key
    * @return true if the object is admitted, false if it is a duplicate
    * @throw ocs::tom::FieldException with reason NullElement for an empty handle if the type has a unique
    *        attribute (where the old code crashed)
    */
   bool
   DuplicateFilter::admit(const ocs::tom::ConstRawElement ep, const char *spool_key) {
      std::vector<std::string> keys(fields_.size());
      for (size_t i = 0; i < fields_.size(); i++) {
         std::string text;
         keys[i] = comparable_key(ep, fields_[i], text);
         if (keys[i].empty()) {
            continue;
         }
         if (const auto it = taken_[i].find(keys[i]); it != taken_[i].end()) {
            recorded.push_back({type_, ocs::tom::field_name(fields_[i]), text, it->second,
                                spool_key != nullptr ? spool_key : ""});
            return false;
         }
      }
      for (size_t i = 0; i < fields_.size(); i++) {
         if (!keys[i].empty()) {
            taken_[i].emplace(keys[i], spool_key != nullptr ? spool_key : "");
         }
      }
      return true;
   }

   /**
    * @brief The duplicate objects recorded since the last clear
    * @return the duplicates, in the order they were read
    * @note MT-NOTE: not MT safe; the spool is read by one thread
    */
   const std::vector<DuplicateObject> &
   duplicate_objects() {
      return recorded;
   }

   /**
    * @brief Forget the recorded duplicate objects
    * @note MT-NOTE: not MT safe; the spool is read by one thread
    */
   void
   clear_duplicate_objects() {
      recorded.clear();
   }
} // namespace ocs::spool
