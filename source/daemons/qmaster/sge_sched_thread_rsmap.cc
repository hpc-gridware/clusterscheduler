/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "basis_types.h"
#include "sge.h"
#include "sge_sched_thread_rsmap.h"
#include "sge_rmon.h"
#include "sge_centry.h"
#include "sge_grantedres.h"
#include "sge_ja_task.h"
#include "sge_job.h"
#include "sge_ulong.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_centry.h"
#include "uti/sge_string.h"
#include "uti/sge_rmon.h"
#include "uti/sge_log.h"

#include "sge_sched_thread_rsmap.h"

/****** sge_sched_thread_remap/add_granted_resource_list() *********************
*  NAME
*     get_remap_ids() --  Function of Univa extension RSMAP complex.
*
*  SYNOPSIS
*     void add_granted_resource_list(lListElem *ja_task,
*                                    lListElem *job,
*                                    lList *granted,
*                                    lList *host_list)
*
*  FUNCTION
*     Non-opensource function of Unvia extension RSMAP complex.
*
*  INPUTS
*     lListElem *ja_task - The job array task
*     lListElem *job     - The job
*     lList *granted     - The granted destination identifier list
*     lList *host_list   - The host list
*
*  NOTES
*     MT-NOTE: add_granted_resource_list() is MT safe
*
*******************************************************************************/
void add_granted_resource_list(
            lListElem *ja_task,
            lListElem *job,
            lList *granted,
            lList *host_list) {

   DENTER(TOP_LAYER);

   DRETURN_VOID;
}

