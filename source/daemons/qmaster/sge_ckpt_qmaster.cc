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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>
#include <cctype>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_utility.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_utility_qmaster.h"
#include "sge_ckpt_qmaster.h"
#include "evm/sge_event_master.h"
#include "symbols.h"
#include "sge_persistence_qmaster.h"

#include "msg_common.h"
#include "msg_qmaster.h"

/****** qmaster/ckpt/ckpt_mod() ***********************************************
*  NAME
*     ckpt_mod -- add/modify ckpt object in master ckpt list
*
*  SYNOPSIS
*     int ckpt_mod (lList **alpp, lListElem *new_ckpt, lListElem *ckpt, 
*                   int add, char *ruser, char *rhost, gdi_object_t *object,
*                   int sub_command);
*
*  FUNCTION
*     This function will be called from the framework which will
*     add/modify/delete generic gdi objects.
*     The purpose of this function is it to add new ckpt
*     objects or modify existing checkpointing interfaces. 
*
*
*  INPUTS
*     alpp        - reference to an answer list.
*     new_ckpt    - if a new ckpt object will be created by this 
*                   function, than new_ckpt is new uninitialized
*                   CULL object
*                   if this function was called due to a modify request
*                   than new_ckpt will contain the old data
*                   (see add parameter)
*     ckpt        - a reduced ckpt object which contains all
*                   necessary information to create a new object
*                   or modify parts of an existing one
*     add         - 1 if a new element should be added to the master list 
*                   0 to modify an existing object
*     ruser       - username of person who invoked this gdi request
*     rhost       - hostname of the host where someone initiated an gdi call
*     object      - structure of the gdi framework which contains 
*                   additional information to perform the request
*                   (function pointers, names, CULL-types) 
*     sub_command - how should we handle sublist elements
*              SGE_GDI_CHANGE - modify sublist elements
*              SGE_GDI_APPEND - add elements to a sublist
*              SGE_GDI_REMOVE - remove sublist elements
*              SGE_GDI_SET - replace the complete sublist    
*
*  RESULT
*     [alpp] - error messages will be added to this list
*     0 - success
*     STATUS_EUNKNOWN - an error occurred
******************************************************************************/
int
ckpt_mod(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, lList **alpp, lListElem *new_ckpt, lListElem *ckpt, int add, const char *ruser,
         const char *rhost, gdi_object_t *object, int sub_command, monitoring_t *monitor) {
   const char *ckpt_name;

   DENTER(TOP_LAYER);

   /* ---- CK_name */
   if (lGetPosViaElem(ckpt, CK_name, SGE_NO_ABORT) >= 0) {
      if (add) {
         if (attr_mod_str(alpp, ckpt, new_ckpt, CK_name, SGE_ATTR_CKPT_NAME)) {
            goto ERROR;
         }
      }
      ckpt_name = lGetString(new_ckpt, CK_name);
      if (add && verify_str_key(
              alpp, ckpt_name, MAX_VERIFY_STRING, SGE_ATTR_CKPT_NAME, KEY_TABLE) != STATUS_OK) {
         DRETURN(STATUS_EUNKNOWN);
      }
   } else {
      ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(CK_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      goto ERROR;
   }

   /* ---- CK_interface */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_interface, SGE_ATTR_INTERFACE);

   /* ---- CK_ckpt_command */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_ckpt_command, SGE_ATTR_CKPT_COMMAND);

   /* ---- CK_migr_command */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_migr_command, SGE_ATTR_MIGR_COMMAND);

   /* ---- CK_rest_command */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_rest_command,
                SGE_ATTR_RESTART_COMMAND);

   /* ---- CK_ckpt_dir */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_ckpt_dir, SGE_ATTR_CKPT_DIR);

   /* ---- CK_when */
   if (lGetPosViaElem(ckpt, CK_when, SGE_NO_ABORT) >= 0) {
      int new_flags, flags;

      new_flags = sge_parse_checkpoint_attr(lGetString(new_ckpt, CK_when));
      flags = sge_parse_checkpoint_attr(lGetString(ckpt, CK_when));

      if (SGE_GDI_IS_SUBCOMMAND_SET(sub_command, SGE_GDI_APPEND)
          || SGE_GDI_IS_SUBCOMMAND_SET(sub_command, SGE_GDI_CHANGE)) {
         new_flags |= flags;
      } else if (SGE_GDI_IS_SUBCOMMAND_SET(sub_command, SGE_GDI_REMOVE)) {
         new_flags &= (~flags);
      } else {
         new_flags = flags;
      }
      if (is_checkpoint_when_valid(new_flags)) {
         lSetString(new_ckpt, CK_when, get_checkpoint_when(new_flags));
      } else {
         ERROR(MSG_CKPT_INVALIDWHENATTRIBUTE_S, ckpt_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }
   }

   /* ---- CK_signal */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_signal, SGE_ATTR_SIGNAL);

   /* ---- CK_clean_command */
   attr_mod_str(alpp, ckpt, new_ckpt, CK_clean_command, SGE_ATTR_CLEAN_COMMAND);

   /* ---- CK_job_pid */
   attr_mod_ulong(ckpt, new_ckpt, CK_job_pid, "job_pid");

   /* validate ckpt data */
   if (ckpt_validate(new_ckpt, alpp) != STATUS_OK) {
      goto ERROR;
   }

   DRETURN(0);

   ERROR:
