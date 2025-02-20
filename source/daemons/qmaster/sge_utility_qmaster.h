#pragma once
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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/


#include "cull/cull.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_daemonize.h"

int attr_mod_procedure(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, const char *attr_name, const char *variables[]);

int attr_mod_zerostr(lListElem *qep, lListElem *new_queue, int nm, const char *attr_name);

int attr_mod_str(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, const char *attr_name);

int attr_mod_double(lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_bool(lListElem *qep, lListElem *new_queue, int nm, const char *attr_name);

int attr_mod_ulong(lListElem *qep, lListElem *new_queue, int nm, const char *attr_name);
int attr_mod_ulong64(lListElem *qep, lListElem *new_queue, int nm, const char *attr_name);

int attr_mod_mem_str(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name);

int attr_mod_time_str(lList **alpp, lListElem *qep, lListElem *new_queue, int nm, char *attr_name, int enable_infinity);

bool cqueue_mod_sublist(lListElem *this_elem, lList **answer_list, lListElem *reduced_elem,
                        ocs::gdi::Command::Cmd command, ocs::gdi::SubCommand::SubCmd sub_command,
                        int attribute_name, int sublist_host_name, int sublist_value_name, int subsub_key,
                        const char *attribute_name_str, const char *object_name_str);

int
multiple_occurances(lList **alpp, const lList *lp1, const lList *lp2, int nm, const char *name, const char *obj_name);

void normalize_sublist(lListElem *ep, int nm);

bool attr_mod_sub_list(lList **alpp, lListElem *this_elem, int this_elem_name, int this_elem_primary_key,
                       const lListElem *delta_elem, ocs::gdi::Command::Cmd, ocs::gdi::SubCommand::SubCmd sub_command,
                       const char *sub_list_name, const char *object_name, int no_info, bool *changed);
