/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "sge_answer.h"
#include "msg_sgeobjlib.h"
#include "msg_common.h"

#include "sge_centry_rsmap.h"

bool centry_check_rsmap(lList **answer_list, u_long32 status, const char* attrname)
{
   /* a RSMAP is not allowed */
   answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                           MSG_SGETEXT_UNKNOWN_ATTR_TYPE_S, "RSMAP");
   return false;
}
