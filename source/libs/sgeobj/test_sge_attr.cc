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

#include <cstdio>
#include <cstdlib>

#include "uti/sge_rmon.h"

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_attr.h"

bool check_attr_str_list_find_value(void) 
{
   bool ret = false;
   lList *attr_list = nullptr;
   lList *answer_list = nullptr;
   lList *ambiguous_hostref_list = nullptr;
   lListElem *attr = nullptr;

   {
      lList *hostref_list1 = nullptr;
      lList *hostref_list2 = nullptr;
      lList *hostref_list3 = nullptr;
      lListElem *hgroup1;
      lListElem *hgroup2;
      lListElem *hgroup3;

      hostref_list_add(&hostref_list1, nullptr, "a");
      hostref_list_add(&hostref_list1, nullptr, "b");
      hostref_list_add(&hostref_list1, nullptr, "c");
      hostref_list_add(&hostref_list1, nullptr, "d");
      hostref_list_add(&hostref_list1, nullptr, "e");

      hostref_list_add(&hostref_list2, nullptr, "f");
      hostref_list_add(&hostref_list2, nullptr, "a");
      hostref_list_add(&hostref_list2, nullptr, "b");
      hostref_list_add(&hostref_list2, nullptr, "g");
      hostref_list_add(&hostref_list2, nullptr, "c");

      hostref_list_add(&hostref_list3, nullptr, "f");
      hostref_list_add(&hostref_list3, nullptr, "g");
      hostref_list_add(&hostref_list3, nullptr, "h");
      hostref_list_add(&hostref_list3, nullptr, "i");
      hostref_list_add(&hostref_list3, nullptr, "j");
   
      hgroup1 = hgroup_create(nullptr, "@A", hostref_list1);
      hgroup2 = hgroup_create(nullptr, "@B", hostref_list2);
      hgroup3 = hgroup_create(nullptr, "@C", hostref_list3);
      *object_type_get_master_list(SGE_TYPE_HGROUP) = lCreateList("", HGRP_Type);
      lAppendElem(*object_type_get_master_list(SGE_TYPE_HGROUP), hgroup1);
      lAppendElem(*object_type_get_master_list(SGE_TYPE_HGROUP), hgroup2);
      lAppendElem(*object_type_get_master_list(SGE_TYPE_HGROUP), hgroup3);
   }
   return ret;
}

int main(int argc, char *argv[])
{
   bool failed = false;

   DENTER_MAIN(TOP_LAYER, "execd");

   lInit(nmv);

   if (!failed) {
      failed = check_attr_str_list_find_value();
   }

   return failed ? 1 : 0;
}

