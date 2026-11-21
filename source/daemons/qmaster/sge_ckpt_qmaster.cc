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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Checkpointing environments
 */
#include <cstdio>
#include <cstring>
#include <cctype>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_JB.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_utility_qmaster.h"
#include "sge_ckpt_qmaster.h"
#include "evm/sge_event_master.h"
#include "symbols.h"
#include "sge_persistence_qmaster.h"

#include "msg_common.h"
#include "msg_qmaster.h"

/**
 * @brief Add/modify ckpt object in master ckpt list
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to add new ckpt
 * objects or modify existing checkpointing interfaces.
 *
 * @param alpp reference to an answer list.
 * @param new_ckpt if a new ckpt object will be created by this function, than new_ckpt is new uninitialized
 *        CULL object if this function was called due to a modify request than new_ckpt will contain the old
 *        data (see add parameter)
 * @param ckpt a reduced ckpt object which contains all necessary information to create a new object or modify
 *        parts of an existing one
 * @param add 1 if a new element should be added to the master list 0 to modify an existing object
 * @param ruser username of person who invoked this gdi request
 * @param rhost hostname of the host where someone initiated an gdi call
 * @param object structure of the gdi framework which contains additional information to perform the request
 *        (function pointers, names, CULL-types)
 * @param sub_command how should we handle sublist elements: `SGE_GDI_CHANGE`
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cmd the command being executed
 * @param monitor for monitoring qmaster threads
 *        modifies them, `SGE_GDI_APPEND` adds to the sublist, `SGE_GDI_REMOVE`
 *        removes from it, `SGE_GDI_SET` replaces the whole sublist
 *
 * @return [alpp] - error messages will be added to this list 0 - success STATUS_EUNKNOWN - an error occurred
 */
int
ckpt_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
         const ocs::tom::Element<ocs::tom::CK> new_ckpt, const ocs::tom::ConstElement<ocs::tom::CK> ckpt, int add,
         const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
         ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *ckpt_name;

   /* ---- CK_name */
   if (ckpt.has<CK_name>()) {
      if (add) {
         // a nullptr name is refused by verify_obj_name() below
         new_ckpt.copy_if_present<CK_name>(ckpt);
      }
      ckpt_name = new_ckpt.get<CK_name>();
      if (add && verify_obj_name(
              alpp, ckpt_name, MAX_VERIFY_STRING, SGE_ATTR_CKPT_NAME) != STATUS_OK) {
         DRETURN(STATUS_EUNKNOWN);
      }
   } else {
      ERROR(MSG_SGETEXT_MISSINGCULLFIELD_SS, ocs::tom::FieldTraits<CK_name>::info.name, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      goto ERROR;
   }

   /* ---- CK_interface */
   new_ckpt.copy_if_present<CK_interface>(ckpt);

   /* ---- CK_ckpt_command */
   new_ckpt.copy_if_present<CK_ckpt_command>(ckpt);

   /* ---- CK_migr_command */
   new_ckpt.copy_if_present<CK_migr_command>(ckpt);

   /* ---- CK_rest_command */
   new_ckpt.copy_if_present<CK_rest_command>(ckpt);

   /* ---- CK_ckpt_dir */
   new_ckpt.copy_if_present<CK_ckpt_dir>(ckpt);

   /* ---- CK_when */
   if (ckpt.has<CK_when>()) {
      int new_flags, flags;

      new_flags = sge_parse_checkpoint_attr(new_ckpt.get<CK_when>());
      flags = sge_parse_checkpoint_attr(ckpt.get<CK_when>());

      if ((sub_command & ocs::gdi::SubCommand::APPEND) == ocs::gdi::SubCommand::APPEND ||
          (sub_command & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE) {
         new_flags |= flags;
      } else if ((sub_command & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE) {
         new_flags &= (~flags);
      } else {
         new_flags = flags;
      }
      if (is_checkpoint_when_valid(new_flags)) {
         new_ckpt.set<CK_when>(get_checkpoint_when(new_flags));
      } else {
         ERROR(MSG_CKPT_INVALIDWHENATTRIBUTE_S, ckpt_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }
   }

   /* ---- CK_signal */
   new_ckpt.copy_if_present<CK_signal>(ckpt);

   /* ---- CK_clean_command */
   new_ckpt.copy_if_present<CK_clean_command>(ckpt);

   /* ---- CK_job_pid */
   new_ckpt.copy_if_present<CK_job_pid>(ckpt);

   /* validate ckpt data */
   if (ckpt_validate(new_ckpt, alpp) != STATUS_OK) {
      goto ERROR;
   }

   DRETURN(0);

   ERROR:
DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Spool a ckpt object
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * After an object was modified/added successfully it
 * is necessary to spool the current state to the filesystem.
 *
 * @param alpp reference to an answer list.
 * @param ep ckpt object which should be spooled
 * @param object structure of the gdi framework which contains additional information to perform the request
 *        (function pointers, names, CULL-types)
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return [alpp] - error messages will be added to this list 0 - success STATUS_EEXIST - an error occurred
 */
int ckpt_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
               const ocs::tom::ConstElement<ocs::tom::CK> ep, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ep,
                                   ep.get<CK_name>(), SGE_TYPE_CKPT);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, ep.get<CK_name>());
   }

   DRETURN(dbret ? 0 : 1);
}

