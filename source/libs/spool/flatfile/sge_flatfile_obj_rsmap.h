#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "cull/cull_list.h"
#include "uti/sge_dstring.h"

int read_CE_stringval_host(lListElem *ep, int nm, const char *buf,
                           lList **alp);

int write_CE_stringval_host(const lListElem *ep, int nm, dstring *buffer,
                            lList **alp);
