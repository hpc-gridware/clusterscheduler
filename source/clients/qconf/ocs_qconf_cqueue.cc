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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief qconf - the cluster queue switches
 */

#include <cstring>
#include <ostream>

#include "uti/sge_edit.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge.h"

#include "msg_common.h"
#include "msg_clients_common.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_centry.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "ocs_qconf_cqueue.h"
#include "qconf/ocs_QConfObject.h"
#include "ocs_qconf_parse.h"   /* CS-2313a: qconf_opt_format */
#include "msg_qconf.h"

static bool 
cqueue_provide_modify_context(lListElem **this_elem, lList **answer_list,
                              bool ignore_unchanged_message);

/** @brief Send one cluster queue to qmaster
 *
 * The single point where the cluster queue switches reach the master.
 *
 * @param this_elem the cluster queue (`CQ_Type`) to send
 * @param answer_list used to return error messages
 * @param gdi_command `ADD`, `MOD` or `DEL`
 * @param sub_cmd how a list-valued attribute is merged with the one already
 *                stored: replaced wholesale, or added to
 * @return true on success; false with `answer_list` filled otherwise
 */
bool cqueue_add_del_mod_via_gdi(lListElem *this_elem, lList **answer_list, ocs::gdi::Command gdi_command, ocs::gdi::SubCommand sub_cmd) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr) {
      bool do_verify = (gdi_command == ocs::gdi::Command::MOD) || (gdi_command == ocs::gdi::Command::ADD) ? true : false;

      if (do_verify) {
         ret &= cqueue_verify_attributes(this_elem, answer_list,
                                         this_elem, false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
      }
      if (ret) {
         lList *cqueue_list = nullptr;
         lList *gdi_answer_list = nullptr;

         cqueue_list = lCreateList("", CQ_Type);
         lAppendElem(cqueue_list, this_elem);
         gdi_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, gdi_command, sub_cmd, &cqueue_list, nullptr, nullptr);
         answer_list_replace(answer_list, &gdi_answer_list);
         lDechainElem(cqueue_list, this_elem);
         lFreeList(&cqueue_list);
      }
   }
   DRETURN(ret);
}


/** @brief Fetch one cluster queue from qmaster
 *
 * @param answer_list used to return error messages
 * @param name the cluster queue to fetch
 * @return the cluster queue (`CQ_Type`), or `nullptr` with `answer_list` filled
 */
lListElem *
cqueue_get_via_gdi(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   lListElem *ret = nullptr;

   if (name != nullptr) {
      lList *gdi_answer_list = nullptr;
      lEnumeration *what = nullptr;
      lCondition *where = nullptr;
      lList *cqueue_list = nullptr;

      what = lWhat("%T(ALL)", CQ_Type);
      where = lWhere("%T(%I==%s)", CQ_Type, CQ_name, name);
      gdi_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &cqueue_list, where, what);
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (!answer_list_has_error(&gdi_answer_list)) {
         ret = lDechainElem(cqueue_list, lFirstRW(cqueue_list));
      } else {
         answer_list_replace(answer_list, &gdi_answer_list);
      }
      lFreeList(&cqueue_list);
      lFreeList(&gdi_answer_list);

      /*
       * CS-2677: the queue arrives without its host list -- that list is the
       * member list of "@@<queue>". Fetch the group and join the two, so the
       * caller sees the configuration in the shape it has always had (N-X-1).
       * A second request rather than a multi-request: this is the one-object
       * path, and the callers that fetch many use cqueue_hgroup_get_*_via_gdi().
       */
      if (ret != nullptr) {
         dstring name_buffer = DSTRING_INIT;
         const char *group_name = hgroup_queue_group_name(name, &name_buffer);
         lList *hgroup_list = nullptr;

         what = lWhat("%T(ALL)", HGRP_Type);
         // CS-2762: host field, so the host comparison - see sge_qconf_hgroup.cc
         where = lWhere("%T(%I h= %s)", HGRP_Type, HGRP_name, group_name);
         gdi_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &hgroup_list, where, what);
         lFreeWhat(&what);
         lFreeWhere(&where);

         if (!answer_list_has_error(&gdi_answer_list)) {
            cqueue_fill_hostlist(ret, hgroup_list);
         } else {
            answer_list_replace(answer_list, &gdi_answer_list);
         }
         lFreeList(&hgroup_list);
         lFreeList(&gdi_answer_list);
         sge_dstring_free(&name_buffer);
      }
   }

   DRETURN(ret);
}