/**
 * @brief Does something after a successful modify
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * After an object was modified/added and spooled successfully
 * it is possibly necessary to perform additional tasks.
 * For example it is necessary to send some events to
 * +     other daemon.
 *
 * @param ep new ckpt object
 * @param old_ep old ckpt object before modification or nullptr if a new object was added
 * @param object structure of the gdi framework which contains additional information to perform the request
 *        (function pointers, names, CULL-types)
 * @param packet the client request
 * @param task the GDI task being answered
 * @param post what is left for after the last object of the request, unused
 * @param monitor for monitoring qmaster threads
 *
 * @return success
 */
int
ckpt_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CK> ep,
             const ocs::tom::ConstElement<ocs::tom::CK> old_ep, gdi_object_t *object,
             [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *ckpt_name;

   ckpt_name = ep.get<CK_name>();

   sge_add_event(0, old_ep ? sgeE_CKPT_MOD : sgeE_CKPT_ADD, 0, 0,
                 ckpt_name, nullptr, nullptr, ep, packet->gdi_session);

   DRETURN(0);
}

/**
 * @brief Delete ckpt object in master ckpt list
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to delete ckpt objects.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep element which should be deleted
 * @param alpp reference to an answer list.
 * @param ruser username of person who invoked this gdi request
 * @param rhost hostname of the host where someone initiated an gdi call
 *
 * @return [alpp] - error messages will be added to this list 0 - success STATUS_EUNKNOWN - an error occurred
 */
int
sge_del_ckpt(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::CK> ep,
             ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   const auto ckpt_list = ocs::DataStore::master_list_rw<SGE_TYPE_CKPT>();

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no ckpt element, if ep has no CK_name */
   if (!ep.has<CK_name>()) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, ocs::tom::FieldTraits<CK_name>::info.name, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *ckpt_name = ep.get<CK_name>();
   if (!ckpt_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
   const auto found = ckpt_list.find_first<CK_name>(ckpt_name);

   if (!found) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_CKPT, ckpt_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* 
    * Try to find references in other objects
    */
   {
      ocs::tom::ListPtr<ocs::tom::AN> local_answer_list;

      if (ckpt_is_referenced(found, &local_answer_list,
                             ocs::DataStore::master_list<SGE_TYPE_JOB>(),
                             ocs::DataStore::master_list<SGE_TYPE_CQUEUE>())) {
         const auto answer = local_answer_list->front();

         ERROR("denied: %s", answer.get<AN_text>());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         local_answer_list.reset();
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
   ckpt_list.erase(found);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, ckpt_name, MSG_OBJ_CKPT);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}


/** @brief The `when` string of a checkpointing environment, from its bitmask
 *
 * @param bitmask the checkpoint occasions as a bitmask
 * @return the string, which the caller owns
 */
const char *
get_checkpoint_when(int bitmask) {
   DENTER(TOP_LAYER);

   int i = 0;
   static char when[32];
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

/** @brief Is this combination of checkpoint occasions allowed?
 *
 * @param bitmask the checkpoint occasions as a bitmask
 * @return non-zero when the bitmask is valid
 *
 * @note Also declared in `libs/uti/sge_parse_num_par.h`, which is a layering
 *       oddity: a libuti header declaring a function only qmaster implements.
 *       Left as it is - removing a declaration would be a code change.
 */
int
is_checkpoint_when_valid(int bitmask) {
   DENTER(TOP_LAYER);

   int ret = 0;
   int mask = 0;

   mask = CHECKPOINT_SUSPEND | CHECKPOINT_AT_SHUTDOWN
          | CHECKPOINT_AT_MINIMUM_INTERVAL | CHECKPOINT_AT_AUTO_RES;

   if (bitmask == NO_CHECKPOINT
       || ((bitmask & mask) == bitmask)) {
      ret = 1;
   }
   DRETURN(ret);
}


