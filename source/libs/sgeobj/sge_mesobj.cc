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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_mesobj.h"
#include "sgeobj/msg_sgeobjlib.h"

static bool
qim_list_add(lList **this_list, u_long32 type, const char *message);

static bool
qim_list_trash_all_of_type_X(lList **this_list, u_long32 type);

static bool
qim_list_add(lList **this_list, u_long32 type, const char *message) 
{
   bool ret = true;

   DENTER(TOP_LAYER);
   if (this_list != nullptr && message != nullptr) {
      lListElem *new_elem = lAddElemUlong(this_list, QIM_type, type, QIM_Type);

      lSetString(new_elem, QIM_message, message);
   }
   DRETURN(ret);
}

static bool
qim_list_trash_all_of_type_X(lList **this_list, u_long32 type) 
{
   bool ret = true;
   
   DENTER(TOP_LAYER);
   if (this_list != nullptr) {
      lListElem *elem = nullptr;
      lListElem *next_elem = nullptr;

      next_elem = lFirstRW(*this_list);
      while ((elem = next_elem) != nullptr) {
         u_long32 elem_type = lGetUlong(elem, QIM_type);

         next_elem = lNextRW(elem);
         if ((elem_type & type) != 0) {
            lRemoveElem(*this_list, &elem);
         }
      }
      if (lGetNumberOfElem(*this_list) == 0) {
         lFreeList(this_list);
      }
   }
   DRETURN(ret);
}

bool
object_message_add(lListElem *this_elem, int name, 
                   u_long32 type, const char *message)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   if (this_elem != nullptr) {
      lList *qim_list = nullptr;

      lXchgList(this_elem, name, &qim_list);
      ret &= qim_list_add(&qim_list, type, message);
      lXchgList(this_elem, name, &qim_list);
   }
   DRETURN(ret);
}

bool
object_message_trash_all_of_type_X(lListElem *this_elem, int name,
                                   u_long32 type)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   if (this_elem != nullptr) {
      lList *qim_list = nullptr;

      lXchgList(this_elem, name, &qim_list);
      ret &= qim_list_trash_all_of_type_X(&qim_list, type);
      lXchgList(this_elem, name, &qim_list);
   }
   DRETURN(ret);
}