DRETURN(STATUS_EUNKNOWN);
}

/****** qmaster/ckpt/ckpt_spool() *********************************************
*
*  NAME
*     ckpt_spool -- spool a ckpt object  
*
*  SYNOPSIS
*     int ckpt_spool(lList **alpp, lListElem *ep, gdi_object_t *object);
*
*  FUNCTION
*     This function will be called from the framework which will
*     add/modify/delete generic gdi objects.
*     After an object was modified/added successfully it
*     is necessary to spool the current state to the filesystem.
*
*
*  INPUTS
*     alpp        - reference to an answer list.
*     ep          - ckpt object which should be spooled
*     object      - structure of the gdi framework which contains 
*                   additional information to perform the request
*                   (function pointers, names, CULL-types) 
*
*  RESULT
*     [alpp] - error messages will be added to this list
*     0 - success
*     STATUS_EEXIST - an error occurred
******************************************************************************/
int ckpt_spool(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, lList **alpp, lListElem *ep, gdi_object_t *object) {
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ep,
                                   lGetString(ep, CK_name), SGE_TYPE_CKPT, true);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, lGetString(ep, CK_name));
   }

   DRETURN(dbret ? 0 : 1);
}

/****** qmaster/ckpt/ckpt_success() *******************************************
*
*  NAME
*     ckpt_success -- does something after a successful modify
*
*  SYNOPSIS
*     int ckpt_success(lListElem *ep; lListElem *old_ep; gdi_object_t *object);
*
*  FUNCTION
*     This function will be called from the framework which will
*     add/modify/delete generic gdi objects.
*     After an object was modified/added and spooled successfully 
*     it is possibly necessary to perform additional tasks.
*     For example it is necessary to send some events to
+     other daemon.
*
*
*  INPUTS
*     ep          - new ckpt object 
*     old_ep      - old ckpt object before modification or
*                   nullptr if a new object was added
*     object      - structure of the gdi framework which contains 
*                   additional information to perform the request
*                   (function pointers, names, CULL-types) 
*
*  RESULT
*     0 - success
******************************************************************************/
int
ckpt_success(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   const char *ckpt_name;

   DENTER(TOP_LAYER);

   ckpt_name = lGetString(ep, CK_name);

   sge_add_event(0, old_ep ? sgeE_CKPT_MOD : sgeE_CKPT_ADD, 0, 0,
                 ckpt_name, nullptr, nullptr, ep, packet->gdi_session);

   DRETURN(0);
}

