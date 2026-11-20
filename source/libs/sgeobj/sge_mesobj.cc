/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Message lists: why an object is in the state it is in
 *
 * @see sge_mesobj.h
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_mesobj.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_QIM.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "tom/ocs_Element.h"

/**
 * @brief Add a message to an object's message list
 *
 * Queue instances and jobs carry a list of messages explaining their state;
 * this appends one. The list is created if the object holds none yet.
 *
 * @tparam F the list field holding the messages (QIM), e.g. QU_message_list
 * @param this_elem the object to add to; an empty handle changes nothing
 * @param type the message type, so it can be trashed selectively
 * @param message the text; nullptr adds nothing
 * @return true
 */
template <auto F>
bool object_message_add(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> this_elem,
                        const uint32_t type, const char *message) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem && message != nullptr) {
      // "ulong_sublist" is the name the list library gives a list it creates for a uint32_t key
      auto new_elem = this_elem.template get_or_create<F>("ulong_sublist").template emplace_back<QIM_type>(type);

      ocs::tom::set<QIM_message>(new_elem, message);
   }
   DRETURN(ret);
}

template bool object_message_add<QU_message_list>(ocs::tom::Element<ocs::tom::QU> this_elem, uint32_t type,
                                                  const char *message);
template bool object_message_add<JAT_message_list>(ocs::tom::Element<ocs::tom::JAT> this_elem, uint32_t type,
                                                   const char *message);

/**
 * @brief Remove every message of the given types from an object
 *
 * A message list emptied this way is freed and the field set to nullptr.
 *
 * @tparam F the list field holding the messages (QIM), e.g. QU_message_list
 * @param this_elem the object to clean up; an empty handle changes nothing
 * @param type a mask of the message types to remove
 * @return true
 */
template <auto F>
bool object_message_trash_all_of_type_X(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> this_elem,
                                        const uint32_t type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem) {
      const auto qim_list = ocs::tom::get<F>(this_elem);
      auto next_elem = qim_list.front();

      while (next_elem) {
         const auto elem = next_elem;
         next_elem = elem.next();
         if ((ocs::tom::get<QIM_type>(elem) & type) != 0) {
            qim_list.erase(elem);
         }
      }
      if (qim_list.empty()) {
         ocs::tom::set<F>(this_elem, ocs::tom::ListPtr<ocs::tom::QIM>());
      }
   }
   DRETURN(ret);
}

template bool object_message_trash_all_of_type_X<QU_message_list>(ocs::tom::Element<ocs::tom::QU> this_elem,
                                                                  uint32_t type);
template bool object_message_trash_all_of_type_X<JAT_message_list>(ocs::tom::Element<ocs::tom::JAT> this_elem,
                                                                   uint32_t type);
