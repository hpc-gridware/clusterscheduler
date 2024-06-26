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

#include <ctime>

#include "sgeobj/cull/sge_calendar_CAL_L.h"
#include "sgeobj/cull/sge_calendar_CA_L.h"
#include "sgeobj/cull/sge_calendar_CQU_L.h"
#include "sgeobj/cull/sge_calendar_TMR_L.h"
#include "sgeobj/cull/sge_calendar_TM_L.h"

bool 
calendar_parse_year(lListElem *this_elem, lList **answer_list);

bool 
calendar_parse_week(lListElem *this_elem, lList **answer_list);


u_long32 calender_state_changes(const lListElem *cep, lList **state_changes_list, u_long64 *when, u_long64 *now);

bool 
calendar_is_referenced(const lListElem *calendar, lList **answer_list,
                       const lList *master_cqueue_list);
/* */

lListElem* sge_generic_cal(char *cal_name);

void cullify_tm(lListElem *tm_ep, struct tm *tm_now);
void uncullify_tm(const lListElem *tm_ep, struct tm *tm_now);

bool calendar_open_in_time_frame(const lListElem *cep, u_long64 start_time, u_long64 duration);