/****** qmaster/ckpt/sge_del_ckpt() *******************************************
*
*  NAME
*     sge_del_ckpt -- delete ckpt object in master ckpt list
*
*  SYNOPSIS
*     int sge_del_ckpt(lListElem *ep, lList **alpp, char *ruser, char *rhost);
* 
*  FUNCTION
*     This function will be called from the framework which will
*     add/modify/delete generic gdi objects.
*     The purpose of this function is it to delete ckpt objects. 
*
*
*  INPUTS
*     ep          - element which should be deleted
*     alpp        - reference to an answer list.
*     ruser       - username of person who invoked this gdi request
*     rhost       - hostname of the host where someone initiated an gdi call
*
*  RESULT
*     [alpp] - error messages will be added to this list
*     0 - success
*     STATUS_EUNKNOWN - an error occurred
******************************************************************************/
int
sge_del_ckpt(sge_gdi_packet_class_t *packet, sge_gdi_task_class_t *task, lListElem *ep, lList **alpp, char *ruser, char *rhost) {
   lListElem *found;
   int pos;
   const char *ckpt_name;
   lList **lpp = ocs::DataStore::get_master_list_rw(SGE_TYPE_CKPT);

   DENTER(TOP_LAYER);

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no ckpt element, if ep has no CK_name */
   if ((pos = lGetPosViaElem(ep, CK_name, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(CK_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   ckpt_name = lGetPosString(ep, pos);
   if (!ckpt_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
   found = ckpt_list_locate(*lpp, ckpt_name);

   if (!found) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_CKPT, ckpt_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* 
    * Try to find references in other objects
    */
   {
      lList *local_answer_list = nullptr;

      if (ckpt_is_referenced(found, &local_answer_list, *ocs::DataStore::get_master_list(SGE_TYPE_JOB),
                             *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE))) {
         const lListElem *answer = lFirst(local_answer_list);

         ERROR("denied: %s", lGetString(answer, AN_text));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         lFreeList(&local_answer_list);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* remove ckpt file 1st */
   if (!sge_event_spool(alpp, 0, sgeE_CKPT_DEL, 0, 0, ckpt_name, nullptr, nullptr,
                        nullptr, nullptr, nullptr, true, true, packet->gdi_session)) {
      ERROR(MSG_CANTSPOOL_SS, MSG_OBJ_CKPT, ckpt_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EDISK);
   }

   /* now we can remove the element */
   lRemoveElem(*lpp, &found);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, ckpt_name, MSG_OBJ_CKPT);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

const char *
get_checkpoint_when(int bitmask) {
   int i = 0;
   static char when[32];
   DENTER(TOP_LAYER);

   if (is_checkpoint_when_valid(bitmask) && !(bitmask & NO_CHECKPOINT)) {
      if (bitmask & CHECKPOINT_SUSPEND) {
         when[i++] = CHECKPOINT_SUSPEND_SYM;
      }
      if (bitmask & CHECKPOINT_AT_SHUTDOWN) {
         when[i++] = CHECKPOINT_AT_SHUTDOWN_SYM;
      }
      if (bitmask & CHECKPOINT_AT_MINIMUM_INTERVAL) {
         when[i++] = CHECKPOINT_AT_MINIMUM_INTERVAL_SYM;
      }
      if (bitmask & CHECKPOINT_AT_AUTO_RES) {
         when[i++] = CHECKPOINT_AT_AUTO_RES_SYM;
      }
   } else {
      when[i++] = NO_CHECKPOINT_SYM;
   }
   when[i] = '\0';

   DRETURN(when);
}

int
is_checkpoint_when_valid(int bitmask) {
   int ret = 0;
   int mask = 0;

   DENTER(TOP_LAYER);
   mask = CHECKPOINT_SUSPEND | CHECKPOINT_AT_SHUTDOWN
          | CHECKPOINT_AT_MINIMUM_INTERVAL | CHECKPOINT_AT_AUTO_RES;

   if (bitmask == NO_CHECKPOINT
       || ((bitmask & mask) == bitmask)) {
      ret = 1;
   }
   DRETURN(ret);
}


