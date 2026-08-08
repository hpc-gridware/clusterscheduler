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
 * @brief The scheduler's side of suspend on subordinate
 */

#include <cstdio>

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"

#include "subordinate_schedd.h"

/**
 * @brief Marks a queue as suspended on subordinate, recursively
 *
 * Raises the queue's suspend-on-subordinate counter and, when it goes from 0
 * to 1, sets the state. Since the queue may itself have subordinates, the
 * function then descends into them, which is why the complete queue list has
 * to be passed.
 *
 * @param[in]     qname name of the queue that needs suspension on subordinate
 * @param[in,out] qlist the complete queue list, needed to descend into the
 *                      subordinates of `qname`
 *
 * @return 0 on success, 1 if the queue is not in `qlist` - which is not an
 *         error: the list may be a subset, and a queue that is not in it is
 *         already suspended
 */
int sos_schedd(const char *qname, lList *qlist) 
{
   lListElem *q;
   uint32_t sos;
   int ret = 0;

   DENTER(TOP_LAYER);

   q = qinstance_list_locate2(qlist, qname);
   if (!q) {
      /* 
         In the qlist we got is only a subset of all
         queues. If the rest of the queues is really
         suspended then this is no error because
         they are already suspended.
      */
      DRETURN(1);
   }

   /* increment sos counter */
   sos = lGetUlong(q, QU_suspended_on_subordinate);
   lSetUlong(q, QU_suspended_on_subordinate, ++sos);

   /* first sos ? */
   if (sos==1) {
      DPRINTF("QUEUE %s GETS SUSPENDED ON SUBORDINATE\n", qname);
      /* state transition */
      qinstance_state_set_susp_on_sub(q, true);
   }

   DRETURN(ret);
}

