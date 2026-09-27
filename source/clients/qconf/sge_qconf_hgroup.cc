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
 * @brief qconf - the host group switches
 */

#include "uti/ocs_Pattern.h"
#include "uti/sge_edit.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_href.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/ocs_spool_json.h"

#include "gdi/ocs_gdi_Client.h"

#include "sge_qconf_hgroup.h"
#include "ocs_qconf_parse.h"   /* CS-2313a: qconf_opt_format */
#include <ostream>

#include "msg_common.h"
#include "msg_qconf.h"
/* MSG_HGROUP_NOTEXIST_S lives in the procedure's own message header since
 * CS-2816. The path is spelled out because a bare "msg_qconf.h" from a file in
 * clients/qconf finds the client's header first, and the two are different. */
#include "procedure/qconf/msg_qconf.h"


static bool
hgroup_provide_modify_context(lListElem **this_elem, lList **answer_list, bool ignore_unchanged_message);



/** @brief Send one host group to qmaster
 *
 * The single point where the host group switches reach the master.
 *
 * @param this_elem the host group (`HGRP_Type`) to send
 * @param answer_list used to return error messages
 * @param gdi_command `ADD`, `MOD` or `DEL`
 * @return true on success; false with `answer_list` filled otherwise
 */
bool hgroup_add_del_mod_via_gdi(lListElem *this_elem, lList **answer_list, ocs::gdi::Command gdi_command) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_elem != nullptr) {
      lListElem *element = nullptr;
      lList *hgroup_list = nullptr;
      lList *gdi_answer_list = nullptr;

      element = lCopyElem(this_elem);
      hgroup_list = lCreateList("", HGRP_Type);
      lAppendElem(hgroup_list, element);
      gdi_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, gdi_command, ocs::gdi::SubCommand::NONE ,&hgroup_list, nullptr, nullptr);
      answer_list_replace(answer_list, &gdi_answer_list);
      lFreeList(&hgroup_list);
   }
   DRETURN(ret);
}

/** @brief Fetch one host group from qmaster
 *
 * @param answer_list used to return error messages
 * @param name the host group to fetch
 * @return the host group (`HGRP_Type`), or `nullptr` with `answer_list` filled
 */
lListElem *
hgroup_get_via_gdi(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   lListElem *ret = nullptr;

   if (name != nullptr) {
      lList *gdi_answer_list = nullptr;
      lEnumeration *what = nullptr;
      lCondition *where = nullptr;
      lList *hostgroup_list = nullptr;

      what = lWhat("%T(ALL)", HGRP_Type);
      /*
       * CS-2762: "h=", not "==" with %s. HGRP_name is an lHostT field, and the
       * object layer folds its case - a group created as "@MixedCase" is found
       * and modified by writing "@MIXEDCASE". A string comparison here made the
       * lookup the one place that disagreed, so a group could be created in one
       * spelling and displayed through none but that one.
       */
      where = lWhere("%T(%I h= %s)", HGRP_Type, HGRP_name, name);
      gdi_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &hostgroup_list, where, what);
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (!answer_list_has_error(&gdi_answer_list)) {
         ret = lDechainElem(hostgroup_list, lFirstRW(hostgroup_list));
      } else {
         answer_list_replace(answer_list, &gdi_answer_list);
      }
      lFreeList(&hostgroup_list);
      lFreeList(&gdi_answer_list);
   } 
   DRETURN(ret);
}

