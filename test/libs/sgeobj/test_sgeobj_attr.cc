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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/                                   

#include "cstdio"
#include "cstdlib"

#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/oge_DataStore.h"

bool check_attr_str_list_find_value() 
{
   bool ret = false;

   {
      lList *hostref_list1 = nullptr;
      lList *hostref_list2 = nullptr;
      lList *hostref_list3 = nullptr;
      lListElem *hgroup1;
      lListElem *hgroup2;
      lListElem *hgroup3;

      href_list_add(&hostref_list1, nullptr, "a");
      href_list_add(&hostref_list1, nullptr, "b");
      href_list_add(&hostref_list1, nullptr, "c");
      href_list_add(&hostref_list1, nullptr, "d");
      href_list_add(&hostref_list1, nullptr, "e");

      href_list_add(&hostref_list2, nullptr, "f");
      href_list_add(&hostref_list2, nullptr, "a");
      href_list_add(&hostref_list2, nullptr, "b");
      href_list_add(&hostref_list2, nullptr, "g");
      href_list_add(&hostref_list2, nullptr, "c");

      href_list_add(&hostref_list3, nullptr, "f");
      href_list_add(&hostref_list3, nullptr, "g");
      href_list_add(&hostref_list3, nullptr, "h");
      href_list_add(&hostref_list3, nullptr, "i");
      href_list_add(&hostref_list3, nullptr, "j");
   
      hgroup1 = hgroup_create(nullptr, "@A", hostref_list1, true);
      hgroup2 = hgroup_create(nullptr, "@B", hostref_list2, true);
      hgroup3 = hgroup_create(nullptr, "@C", hostref_list3, true);
      *oge::DataStore::get_master_list(SGE_TYPE_HGROUP) = lCreateList("", HGRP_Type);
      lAppendElem(*oge::DataStore::get_master_list_rw(SGE_TYPE_HGROUP), hgroup1);
      lAppendElem(*oge::DataStore::get_master_list_rw(SGE_TYPE_HGROUP), hgroup2);
      lAppendElem(*oge::DataStore::get_master_list_rw(SGE_TYPE_HGROUP), hgroup3);
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

