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
 * @brief Id lists: the objects a request names
 *
 * @see sge_id.h
 */

#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"

/// Debug layer the id list traces are written to
#define ID_LAYER BASIS_LAYER

/**
 * @brief Turn a list of textual ids into an id list
 *
 * Every entry is parsed as job/task id when @p transition is no queue action;
 * an entry that does not parse that way is added as string (queue pattern).
 *
 * @param[in,out] id_list receives the parsed ids, created when the handle is empty; freed and left empty when an
 *        entry could not be added
 * @param[out] answer_list receives the message naming an unparsable entry
 * @param str_list the texts to parse
 * @param transition the `QI_DO_*` action the ids are meant for
 * @param option the `QI_TRANSITION_*` option that goes with it
 * @return true when every entry could be parsed
 */
bool id_list_build_from_str_list(ocs::tom::ListPtr<ocs::tom::ID> &id_list,
                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 const ocs::tom::ConstList<ocs::tom::ST> str_list,
                                 const uint32_t transition,
                                 const uint32_t option) {
   DENTER(ID_LAYER);
   bool ret = true;

   if (transition_is_valid_for_qinstance(transition, answer_list) &&
       transition_option_is_valid_for_qinstance(option, answer_list) &&
       str_list_is_valid(str_list, answer_list)) {
      for (const auto elem : str_list) {
         const char *string = ocs::tom::get<ST_name>(elem);
         ocs::tom::Element<ocs::tom::ID> new_id;

         /*
          * Try to parse and add jid/taskid
          * or add string (queue pattern) 
          */

         if ((transition & QUEUE_DO_ACTION) == 0) { 
            new_id = sge_parse_jobtasks(id_list, string, answer_list, false,
                                        ocs::tom::ConstList<ocs::tom::RN>()).second;
         }   

         if (!new_id) {
            try {
               if (!id_list) {
                  // the name and hashing ocs::tom::emplace_back() gave a list it created
                  id_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
               }
               new_id = id_list->emplace_back<ID_str>(string);
            } catch (const ocs::RuntimeException &) {
               // no memory: no element, as the soft path of ocs::tom::emplace_back() gave none
            }
         }
         if (new_id) {
            ocs::tom::set<ID_action>(new_id, transition);
            ocs::tom::set<ID_force>(new_id, option);
         } else {
            answer_list_add(answer_list, MSG_ID_UNABLETOCREATE, 
                            STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
            id_list.reset();
            break;
         }
         
      }
   }

   DRETURN(ret);
}
