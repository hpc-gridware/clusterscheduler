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

#include <cstring>

#include "uti/sge_edit.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "gdi/ocs_gdi_Client.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_answer.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "ocs_qconf_rqs.h"
#include "ocs_qconf_parse.h"   /* CS-2313a: qconf_opt_format */
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qconf.h"
#include "msg_sgeobjlib.h"   /* MSG_RQS_REQUEST_DUPLICATE_NAME_S */

static bool
rqs_provide_modify_context(lList **rqs_list, lList **answer_list, bool ignore_unchanged_message);

/****** resource_quota_qconf/rqs_show() *********************************
*  NAME
*     rqs_show() -- show resource quota sets
*
*  SYNOPSIS
*     bool rqs_show(lList **answer_list, const char *name) 
*
*  FUNCTION
*     This funtion gets the selected resource quota sets from GDI and
*     writes they out on stdout
*
*  INPUTS
*     lList **answer_list - answer list
*     const char *name    - comma separated list of resource quota set names
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_show() is MT safe 
*
*******************************************************************************/
bool
rqs_show(lList **answer_list, const char *name)
{
   lList *rqs_list = nullptr;
   bool ret = false;

   DENTER(TOP_LAYER);

   if (name != nullptr) {
      lList *rqsref_list = nullptr;

      lString2List(name, &rqsref_list, RQS_Type, RQS_name, ", ");
      ret = rqs_get_via_gdi(answer_list, rqsref_list, &rqs_list);
      lFreeList(&rqsref_list);
   } else {
      ret = rqs_get_all_via_gdi(answer_list, &rqs_list);
   }

   if (ret && lGetNumberOfElem(rqs_list)) {
      const char* filename;
      filename = spool_flatfile_write_list(answer_list, rqs_list, RQS_fields, 
                                        &qconf_rqs_sfi,
                                        SP_DEST_STDOUT, qconf_opt_format, nullptr,
                                        false);
      sge_free(&filename);
   }
   if (lGetNumberOfElem(rqs_list) == 0) {
      answer_list_add(answer_list, MSG_NORQSFOUND, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
      ret = false;
   }

   lFreeList(&rqs_list);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_get_via_gdi() **************************
*  NAME
*     rqs_get_via_gdi() -- get resource quota sets from GDI
*
*  SYNOPSIS
*     bool rqs_get_via_gdi(lList **answer_list, const lList 
*     *rqsref_list, lList **rqs_list) 
*
*  FUNCTION
*     This function gets the selected resource quota sets from qmaster. The selection
*     is done in the string list rqsref_list.
*
*  INPUTS
*     lList **answer_list       - answer list
*     const lList *rqsref_list - resource quota sets selection
*     lList **rqs_list         - copy of the selected rule sets
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_get_via_gdi() is MT safe 
*
*******************************************************************************/
bool
rqs_get_via_gdi(lList **answer_list, const lList *rqsref_list, lList **rqs_list)
{
   bool ret = false;

   DENTER(TOP_LAYER);
   if (rqsref_list != nullptr) {
      lCondition *where = nullptr;
      lEnumeration *what = nullptr;

      what = lWhat("%T(ALL)", RQS_Type);

      for_each_ep_lv(rqsref, rqsref_list) {
         lCondition *add_where = nullptr;
         add_where = lWhere("%T(%I p= %s)", RQS_Type, RQS_name, lGetString(rqsref, RQS_name));
         if (where == nullptr) {
            where = add_where;
         } else {
            where = lOrWhere(where, add_where);
         }
      }
      *answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::RQS_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, rqs_list, where, what);
      if (!answer_list_has_error(answer_list)) {
         ret = true;
      }

      lFreeWhat(&what);
      lFreeWhere(&where);
   }

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_get_all_via_gdi() **********************
*  NAME
*     rqs_get_all_via_gdi() -- get all resource quota sets from GDI 
*
*  SYNOPSIS
*     bool rqs_get_all_via_gdi(lList **answer_list, lList **rqs_list) 
*
*  FUNCTION
*     This function gets all resource quota sets known by qmaster
*
*  INPUTS
*     lList **answer_list - answer list
*     lList **rqs_list   - copy of all resource quota sets
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_get_all_via_gdi() is MT safe 
*
*******************************************************************************/
bool
rqs_get_all_via_gdi(lList **answer_list, lList **rqs_list)
{
   bool ret = false;
   lEnumeration *what = lWhat("%T(ALL)", RQS_Type);

   DENTER(TOP_LAYER);

   *answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::RQS_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, rqs_list, nullptr, what);
   if (!answer_list_has_error(answer_list)) {
      ret = true;
   }

   lFreeWhat(&what);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_add() ******************************
*  NAME
*     rqs_add() -- add resource quota set list
*
*  SYNOPSIS
*     bool rqs_add(lList **answer_list, const char *name) 
*
*  FUNCTION
*     This function provide a modify context for qconf to add new resource
*     quota sets. If no name is given a template rule set is shown
*
*  INPUTS
*     lList **answer_list - answer list
*     const char *name    - comma seperated list of rule sets to add
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_add() is MT safe 
*
*******************************************************************************/
bool
rqs_add(lList **answer_list, const char *name)
{
   bool ret = false;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lList *rqs_list = nullptr;

      lString2List(name, &rqs_list, RQS_Type, RQS_name, ", ");
      for_each_rw_lv (rqs, rqs_list) {
         rqs = rqs_set_defaults(rqs);
      }

      ret = rqs_provide_modify_context(&rqs_list, answer_list, true);

      if (ret) {
         ret = rqs_add_del_mod_via_gdi(rqs_list, answer_list, ocs::gdi::Command::ADD, ocs::gdi::SubCommand::SET_ALL);
      }

      lFreeList(&rqs_list);
   }

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_modify() ***************************
*  NAME
*     rqs_modify() -- modify resource quota sets
*
*  SYNOPSIS
*     bool rqs_modify(lList **answer_list, const char *name) 
*
*  FUNCTION
*     This function provides a modify context for qconf to modify resource
*     quota sets.
*
*  INPUTS
*     lList **answer_list - answer list
*     const char *name    - comma seperated list of rule sets to modify
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_modify() is MT safe 
*
*******************************************************************************/
bool
rqs_modify(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);
   bool ret = false;
   lList *rqs_list = nullptr;
   ocs::gdi::Command cmd = ocs::gdi::Command::NONE;
   ocs::gdi::SubCommand sub_cmd = ocs::gdi::SubCommand::NONE;

   if (name != nullptr) {
      lList *rqsref_list = nullptr;

      cmd = ocs::gdi::Command::MOD;
      sub_cmd = ocs::gdi::SubCommand::SET_ALL;

      lString2List(name, &rqsref_list, RQS_Type, RQS_name, ", ");
      ret = rqs_get_via_gdi(answer_list, rqsref_list, &rqs_list);
      lFreeList(&rqsref_list);
   } else {
      cmd = ocs::gdi::Command::REPLACE;
      ret = rqs_get_all_via_gdi(answer_list, &rqs_list);
   }

   if (ret) {
      ret = rqs_provide_modify_context(&rqs_list, answer_list, false);
   }
   if (ret) {
      ret = rqs_add_del_mod_via_gdi(rqs_list, answer_list, cmd, sub_cmd);
   }

   lFreeList(&rqs_list);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_upsert_via_gdi() ******************************
*  NAME
*     rqs_upsert_via_gdi() -- modify-or-add the file's rqs, preserve the rest
*
*  SYNOPSIS
*     static bool rqs_upsert_via_gdi(lList **answer_list, lList *file_rqs_list)
*
*  FUNCTION
*     Applies the resource quota sets in @p file_rqs_list to the current rqs
*     configuration: an rqs is modified (MOD) if one of the same name already
*     exists, added (ADD) otherwise, and every other rqs is left untouched. This
*     gives -Arqs and -Mrqs (without an explicit rqs_list) the same add-or-modify
*     semantics as the per-object -Ap/-Mp, instead of replacing the whole rqs list
*     (which would silently delete rqs not present in the file). The file's rqs
*     are split by existence into a MOD batch and an ADD batch so that the
*     resulting messages accurately read "modified"/"added".
*
*  INPUTS
*     lList **answer_list   - answer list
*     lList *file_rqs_list  - the rqs read from the file (not modified/freed here)
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: rqs_upsert_via_gdi() is MT safe
*******************************************************************************/
static bool
rqs_upsert_via_gdi(lList **answer_list, lList *file_rqs_list)
{
   DENTER(TOP_LAYER);

   /* a file that lists the same rqs name more than once is ambiguous; reject it.
    * The former REPLACE (SET_ALL) path was rejected by qmaster with this message;
    * the per-element upsert below would otherwise silently apply each block in turn
    * (last-write-wins), so an equivalent client-side check restores the behaviour. */
   for (const lListElem *a = lFirst(file_rqs_list); a != nullptr; a = lNext(a)) {
      const char *name_a = lGetString(a, RQS_name);
      for (const lListElem *b = lNext(a); b != nullptr; b = lNext(b)) {
         if (name_a != nullptr && strcmp(name_a, lGetString(b, RQS_name)) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                                    MSG_RQS_REQUEST_DUPLICATE_NAME_S, name_a);
            DRETURN(false);
         }
      }
   }

   lList *current_list = nullptr;
   if (!rqs_get_all_via_gdi(answer_list, &current_list)) {
      DRETURN(false);
   }

   /* split the file's rqs into those that already exist (MOD) and those that are
    * new (ADD); both commands leave rqs not present in the file untouched and
    * produce accurate "modified"/"added" messages (unlike REPLACE, which reports
    * a spurious "removed" for every changed rule set). */
   lList *mod_list = nullptr;
   lList *add_list = nullptr;
   lListElem *file_rqs;
   for_each_ep_lv(file_rqs, file_rqs_list) {
      const char *rqs_name = lGetString(file_rqs, RQS_name);
      lList **target = (rqs_list_locate(current_list, rqs_name) != nullptr) ? &mod_list : &add_list;
      if (*target == nullptr) {
         *target = lCreateList("rqs_list", RQS_Type);
      }
      lAppendElem(*target, lCopyElem(file_rqs));
   }
   lFreeList(&current_list);

   bool ret = true;
   if (ret && mod_list != nullptr) {
      ret = rqs_add_del_mod_via_gdi(mod_list, answer_list, ocs::gdi::Command::MOD, ocs::gdi::SubCommand::SET_ALL);
   }
   if (ret && add_list != nullptr) {
      ret = rqs_add_del_mod_via_gdi(add_list, answer_list, ocs::gdi::Command::ADD, ocs::gdi::SubCommand::SET_ALL);
   }
   lFreeList(&mod_list);
   lFreeList(&add_list);

   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_add_from_file() ********************
*  NAME
*     rqs_add_from_file() -- add resource quota set from file
*
*  SYNOPSIS
*     bool rqs_add_from_file(lList **answer_list, const char 
*     *filename) 
*
*  FUNCTION
*     This function add new resource quota sets from file.
*
*  INPUTS
*     lList **answer_list  - answer list
*     const char *filename - filename of new resource quota sets
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_add_from_file() is MT safe 
*
*******************************************************************************/
bool
rqs_add_from_file(lList **answer_list, const char *filename) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (filename != nullptr) {
      lList *rqs_list = nullptr;

      /* fields_out field does not work for rqs because of duplicate entry */
      rqs_list = spool_flatfile_read_list(answer_list, RQS_Type, RQS_fields, nullptr, true, &qconf_rqs_sfi, qconf_opt_format, nullptr, filename);
      if (!answer_list_has_error(answer_list)) {
         /* upsert the file's rqs (modify-or-add) and keep all other rqs, matching
          * -Mrqs and the per-object -Ap/-Mp behaviour */
         ret = rqs_upsert_via_gdi(answer_list, rqs_list);
      }

      lFreeList(&rqs_list);
   }
   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_provide_modify_context() ***********
*  NAME
*     rqs_provide_modify_context() -- provide qconf modify context
*
*  SYNOPSIS
*     bool rqs_provide_modify_context(lList **rqs_list, lList 
*     **answer_list, bool ignore_unchanged_message) 
*
*  FUNCTION
*     This function provides a editor session to edit the selected resource quota
*     sets interactively. 
*
*  INPUTS
*     lList **rqs_list             - resource quota sets to modify
*     lList **answer_list           - answer list
*     bool ignore_unchanged_message - ignore unchanged message
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_provide_modify_context() is MT safe 
*
*******************************************************************************/
static bool
rqs_provide_modify_context(lList **rqs_list, lList **answer_list, bool ignore_unchanged_message)
{
   bool ret = false;
   int status = 0;
   const char *filename = nullptr;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();
   
   DENTER(TOP_LAYER);

   if (rqs_list == nullptr) {
      answer_list_add(answer_list, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      DRETURN(ret); 
   }

   if (*rqs_list == nullptr) {
      *rqs_list = lCreateList("", RQS_Type);
   }

   filename = spool_flatfile_write_list(answer_list, *rqs_list, RQS_fields, &qconf_rqs_sfi, SP_DEST_TMP, qconf_opt_format, filename, false);

   if (answer_list_has_error(answer_list)) {
      if (filename != nullptr) {
         unlink(filename);
         sge_free(&filename);
      }
      DRETURN(ret);
   }

   status = sge_edit(filename, uid, gid);

   if (status == 0) {
      lList *new_rqs_list = nullptr;

      /* fields_out field does not work for rqs because of duplicate entry */
      new_rqs_list = spool_flatfile_read_list(answer_list, RQS_Type, RQS_fields,
                                               nullptr, true, &qconf_rqs_sfi,
                                               qconf_opt_format, nullptr, filename);
      if (answer_list_has_error(answer_list)) {
         lFreeList(&new_rqs_list);
      }
      if (new_rqs_list != nullptr) {
         if (ignore_unchanged_message || object_list_has_differences(new_rqs_list, answer_list, *rqs_list)) {
            lFreeList(rqs_list);
            *rqs_list = new_rqs_list;
            ret = true;
         } else {
            lFreeList(&new_rqs_list);
            answer_list_add(answer_list, MSG_FILE_NOTCHANGED,
                            STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE,
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
   } else if (status == 1) {
      answer_list_add(answer_list, MSG_FILE_FILEUNCHANGED,
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   } else {
      answer_list_add(answer_list, MSG_PARSE_EDITFAILED,
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }

   unlink(filename);
   sge_free(&filename);
   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_add_del_mod_via_gdi() **************
*  NAME
*    rqs_add_del_mod_via_gdi/rqs_add_del_mod_via_gdi() -- modfies qmaster resource quota sets
*
*  SYNOPSIS
*     bool rqs_add_del_mod_via_gdi(lList *rqs_list, lList 
*     **answer_list, uint32_t gdi_command)
*
*  FUNCTION
*     This function modifies via GDI the qmaster copy of the resource quota sets.
*
*  INPUTS
*     lList *rqs_list     - resource quota sets to modify on qmaster
*     lList **answer_list  - answer list from qmaster
*     uint32_t gdi_command - commands what to do
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_add_del_mod_via_gdi() is MT safe 
*
*******************************************************************************/
bool
rqs_add_del_mod_via_gdi(lList *rqs_list, lList **answer_list, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_cmd)
{
   bool ret = false;
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   
   DENTER(TOP_LAYER);

   if (rqs_list != nullptr) {
      bool do_verify = (cmd == ocs::gdi::Command::MOD) ||
                       (cmd == ocs::gdi::Command::ADD ||
                       (cmd == ocs::gdi::Command::REPLACE)) ? true : false;

      if (do_verify) {
         ret = rqs_list_verify_attributes(rqs_list, answer_list, false, master_centry_list);
      }
      if (ret) {
         lList *my_answer_list = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::RQS_LIST, cmd, sub_cmd, &rqs_list, nullptr, nullptr);
         if (my_answer_list != nullptr) {
            answer_list_append_list(answer_list, &my_answer_list);
         }
      }
   }
   DRETURN(ret);
}

/****** resource_quota_qconf/rqs_modify_from_file() *****************
*  NAME
*     rqs_modify_from_file() -- modifies resource quota sets from file
*
*  SYNOPSIS
*     bool rqs_modify_from_file(lList **answer_list, const char 
*     *filename, const char* name) 
*
*  FUNCTION
*     This function allows to modify one or all resource quota sets from a file
*
*  INPUTS
*     lList **answer_list  - answer list
*     const char *filename - filename with the resource quota sets to change
*     const char* name     - comma separated list of rule sets to change
*
*  RESULT
*     bool - true  on success
*            false on error
*
*  NOTES
*     MT-NOTE: rqs_modify_from_file() is MT safe 
*
*******************************************************************************/
bool
rqs_modify_from_file(lList **answer_list, const char *filename, const char* name)
{
   bool ret = false;
   ocs::gdi::Command cmd = ocs::gdi::Command::NONE;
   ocs::gdi::SubCommand sub_cmd = ocs::gdi::SubCommand::NONE;

   DENTER(TOP_LAYER);
   if (filename != nullptr) {
      lList *rqs_list = nullptr;

      /* fields_out field does not work for rqs because of duplicate entry */
      rqs_list = spool_flatfile_read_list(answer_list, RQS_Type, RQS_fields,
                                          nullptr, true, &qconf_rqs_sfi,
                                          qconf_opt_format, nullptr, filename);
      if (rqs_list != nullptr) {

         if (name != nullptr && strlen(name) > 0 ) {
            /* -Mrqs <file> <rqs_list>: modify only the explicitly named rqs (which
             * must be present in the file) and leave every other rqs untouched */
            lList *selected_rqs_list = nullptr;
            lList *found_rqs_list = lCreateList("rqs_list", RQS_Type);
            bool ok = true;

            cmd = ocs::gdi::Command::MOD;
            sub_cmd = ocs::gdi::SubCommand::SET_ALL;

            lString2List(name, &selected_rqs_list, RQS_Type, RQS_name, ", ");
            for_each_ep_lv(tmp_rqs, selected_rqs_list) {
               lListElem *found = rqs_list_locate(rqs_list, lGetString(tmp_rqs, RQS_name));
               if (found != nullptr) {
                  lAppendElem(found_rqs_list, lCopyElem(found));
               } else {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_RQS_NOTFOUNDINFILE_SS, lGetString(tmp_rqs, RQS_name), filename);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
                  ok = false;
                  break;
               }
            }
            lFreeList(&selected_rqs_list);
            if (ok) {
               ret = rqs_add_del_mod_via_gdi(found_rqs_list, answer_list, cmd, sub_cmd);
            }
            lFreeList(&found_rqs_list);
         } else {
            /* -Mrqs <file> without an explicit rqs_list behaves like the per-object
             * -Mp/-Mq: upsert each rqs contained in the file (modify it if it
             * already exists, add it if it is new) and leave every other rqs
             * untouched - rather than REPLACE-ing the whole rqs configuration. */
            ret = rqs_upsert_via_gdi(answer_list, rqs_list);
         }
      }
      lFreeList(&rqs_list);
   }
   DRETURN(ret);
}
