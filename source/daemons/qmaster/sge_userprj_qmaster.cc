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
/*
   This is the module for handling users and projects.
   We save users to <spool>/qmaster/$USER_DIR and
                                    $PROJECT_DIR
 */

#include <cstdio>
#include <cctype>
#include <cstring>
#include <unistd.h>

#include "uti/sge_rmon.h"
#include "uti/sge_time.h"
#include "uti/sge_log.h"
#include "uti/sge_lock.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_resource_quota.h"

#include "sge_utility_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "evm/sge_event_master.h"
#include "sge_resource_quota_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "spool/sge_spooling.h"

#include "msg_common.h"
#include "msg_qmaster.h"


static int
do_add_auto_user(lListElem *, lList **, monitoring_t *monitor);


int
userprj_mod(lList **alpp, lListElem *modp, lListElem *ep, int add, const char *ruser,
            const char *rhost, gdi_object_t *object, int sub_command, monitoring_t *monitor) {
   int user_flag = (object->target == SGE_UU_LIST) ? 1 : 0;
   int pos;
   const char *userprj;
   u_long32 uval;
   lList *lp;
   const char *obj_name;
   int obj_key;
   int obj_oticket;
   int obj_fshare;
   int obj_usage;
   int obj_version;
   int obj_project;
   const lList *master_userset_list = *object_type_get_master_list(SGE_TYPE_USERSET);
   const lList *obj_master_list;

   DENTER(TOP_LAYER);

   if (user_flag == 0) {
      // project
      obj_name = MSG_OBJ_PRJ;
      obj_key = PR_name;
      obj_oticket = PR_oticket;
      obj_fshare = PR_fshare;
      obj_usage = PR_usage;
      obj_version = PR_version;
      obj_project = PR_project;
      obj_master_list = *object_type_get_master_list(SGE_TYPE_PROJECT);
   } else {
      // user
      obj_name = MSG_OBJ_USER;
      obj_key = UU_name;
      obj_oticket = UU_oticket;
      obj_fshare = UU_fshare;
      obj_usage = UU_usage;
      obj_version = UU_version;
      obj_project = UU_project;
      obj_master_list = *object_type_get_master_list(SGE_TYPE_USER);
   }

   /* ---- UP_name */
   userprj = lGetString(ep, obj_key);
   if (add) {
      if (!strcmp(userprj, "default")) {
         ERROR((SGE_EVENT, MSG_UP_NOADDDEFAULT_S, obj_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         goto Error;
      }
      /* may not add user/project with same name as an existing project/user */
      if (lGetElemStr(obj_master_list, obj_key, userprj)) {
         ERROR((SGE_EVENT, MSG_UP_ALREADYEXISTS_SS, obj_name, userprj));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         goto Error;
      }
      if (verify_str_key(alpp, userprj, MAX_VERIFY_STRING, obj_name, KEY_TABLE) != STATUS_OK) {
         goto Error;
      }
      lSetString(modp, obj_key, userprj);
   }

   /* ---- UP_oticket */
   if ((pos = lGetPosViaElem(ep, obj_oticket, SGE_NO_ABORT)) >= 0) {
      uval = lGetPosUlong(ep, pos);
      lSetUlong(modp, obj_oticket, uval);
   }

   /* ---- UP_fshare */
   if ((pos = lGetPosViaElem(ep, obj_fshare, SGE_NO_ABORT)) >= 0) {
      uval = lGetPosUlong(ep, pos);
      lSetUlong(modp, obj_fshare, uval);
   }

   if (user_flag) {
      /* ---- UU_delete_time */
      if ((pos = lGetPosViaElem(ep, UU_delete_time, SGE_NO_ABORT)) >= 0) {
         uval = lGetPosUlong(ep, pos);
         lSetUlong(modp, UU_delete_time, uval);
      }
   }

   /* ---- UP_usage */
   if ((pos = lGetPosViaElem(ep, obj_usage, SGE_NO_ABORT)) >= 0) {
      lp = lGetPosList(ep, pos);
      lSetList(modp, obj_usage, lCopyList("usage", lp));
      lAddUlong(modp, obj_version, 1);
   }

   /* ---- UP_project */
   if ((pos = lGetPosViaElem(ep, obj_project, SGE_NO_ABORT)) >= 0) {
      lp = lGetPosList(ep, pos);
      lSetList(modp, obj_project, lCopyList("project", lp));
      lAddUlong(modp, obj_version, 1);
   }

   if (user_flag) {
      /* ---- UU_default_project */
      if ((pos = lGetPosViaElem(ep, UU_default_project, SGE_NO_ABORT)) >= 0) {
         const char *dproj;

         NULL_OUT_NONE(ep, UU_default_project);
         /* make sure default project exists */
         if ((dproj = lGetPosString(ep, pos))) {
            const lList *master_project_list = *object_type_get_master_list(SGE_TYPE_PROJECT);
            if (master_project_list == nullptr || !prj_list_locate(master_project_list, dproj)) {
               ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_PRJ, dproj));
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               goto Error;
            }
         }

         lSetString(modp, UU_default_project, dproj);
      }
   } else {
      /* ---- PR_acl */
      if ((pos = lGetPosViaElem(ep, PR_acl, SGE_NO_ABORT)) >= 0) {
         lp = lGetPosList(ep, pos);
         lSetList(modp, PR_acl, lCopyList("acl", lp));

         if (userset_list_validate_acl_list(lGetList(ep, PR_acl), alpp, master_userset_list) != STATUS_OK) {
            /* answerlist gets filled by userset_list_validate_acl_list() in case of errors */
            goto Error;
         }
      }

      /* ---- PR_xacl */
      if ((pos = lGetPosViaElem(ep, PR_xacl, SGE_NO_ABORT)) >= 0) {
         lp = lGetPosList(ep, pos);
         lSetList(modp, PR_xacl, lCopyList("xacl", lp));
         if (userset_list_validate_acl_list(lGetList(ep, PR_xacl), alpp, master_userset_list) != STATUS_OK) {
            /* answerlist gets filled by userset_list_validate_acl_list() in case of errors */
            goto Error;
         }
      }

      if (lGetPosViaElem(modp, PR_xacl, SGE_NO_ABORT) >= 0 ||
          lGetPosViaElem(modp, PR_acl, SGE_NO_ABORT) >= 0) {
         if (multiple_occurances(alpp,
                                 lGetList(modp, PR_acl),
                                 lGetList(modp, PR_xacl),
                                 US_name, userprj,
                                 "project")) {
            goto Error;
         }
      }
   }

   DRETURN(0);

   Error:
   DRETURN(STATUS_EUNKNOWN);
}

