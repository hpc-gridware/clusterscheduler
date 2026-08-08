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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The scheduler's side of parallel environments
 *
 * A parallel job asks for a PE and a slot range, and the PE's **allocation
 * rule** decides how those slots may be spread over hosts. The rule is a
 * string in the PE configuration; pe_allocation_rule_slots() turns it into
 * either a fixed number of slots per host or one of the two special values
 * below.
 */

#include "sched/sge_select_queue.h"

/**
 * @brief The allocation rules that are not a fixed slot count
 *
 * Negative so they cannot collide with a real number of slots per host, which
 * is what #ALLOC_RULE_IS_FIXED tests for.
 */
enum {
   ALLOC_RULE_FILLUP = -1,      ///< `$fill_up`: fill each host before using the next
   ALLOC_RULE_ROUNDROBIN = -2   ///< `$round_robin`: one slot per host in turn
};

/** Is this allocation rule a fixed number of slots per host? */
#define ALLOC_RULE_IS_FIXED(x) (x>0)

int pe_allocation_rule_slots(const char *allocation_rule, int slots, bool strict_fixed_modulo = true);

dispatch_t pe_match_static(const sge_assignment_t *a);
