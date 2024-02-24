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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"

#include "spool/sge_spooling.h"

#include "sched_conf_qmaster.h"
#include "configuration_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_qmaster.h"
#include "msg_common.h"

static void
check_reprioritize_interval(lList **alpp, char *ruser, char *rhost);


int
sge_read_sched_configuration(const lListElem *aSpoolContext, lList **anAnswer) {
   lList *sched_conf = nullptr;

   DENTER(TOP_LAYER);

   spool_read_list(anAnswer, aSpoolContext, &sched_conf, SGE_TYPE_SCHEDD_CONF);

   if (lGetNumberOfElem(sched_conf) == 0) {
      lListElem *ep = sconf_create_default();

      if (sched_conf == nullptr) {
         sched_conf = lCreateList("schedd_config_list", SC_Type);
      }

      lAppendElem(sched_conf, ep);
      spool_write_object(anAnswer, spool_get_default_context(), ep, "schedd_conf", SGE_TYPE_SCHEDD_CONF, true);
   }

   if (!sconf_set_config(&sched_conf, anAnswer)) {
      lFreeList(&sched_conf);
      DRETURN(-1);
   }

   check_reprioritize_interval(anAnswer, "local", "local");

   DRETURN(0);
}


/************************************************************
  sge_mod_sched_configuration - Master code

  Modify scheduler configuration. We have only one entry in
  Master_Sched_Config_List. So we replace it with the new one.
 ************************************************************/
int
sge_mod_sched_configuration(lListElem *confp, lList **alpp, char *ruser, char *rhost) {
   lList *temp_conf_list = nullptr;

   DENTER(TOP_LAYER);

   if (!confp || !ruser || !rhost) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, __func__));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
   temp_conf_list = lCreateList("sched config", SC_Type);

   lSetUlong(confp, SC_weight_tickets_override,
             sconf_get_weight_tickets_override());

   confp = lCopyElem(confp);
   lAppendElem(temp_conf_list, confp);

   /* just check and log */
   if (!sconf_set_config(&temp_conf_list, alpp)) {
      lFreeList(&temp_conf_list);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (!sge_event_spool(alpp, 0, sgeE_SCHED_CONF,
                        0, 0, "schedd_conf", nullptr, nullptr,
                        confp, nullptr, nullptr, true, true)) {
      answer_list_add(alpp, MSG_SCHEDCONF_CANTCREATESCHEDULERCONFIGURATION, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   check_reprioritize_interval(alpp, ruser, rhost);

   INFO((SGE_EVENT, MSG_SGETEXT_MODIFIEDINLIST_SSSS, ruser, rhost, "scheduler", "scheduler configuration"));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   DRETURN(STATUS_OK);
} /* sge_mod_sched_configuration */


static void
check_reprioritize_interval(lList **alpp, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   if (((sconf_get_reprioritize_interval() == 0) && (mconf_get_reprioritize())) ||
       ((sconf_get_reprioritize_interval() != 0) && (!mconf_get_reprioritize()))) {
      bool flag = (sconf_get_reprioritize_interval() != 0) ? true : false;
      lListElem *conf = sge_get_configuration_for_host(SGE_GLOBAL_NAME);

      sge_set_conf_reprioritize(conf, flag);

      sge_mod_configuration(conf, alpp, ruser, rhost);

      lFreeElem(&conf);
   }

   DRETURN_VOID;
} /* check_reprioritize_interval */

