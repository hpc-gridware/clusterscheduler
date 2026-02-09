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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_userset.h"

#include "sge_pe_schedd.h"
#include "sge_schedd_text.h"
#include "schedd_message.h"
#include "msg_schedd.h"

/** @brief Get the number of slots per host based on the allocation rule of the parallel environment
 *
 * The function checks the allocation rule of the given parallel environment and determines how many slots
 * should be allocated per host based on that rule. The allocation rule can be a specific number,
 * "$pe_slots", "$fill_up", or "$round_robin". If the allocation rule is a number, it checks if the total
 * number of slots can be distributed using that number. If the allocation rule is "$pe_slots", it returns
 * the total number of slots. If the allocation rule is "$fill_up" or "$round_robin", it returns the corresponding
 * constant. If the allocation rule is invalid, it logs an error and returns 0.
 *
 * @param pep   The parallel environment element containing the allocation rule
 * @param slots The total number of slots to be allocated
 * @return The number of slots per host based on the allocation rule, or 0 if the allocation rule is invalid
 *    ALLOC_RULE_FILLUP (-1) if the allocation rule is "$fill_up"
 *    ALLOC_RULE_ROUNDROBIN (-2) if the allocation rule is "$round_robin"
 *    a positive integer if the allocation rule is a valid number
 *    0 if the allocation rule is invalid
 *    1 if the parallel environment is null (indicating a sequential job)
 */
int
pe_allocation_rule_slots(const lListElem *pep, int slots ) {
   DENTER(TOP_LAYER);

   // Sequential job
   if (pep == nullptr) {
      DRETURN(1);
   }

   // Allocation rule is a number
   const char *alloc_rule = lGetString(pep, PE_allocation_rule);
   if (isdigit((int)alloc_rule[0])) {
      const int alloc_rule_value = atoi(alloc_rule);

      // check if the allocation rule is valid
      if (alloc_rule_value == 0) {
         ERROR(MSG_PE_XFAILEDPARSINGALLOCATIONRULEY_SS , lGetString(pep, PE_name), alloc_rule);
         DRETURN(0);
      }
   
      // check if the number of slots can be distributed using the given allocation rule
      if (slots % alloc_rule_value != 0) {
         DPRINTF("pe >%s<: cant distribute %d slots using \"%s\" as alloc rule\n", lGetString(pep, PE_name), slots, alloc_rule);
         DRETURN(0);
      }

      DRETURN(alloc_rule_value);
   }

   if (!strcasecmp(alloc_rule, "$pe_slots")) {
      DRETURN(slots);
   }

   if (!strcasecmp(alloc_rule, "$fill_up")) {
      DRETURN(ALLOC_RULE_FILLUP);
   }
      
   if (!strcasecmp(alloc_rule, "$round_robin")) {
      DRETURN(ALLOC_RULE_ROUNDROBIN);
   }

   ERROR(MSG_PE_XFAILEDPARSINGALLOCATIONRULEY_SS , lGetString(pep, PE_name), alloc_rule);
   DRETURN(0);
}

/****** sge_pe_schedd/pe_restricted() ******************************************
*  NAME
*     pe_match_static() -- Why not job to PE?
*
*  SYNOPSIS
*     int pe_match_static(lListElem *job, lListElem *pe, lList *acl_list, bool 
*     only_static_checks) 
*
*  FUNCTION
*     Checks if PE is suited for the job.
*
*  INPUTS
*     lListElem *job          - ??? 
*     lListElem *pe           - ??? 
*     lList *acl_list         - ??? 
*     bool only_static_checks - ??? 
*
*  RESULT
*     dispatch_t - DISPATCH_OK        ok 
*                  DISPATCH_NEVER_CAT assignment will never be possible for all
*                                     jobs of that category
*
*  NOTES
*     MT-NOTE: pe_restricted() is not MT safe 
*******************************************************************************/
dispatch_t pe_match_static(const sge_assignment_t *a) 
{
   int total_slots;

   DENTER(TOP_LAYER);

   total_slots = (int)lGetUlong(a->pe, PE_slots);
   if (total_slots == 0) {
      /* because there are not enough PE slots in total */
      DPRINTF("total slots %d of PE " SFQ " not in range of job " sge_u32 "\n", total_slots, a->pe_name, a->job_id);
         schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id,
                        SCHEDD_INFO_TOTALPESLOTSNOTINRANGE_S, a->pe_name);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   if (!sge_has_access_(a->user, a->group, a->grp_list,
                        lGetList(a->pe, PE_user_list), lGetList(a->pe, PE_xuser_list), a->acl_list)) {
      DPRINTF("job " sge_u32 " has no access to parallel environment " SFQ "\n", a->job_id, a->pe_name);
      schedd_mes_add(a->monitor_alpp, a->monitor_next_run, a->job_id, SCHEDD_INFO_NOACCESSTOPE_S, a->pe_name);
      DRETURN(DISPATCH_NEVER_CAT);
   }

   DRETURN(DISPATCH_OK);
}