int
userprj_success(lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   int user_flag = (object->target == SGE_UU_LIST) ? 1 : 0;
   const lListElem *rqs;
   int obj_key, obj_filter, obj_consider;
   ev_event obj_add_event, obj_mod_event;

   DENTER(TOP_LAYER);

   if (user_flag == 0) {
      obj_key = PR_name;
      obj_filter = RQR_filter_projects;
      obj_consider = PR_consider_with_categories;
      obj_add_event = sgeE_PROJECT_ADD;
      obj_mod_event = sgeE_PROJECT_MOD;
   } else {
      obj_key = UU_name;
      obj_filter = RQR_filter_users;
      obj_consider = UU_consider_with_categories;
      obj_add_event = sgeE_USER_ADD;
      obj_mod_event = sgeE_USER_MOD;
   }

   for_each_ep(rqs, *(object_type_get_master_list(SGE_TYPE_RQS))) {
      if (scope_is_referenced_rqs(rqs, obj_filter, lGetString(ep, obj_key))) {
         lSetBool(ep, obj_consider, true);
         break;
      }
   }

   sge_add_event(0, old_ep ? obj_mod_event : obj_add_event, 0, 0, lGetString(ep, obj_key), nullptr, nullptr, ep);
   lListElem_clear_changed_info(ep);

   DRETURN(0);
}

