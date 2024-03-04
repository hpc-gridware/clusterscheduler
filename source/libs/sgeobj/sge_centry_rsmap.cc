/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "sgeobj/sge_centry_rsmap.h"

bool centry_check_rsmap(lList **answer_list, u_long32 consumable, const char *attrname) {
   bool ret = true;

   if (consumable == CONSUMABLE_NO) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_CENTRY_RSMAP_NOT_CONSUMABLE_S, attrname);
      ret = false;
   }

   return ret;
}
