#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "basis_types.h"
#include "cull_list.h"

void debit_rsmap_consumable(lListElem *jep, lListElem *hep, const lList *centry_list, bool *ok);

void debit_rsmap_consumable_task(lListElem *jep, lListElem *hep, lList *centry_list, bool *ok,
                                 u_long32 jobid, u_long32 taskid);