static bool
hgroup_provide_modify_context(lListElem **this_elem, lList **answer_list, bool ignore_unchanged_message) {
   DENTER(TOP_LAYER);

   bool ret = false;
   int status = 0;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   
   if (this_elem != nullptr && *this_elem != nullptr) {
      const char *filename = nullptr;
      filename = spool_flatfile_write_object(answer_list, *this_elem, false, HGRP_fields, &qconf_sfi, SP_DEST_TMP, qconf_opt_format, filename, false);
      if (answer_list_has_error(answer_list)) {
         if (filename != nullptr) {
            unlink(filename);
            sge_free(&filename);
         }
         DRETURN(false);
      }
      
      status = sge_edit(filename, uid, gid);
      
      if (status >= 0) {
         lListElem *hgroup = nullptr;

         fields_out[0] = NoName;
         hgroup = spool_flatfile_read_object(answer_list, HGRP_Type, nullptr, HGRP_fields, fields_out, true, &qconf_sfi, qconf_opt_format, nullptr, filename);
            
         if (answer_list_output (answer_list)) {
            lFreeElem(&hgroup);
         }

         if (hgroup != nullptr) {
            missing_field = spool_get_unprocessed_field(HGRP_fields, fields_out, answer_list);
         }

         if (missing_field != NoName) {
            lFreeElem(&hgroup);
            answer_list_output (answer_list);
         }      

         if (hgroup != nullptr) {
            if (object_has_differences(*this_elem, answer_list, hgroup) ||
                ignore_unchanged_message) {
               lFreeElem(this_elem);
               *this_elem = hgroup;
               ret = true;
            } else {
               lFreeElem(&hgroup);
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

/**
 * @brief Creates a default hgroup object
 *
 * To create a new hgrp, qconf needs a default object, that can be edited.
 *
 * @param answer_list any errors?
 * @param name name of the hgrp
 * @param is_name_validate should the name be validated? false, if one generates a template
 *
 * @return true, if everything went fine
 *
 * @note MT-NOTE: hgroup_add() is MT safe
 */
bool
hgroup_add(lList **answer_list, const char *name, bool is_name_validate ) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *hgroup = hgroup_create(answer_list, name, nullptr, is_name_validate);

      if (hgroup == nullptr) {
         ret = false;
      }
      if (ret) {
         ret = hgroup_provide_modify_context(&hgroup, answer_list, true);
      }
      if (ret) {
         /* CS-2306: upsert - modify the host group if it already exists, add it
          * otherwise (consistent with -Ahgrp and the interactive -aprj/-acal). */
         lList *exist_al = nullptr;
         lListElem *existing = hgroup_get_via_gdi(&exist_al, lGetHost(hgroup, HGRP_name));
         lFreeList(&exist_al);
         ocs::gdi::Command cmd = (existing != nullptr) ? ocs::gdi::Command::MOD : ocs::gdi::Command::ADD;
         lFreeElem(&existing);
         ret = hgroup_add_del_mod_via_gdi(hgroup, answer_list, cmd);
      }

      lFreeElem(&hgroup);
   }
  
   DRETURN(ret); 
}

/** @brief Add a host group from a file, without the editor
 *
 * The non-interactive form: the file must already be complete.
 *
 * @param answer_list used to return error messages
 * @param filename the file holding the host group definition
 * @return true on success; false with `answer_list` filled otherwise
 */
bool
hgroup_add_from_file(lList **answer_list, const char *filename) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   if (filename != nullptr) {
      lListElem *hgroup;

      fields_out[0] = NoName;
      hgroup = spool_flatfile_read_object(answer_list, HGRP_Type, nullptr,
                                      HGRP_fields, fields_out, true, &qconf_sfi,
                                      qconf_opt_format, nullptr, filename);

      if (answer_list_output (answer_list)) {
         lFreeElem(&hgroup);
      }

      if (hgroup != nullptr) {
         missing_field = spool_get_unprocessed_field (HGRP_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&hgroup);
         answer_list_output (answer_list);
      }

      if (hgroup == nullptr) {
         ret = false;
      }
      if (ret) {
         ret = hgroup_add_del_mod_via_gdi(hgroup, answer_list, ocs::gdi::Command::ADD);
      }
      lFreeElem(&hgroup);
   }

   DRETURN(ret);
}

/** @brief Change a host group, using the editor
 *
 * Fetches the current definition, opens `$EDITOR` on it, and sends back what changed.
 *
 * @param answer_list used to return error messages
 * @param name the host group to change
 * @return true on success; false with `answer_list` filled otherwise
 */
bool hgroup_modify(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *hgroup = hgroup_get_via_gdi(answer_list, name);

      if (hgroup == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1,
                                 ANSWER_QUALITY_ERROR, MSG_HGROUP_NOTEXIST_S, name);
         ret = false;
      }
      if (ret) {
         ret = hgroup_provide_modify_context(&hgroup, answer_list, false);
      }
      if (ret) {
         ret = hgroup_add_del_mod_via_gdi(hgroup, answer_list, ocs::gdi::Command::MOD);
      }
      lFreeElem(&hgroup);
   }

   DRETURN(ret);
}

/** @brief Change a host group from a file, without the editor
 *
 * The non-interactive form of #hgroup_modify.
 *
 * @param answer_list used to return error messages
 * @param filename the file holding the new definition
 * @return true on success; false with `answer_list` filled otherwise
 */
bool hgroup_modify_from_file(lList **answer_list, const char *filename) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   if (filename != nullptr) {
      lListElem *hgroup;

      fields_out[0] = NoName;
      hgroup = spool_flatfile_read_object(answer_list, HGRP_Type, nullptr,
                                      HGRP_fields, fields_out, true, &qconf_sfi,
                                      qconf_opt_format, nullptr, filename);
            
      if (answer_list_output(answer_list)) {
         lFreeElem(&hgroup);
      }

      if (hgroup != nullptr) {
         missing_field = spool_get_unprocessed_field(HGRP_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&hgroup);
         answer_list_output (answer_list);
      }      

      if (hgroup == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1,
                                 ANSWER_QUALITY_ERROR, MSG_HGROUP_FILEINCORRECT_S, filename);
         ret = false;
      }
      if (ret) {
         ret = hgroup_add_del_mod_via_gdi(hgroup, answer_list, ocs::gdi::Command::MOD);
      }
      if (hgroup) {
         lFreeElem(&hgroup);
      }
   }

   DRETURN(ret);
}

/** @brief Delete a host group
 *
 * @param answer_list used to return error messages
 * @param name the host group to delete
 * @return true on success; false with `answer_list` filled otherwise
 */
bool hgroup_delete(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *hgroup = hgroup_create(answer_list, name, nullptr, true);
   
      if (hgroup != nullptr) {
         ret = hgroup_add_del_mod_via_gdi(hgroup, answer_list, ocs::gdi::Command::DEL);
      }
      lFreeElem(&hgroup);
   }
   DRETURN(ret);
}