static bool
cqueue_provide_modify_context(lListElem **this_elem, lList **answer_list,
                              bool ignore_unchanged_message) {
   DENTER(TOP_LAYER);

   bool ret = false;
   int status = 0;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   
   if (this_elem != nullptr && *this_elem) {
      const char *filename = nullptr;
      filename = spool_flatfile_write_object(answer_list, *this_elem,
                                                     false, CQ_fields,
                                                     &qconf_sfi, SP_DEST_TMP,
                                                     qconf_opt_format, filename,
                                                     false);
      
      if (answer_list_output(answer_list)) {
         if (filename != nullptr) {
            unlink(filename);
            sge_free(&filename);
         }
         DRETURN(false);
      }
 
      status = sge_edit(filename, uid, gid);
      if (status >= 0) {
         lListElem *cqueue;

         fields_out[0] = NoName;
         cqueue = spool_flatfile_read_object(answer_list, CQ_Type, nullptr,
                                             CQ_fields, fields_out, false, 
                                             &qconf_sfi, qconf_opt_format, 
                                             nullptr, filename);

         if (answer_list_output(answer_list)) {
            lFreeElem(&cqueue);
         }

         if (cqueue != nullptr) {
            missing_field = spool_get_unprocessed_field(CQ_fields, fields_out,
                                                        answer_list);
         }

         if (missing_field != NoName) {
            lFreeElem(&cqueue);
            answer_list_output(answer_list);
         }

         if (cqueue != nullptr) {
            if (object_has_differences(*this_elem, answer_list, cqueue) ||
                ignore_unchanged_message) {
               lFreeElem(this_elem);
               *this_elem = cqueue; 
               ret = true;
            } else {
               answer_list_add(answer_list, MSG_FILE_NOTCHANGED,
                               STATUS_ERROR1, ANSWER_QUALITY_ERROR);
            }
         } else {
            answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_PARSE_EDITFAILED,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }

      unlink(filename);
      sge_free(&filename);
   } 
   DRETURN(ret);
}

/** @brief Add a cluster queue, using the editor
 *
 * Builds a template, opens `$EDITOR` on it, and sends the result to qmaster.
 *
 * @param answer_list used to return error messages
 * @param name the name of the cluster queue to create
 * @return true on success; false with `answer_list` filled otherwise
 */
bool cqueue_add(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *cqueue = cqueue_create(answer_list, name);

      if (cqueue == nullptr) {
         ret = false;
      }
      if (ret) {
         ret &= cqueue_set_template_attributes(cqueue, answer_list);
      }
      if (ret) {
         ret &= cqueue_provide_modify_context(&cqueue, answer_list, true);
      }
      if (ret) {
         /* CS-2305: upsert - modify the cqueue if it already exists, add it
          * otherwise (consistent with -Aq and the interactive -aprj/-acal). */
         lList *exist_al = nullptr;
         lListElem *existing = cqueue_get_via_gdi(&exist_al, lGetString(cqueue, CQ_name));
         lFreeList(&exist_al);
         ocs::gdi::Command cmd = (existing != nullptr) ? ocs::gdi::Command::MOD : ocs::gdi::Command::ADD;
         lFreeElem(&existing);
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, cmd, ocs::gdi::SubCommand::SET_ALL);
      }

      lFreeElem(&cqueue);
   }  
  
   DRETURN(ret);
}

/** @brief Add a cluster queue from a file, without the editor
 *
 * The non-interactive form: the file must already be complete.
 *
 * @param answer_list used to return error messages
 * @param filename the file holding the cluster queue definition
 * @return true on success; false with `answer_list` filled otherwise
 */
bool cqueue_add_from_file(lList **answer_list, const char *filename) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   if (filename != nullptr) {
      lListElem *cqueue;

      fields_out[0] = NoName;
      cqueue = spool_flatfile_read_object(answer_list, CQ_Type, nullptr,
                                          CQ_fields, fields_out, false, &qconf_sfi,
                                          qconf_opt_format, nullptr, filename);
            
      if (answer_list_output(answer_list)) {
         lFreeElem(&cqueue);
      }

      if (cqueue != nullptr) {
         missing_field = spool_get_unprocessed_field (CQ_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&cqueue);
         answer_list_output(answer_list);
      }

      if (cqueue == nullptr) {
         ret = false;
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, ocs::gdi::Command::ADD, ocs::gdi::SubCommand::SET_ALL);
      } 

      lFreeElem(&cqueue);
   }  
  
   DRETURN(ret);
}

/** @brief Change a cluster queue, using the editor
 *
 * Fetches the current definition, opens `$EDITOR` on it, and sends back what changed.
 *
 * @param answer_list used to return error messages
 * @param name the cluster queue to change
 * @return true on success; false with `answer_list` filled otherwise
 */
bool cqueue_modify(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *cqueue = cqueue_get_via_gdi(answer_list, name);

      if (cqueue == nullptr) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_DOESNOTEXIST_S, name);
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (ret) {
         ret &= cqueue_provide_modify_context(&cqueue, answer_list, false);
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::SET_ALL);
      }
      lFreeElem(&cqueue);
   }

   DRETURN(ret);
}

/** @brief Change a cluster queue from a file, without the editor
 *
 * The non-interactive form of #cqueue_modify.
 *
 * @param answer_list used to return error messages
 * @param filename the file holding the new definition
 * @return true on success; false with `answer_list` filled otherwise
 */
bool cqueue_modify_from_file(lList **answer_list, const char *filename) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   if (filename != nullptr) {
      lListElem *cqueue;

      fields_out[0] = NoName;
      cqueue = spool_flatfile_read_object(answer_list, CQ_Type, nullptr,
                                          CQ_fields, fields_out, false, &qconf_sfi,
                                          qconf_opt_format, nullptr, filename);
            
      if (answer_list_output(answer_list)) {
         lFreeElem(&cqueue);
      }

      if (cqueue != nullptr) {
         missing_field = spool_get_unprocessed_field (CQ_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&cqueue);
         answer_list_output(answer_list);
      }      

      if (cqueue == nullptr) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_FILENOTCORRECT_S, filename);
         answer_list_add(answer_list, SGE_EVENT,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         ret = false;
      }
      if (ret) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::SET_ALL);
      }
      if (cqueue != nullptr) {
         lFreeElem(&cqueue);
      }
   }

   DRETURN(ret);
}

/** @brief Delete a cluster queue
 *
 * @param answer_list used to return error messages
 * @param name the cluster queue to delete
 * @return true on success; false with `answer_list` filled otherwise
 */
bool cqueue_delete(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *cqueue = cqueue_create(answer_list, name); 
   
      if (cqueue != nullptr) {
         ret &= cqueue_add_del_mod_via_gdi(cqueue, answer_list, ocs::gdi::Command::DEL, ocs::gdi::SubCommand::NONE);
      }

      lFreeElem(&cqueue);
   }
   DRETURN(ret);
}