int
userprj_spool(lList **alpp, lListElem *upe, gdi_object_t *object) {
   lList *answer_list = nullptr;
   bool dbret;
   int user_flag = (object->target == SGE_UU_LIST) ? 1 : 0;
   bool job_spooling = bootstrap_get_job_spooling();

   DENTER(TOP_LAYER);

   /* write user or project to file */
   dbret = spool_write_object(alpp, spool_get_default_context(), upe,
                              lGetString(upe, object->key_nm),
                              user_flag ? SGE_TYPE_USER : SGE_TYPE_PROJECT,
                              job_spooling);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S,
                              lGetString(upe, object->key_nm));
   }

   DRETURN(dbret ? 0 : 1);
}


/***********************************************************************
   master code: delete a user or project
 ***********************************************************************/
int
sge_del_userprj(lListElem *up_ep, lList **alpp, lList **upl,
                const char *ruser, const char *rhost, int user /* =1 user, =0 project */ ) {
   const char *name;
   lListElem *ep;
   lList *projects;

   DENTER(TOP_LAYER);

   if (!up_ep || !ruser || !rhost) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, __func__));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (user) {
      name = lGetString(up_ep, UU_name);
      if (!(ep = user_list_locate(*upl, name))) {
         ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_USER, name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   } else {
      name = lGetString(up_ep, PR_name);
      if (!(ep = prj_list_locate(*upl, name))) {
         ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_PRJ, name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   /* ensure this u/p object is not referenced in actual share tree */
   if (getNode(*object_type_get_master_list(SGE_TYPE_SHARETREE), name, user ? STT_USER : STT_PROJECT, 0)) {
      ERROR((SGE_EVENT, MSG_SGETEXT_CANT_DELETE_UP_IN_SHARE_TREE_SS, user ? MSG_OBJ_USER : MSG_OBJ_PRJ, name));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   if (!user) { /* ensure this project is not referenced in any queue */
      const lListElem *cqueue, *host;
      const lListElem *prj;

      /*
       * fix for bug 6422335
       * check the cq configuration for project references instead of qinstances
       */
      for_each_ep(cqueue, *(object_type_get_master_list(SGE_TYPE_CQUEUE))) {
         for_each_ep(prj, lGetList(cqueue, CQ_projects)) {
            if (lGetSubStr(prj, PR_name, name, APRJLIST_value)) {
               ERROR((SGE_EVENT, MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_QUEUE, lGetString(
                       cqueue, CQ_name)));
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EEXIST);
            }
         }
         for_each_ep(prj, lGetList(cqueue, CQ_xprojects)) {
            if (lGetSubStr(prj, PR_name, name, APRJLIST_value)) {
               ERROR((SGE_EVENT, MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_QUEUE, lGetString(
                       cqueue, CQ_name)));
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EEXIST);
            }
         }
      }

      /* check hosts */
      for_each_ep(host, *object_type_get_master_list(SGE_TYPE_EXECHOST)) {
         if (prj_list_locate(lGetList(host, EH_prj), name)) {
            ERROR((SGE_EVENT, MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_EH, lGetHost(host,
                                                                                                                EH_name)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
         if (prj_list_locate(lGetList(host, EH_xprj), name)) {
            ERROR((SGE_EVENT, MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_EH, lGetHost(host,
                                                                                                                 EH_name)));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }

      /* check global configuration */
      projects = mconf_get_projects();
      if (prj_list_locate(projects, name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_CONF, MSG_OBJ_GLOBAL));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         lFreeList(&projects);
         DRETURN(STATUS_EEXIST);
      }
      lFreeList(&projects);
      projects = mconf_get_xprojects();
      if (prj_list_locate(projects, name)) {
         ERROR((SGE_EVENT, MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_CONF, MSG_OBJ_GLOBAL));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         lFreeList(&projects);
         DRETURN(STATUS_EEXIST);
      }
      lFreeList(&projects);

      /* check user list for reference */
      if ((prj = lGetElemStr(*object_type_get_master_list(SGE_TYPE_USER), UU_default_project, name))) {
         ERROR((SGE_EVENT, MSG_USERPRJ_PRJXSTILLREFERENCEDINENTRYX_SS, name, lGetString(prj, UU_name)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   /* delete user or project file */
   if (!sge_event_spool(alpp, 0, user ? sgeE_USER_DEL : sgeE_PROJECT_DEL, 0, 0, name, nullptr, nullptr, nullptr, nullptr, nullptr,
                        true, true)) {

      DRETURN(STATUS_EDISK);
   }

   INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, name, user ? MSG_OBJ_USER : MSG_OBJ_PRJ));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   lRemoveElem(*upl, &ep);

   DRETURN(STATUS_OK);
}

int
verify_project_list(lList **alpp, const lList *name_list, const lList *prj_list, const char *attr_name,
                    const char *obj_descr, const char *obj_name) {
   const lListElem *up;

   DENTER(TOP_LAYER);

   for_each_ep(up, name_list) {
      if (!lGetElemStr(prj_list, PR_name, lGetString(up, PR_name))) {
         ERROR((SGE_EVENT, MSG_SGETEXT_UNKNOWNPROJECT_SSSS, lGetString(up, PR_name), attr_name, obj_descr, obj_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   DRETURN(STATUS_OK);
}

/*-------------------------------------------------------------------------*/
/* sge_automatic_user_cleanup_handler - handles automatically deleting     */
/* automatic user objects which have expired.                         */
/*-------------------------------------------------------------------------*/
void
sge_automatic_user_cleanup_handler(te_event_t anEvent, monitoring_t *monitor) {
   u_long32 auto_user_delete_time = mconf_get_auto_user_delete_time();
   const char *admin = bootstrap_get_admin_user();
   const char *qmaster_host = bootstrap_get_qualified_hostname();

   DENTER(TOP_LAYER);

   /* shall auto users be deleted again? */
   if (auto_user_delete_time > 0) {
      lListElem *user, *next;
      u_long32 now = sge_get_gmt();
      u_long32 next_delete = now + auto_user_delete_time;

      MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);
      lList **master_user_list = object_type_get_master_list_rw(SGE_TYPE_USER);

      /*
       * Check each user for deletion time. We don't use for_each_ep()
       * because we are deleting entries.
       */
      for (user = lFirstRW(*master_user_list); user; user = next) {
         u_long32 delete_time = lGetUlong(user, UU_delete_time);
         next = lNextRW(user);

         /* 
          * For non auto users, delete_time = 0.
          * Never delete them automatically 
          */
         if (delete_time > 0) {
            const char *name = lGetString(user, UU_name);

            /* if the user has jobs, we increment the delete time */
            if (suser_get_job_counter(suser_list_find(*object_type_get_master_list(SGE_TYPE_SUSER), name)) > 0) {
               lSetUlong(user, UU_delete_time, next_delete);
            } else {
               /* if the delete time has expired, delete user */
               if (delete_time <= now) {
                  lList *answer_list = nullptr;
                  if (sge_del_userprj(user, &answer_list, master_user_list, admin, (char *) qmaster_host, 1) !=
                      STATUS_OK) {
                     /* 
                      * if deleting the user failes (due to user being referenced
                      * in other objects, e.g. queue), regard him as non auto user
                      */
                     lSetUlong(user, UU_delete_time, 0);
                  }
                  /* output low level error messages */
                  answer_list_output(&answer_list);
               }
            }
         }
      }
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
   }

   DRETURN_VOID;
}

/*-------------------------------------------------------------------------*/
/* sge_add_auto_user - handles automatically adding GEEE user objects      */
/*    called in sge_gdi_add_job                                            */
/*-------------------------------------------------------------------------*/
int
sge_add_auto_user(const char *user, lList **alpp, monitoring_t *monitor) {
   lListElem *uep;
   int status = STATUS_OK;
   u_long32 auto_user_delete_time = mconf_get_auto_user_delete_time();

   DENTER(TOP_LAYER);

   uep = user_list_locate(*object_type_get_master_list(SGE_TYPE_USER), user);

   /* if the user already exists */
   if (uep != nullptr) {
      /* and is an auto user */
      if (lGetUlong(uep, UU_delete_time) != 0) {
         /* and we shall not keep auto users forever */
         if (auto_user_delete_time > 0) {
            lSetUlong(uep, UU_delete_time, sge_get_gmt() + auto_user_delete_time);
         }
      }
      /* and we are done */
      DRETURN(STATUS_OK);
   }

   /* create a new auto user */
   uep = lCreateElem(UU_Type);
   if (uep == nullptr) {
      DRETURN(STATUS_EMALLOC);
   } else {
      char *auto_user_default_project = nullptr;

      /* set user object attributes */
      lSetString(uep, UU_name, user);

      if (auto_user_delete_time > 0) {
         lSetUlong(uep, UU_delete_time, sge_get_gmt() + auto_user_delete_time);
      } else {
         lSetUlong(uep, UU_delete_time, 0);
      }

      lSetUlong(uep, UU_oticket, mconf_get_auto_user_oticket());
      lSetUlong(uep, UU_fshare, mconf_get_auto_user_fshare());

      auto_user_default_project = mconf_get_auto_user_default_project();
      if (auto_user_default_project == nullptr || strcasecmp(auto_user_default_project, "none") == 0) {
         lSetString(uep, UU_default_project, nullptr);
      } else {
         lSetString(uep, UU_default_project, auto_user_default_project);
      }

      /* add the auto user via GDI request */
      status = do_add_auto_user(uep, alpp, monitor);
      lFreeElem(&uep);
      sge_free(&auto_user_default_project);
   }

   DRETURN(status);
}

/****** qmaster/sge_userprj_qmaster/do_add_auto_user() *************************
*  NAME
*     do_add_auto_user() -- add auto user to SGE_UU_LIST
*
*  SYNOPSIS
*     static int do_add_auto_user(lListElem*, lList**) 
*
*  NOTES
*     MT-NOTE: do_add_auto_user() is not MT safe 
*
*******************************************************************************/
static int do_add_auto_user(lListElem *anUser, lList **anAnswer, monitoring_t *monitor) {
   int res = STATUS_EUNKNOWN;
   gdi_object_t *userList = nullptr;
   lList *tmpAnswer = nullptr;
   lList *ppList = nullptr;
   const char *admin_user = bootstrap_get_admin_user();
   const char *qualified_hostname = bootstrap_get_qualified_hostname();

   DENTER(TOP_LAYER);

   userList = get_gdi_object(SGE_UU_LIST);

   /* 
    * Add anUser to the user list.
    * Owner of the operation is the admin user on the qmaster host.
    */
   res = sge_gdi_add_mod_generic(&tmpAnswer, anUser, 1, userList, admin_user, qualified_hostname, 0, &ppList,
                                 monitor);

   lFreeList(&ppList);
   if ((STATUS_OK != res) && (nullptr != tmpAnswer)) {
      const lListElem *err = lFirst(tmpAnswer);
      const char *text = lGetString(err, AN_text);
      u_long32 status = lGetUlong(err, AN_status);
      answer_quality_t quality = (answer_quality_t) lGetUlong(err, AN_quality);

      answer_list_add(anAnswer, text, status, quality);
   }

   if (tmpAnswer != nullptr) {
      lFreeList(&tmpAnswer);
   }

   DRETURN(res);
}

/****** sge_userprj_qmaster/sge_userprj_spool() ********************************
*  NAME
*     sge_userprj_spool() -- updates the spooled user and projects
*
*  SYNOPSIS
*     void sge_userprj_spool(void) 
*
*  FUNCTION
*     The usage is only stored every 2 min. To have the acual usage stored when
*     the qmaster is going down, we have to through all user/projects and store
*     them again.
*
*  NOTES
*     MT-NOTE: sge_userprj_spool() is not MT safe, because it is working on global
*              master lists (only reading)
*
*******************************************************************************/
void sge_userprj_spool() {
   lListElem *elem = nullptr;
   lList *answer_list = nullptr;
   const char *name = nullptr;
   u_long32 now = sge_get_gmt();

   DENTER(TOP_LAYER);

   /* this function is used on qmaster shutdown, no need to monitor this lock */
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   for_each_rw(elem, *object_type_get_master_list(SGE_TYPE_USER)) {
      name = lGetString(elem, UU_name);
      sge_event_spool(&answer_list, now, sgeE_USER_MOD, 0, 0, name, nullptr, nullptr, elem, nullptr, nullptr, false, true);
   }

   for_each_rw(elem, *object_type_get_master_list(SGE_TYPE_PROJECT)) {
      name = lGetString(elem, PR_name);
      sge_event_spool(&answer_list, now, sgeE_PROJECT_MOD, 0, 0, name, nullptr, nullptr, elem, nullptr, nullptr, false, true);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   answer_list_output(&answer_list);

   DRETURN_VOID;
}

/****** sge_userprj_qmaster/project_still_used() *******************************
*  NAME
*     project_still_used() -- True, if project still used
*
*  SYNOPSIS
*     static bool project_still_used(const char *p)
*
*  FUNCTION
*     Returns true, if project is still used as ACL with host_conf(5),
*     queue_conf(5).
*
*     Use of projects as ACLs in sge_conf(5) play no role here, 
*     since such ACLs are checked in qmaster and thus are not 
*     relevant for the scheduling algorithm.
*
*  INPUTS
*     const char *p - the project
*
*  RESULT
*     static bool - True, if project still used
*
*  NOTES
*     MT-NOTE: project_still_used() is not MT safe
*******************************************************************************/
static bool project_still_used(const char *p) {
   const lListElem *qc, *cq, *hep, *rqs;

   for_each_ep(rqs, *object_type_get_master_list(SGE_TYPE_RQS)) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_projects, p)) {
         return true;
      }
   }

   for_each_ep(hep, *object_type_get_master_list(SGE_TYPE_EXECHOST)) {
      if (lGetSubStr(hep, PR_name, p, EH_prj) || lGetSubStr(hep, PR_name, p, EH_xprj)) {
         return true;
      }
   }

   for_each_ep(cq, *object_type_get_master_list(SGE_TYPE_CQUEUE)) {
      for_each_ep(qc, lGetList(cq, CQ_projects)) {
         if (lGetSubStr(qc, PR_name, p, APRJLIST_value)) {
            return true;
         }
      }
      for_each_ep(qc, lGetList(cq, CQ_xprojects)) {
         if (lGetSubStr(qc, PR_name, p, APRJLIST_value)) {
            return true;
         }
      }
   }

   return false;
}


/****** sge_userprj_qmaster/project_update_categories() ************************
*  NAME
*     project_update_categories() -- Update all projects wrts categories
*
*  SYNOPSIS
*     void project_update_categories(const lList *added, const lList *removed)
*
*  FUNCTION
*     Each added/removed project is verified whether it is used first
*     time/still as ACL for host_conf(5)/queue_conf(5). If so an event
*     is sent.
*
*  INPUTS
*     const lList *added   - List of added project references (PR_Type)
*     const lList *removed - List of removed project references (PR_Type)
*
*  NOTES
*     MT-NOTE: project_update_categories() is not MT safe
*******************************************************************************/
void project_update_categories(const lList *added, const lList *removed) {
   const lListElem *ep;
   const char *p;
   lListElem *prj;

   DENTER(TOP_LAYER);

   for_each_ep(ep, added) {
      p = lGetString(ep, PR_name);
      DPRINTF(("added project: \"%s\"\n", p));
      prj = lGetElemStrRW(*object_type_get_master_list(SGE_TYPE_PROJECT), PR_name, p);
      if (prj && lGetBool(prj, PR_consider_with_categories) == false) {
         lSetBool(prj, PR_consider_with_categories, true);
         sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, p, nullptr, nullptr, prj);
      }
   }

   for_each_ep(ep, removed) {
      p = lGetString(ep, PR_name);
      DPRINTF(("removed project: \"%s\"\n", p));
      prj = lGetElemStrRW(*object_type_get_master_list(SGE_TYPE_PROJECT), PR_name, p);

      if (prj && !project_still_used(p)) {
         lSetBool(prj, PR_consider_with_categories, false);
         sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, p, nullptr, nullptr, prj);
      }
   }

   DRETURN_VOID;
}


