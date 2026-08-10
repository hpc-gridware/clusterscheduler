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

/// Debug layer the id list traces are written to
#define ID_LAYER BASIS_LAYER

/* EB: ADOC: add commets */

/**
 * @brief Turn a list of textual ids into an id list
 *
 * @param[out] id_list receives the parsed ids
 * @param[out] answer_list receives the message naming an unparsable entry
 * @param str_list the texts to parse
 * @param transition the `QI_DO_*` action the ids are meant for
 * @param option the `QI_TRANSITION_*` option that goes with it
 * @return true when every entry could be parsed
 */
bool
id_list_build_from_str_list(lList **id_list, 
                            lList **answer_list,
                            const lList *str_list,
                            uint32_t transition,
                            uint32_t option)
{
   DENTER(ID_LAYER);
   bool ret = true;

   if (transition_is_valid_for_qinstance(transition, answer_list) &&
       transition_option_is_valid_for_qinstance(option, answer_list) &&
       str_list_is_valid(str_list, answer_list)) {
      for_each_ep_lv(elem, str_list) {
         const char *string = lGetString(elem, ST_name);
         lListElem *new_id = nullptr;

         /*
          * Try to parse and add jid/taskid
          * or add string (queue pattern) 
          */

         if ((transition & QUEUE_DO_ACTION) == 0) { 
            sge_parse_jobtasks(id_list, &new_id, string, answer_list, false, nullptr);
         }   

         if (new_id == nullptr) {
            new_id = lAddElemStr(id_list, ID_str, string, ID_Type);
         }
         if (new_id != nullptr) {
            lSetUlong(new_id, ID_action, transition);
            lSetUlong(new_id, ID_force, option);
         } else {
            answer_list_add(answer_list, MSG_ID_UNABLETOCREATE, 
                            STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
            lFreeList(id_list);
            break;
         }
         
      }
   }

   DRETURN(ret);
}


