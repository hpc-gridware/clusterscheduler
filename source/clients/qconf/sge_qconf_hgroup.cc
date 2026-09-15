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
#include "msg_common.h"
#include "msg_qconf.h"


static void 
hgroup_list_show_elem(lList *hgroup_list, const char *name, int indent);
static bool
hgroup_provide_modify_context(lListElem **this_elem, lList **answer_list, bool ignore_unchanged_message);


static void
hgroup_list_show_elem(lList *hgroup_list, const char *name, int indent) {
   DENTER(TOP_LAYER);

   const char *const indent_string = "   ";
   const lListElem *hgroup = nullptr;
   int i;

   for (i = 0; i < indent; i++) {
      printf("%s", indent_string);
   }

   /*
    * CS-2680, N-I-6. A matcher is a leaf: it references nothing that could be
    * expanded, and looking it up as a group name would be asking the wrong
    * question. It is marked, because the tree is where the shape of a group is
    * read off and a rule among the names should not read like one more name.
    */
   if (ocs::is_matcher(name)) {
      printf("%s (matcher)\n", name);
      DRETURN_VOID;
   }
   printf("%s\n", name);

   hgroup = lGetElemHost(hgroup_list, HGRP_name, name);
   if (hgroup != nullptr) {
      const lList *sub_list = lGetList(hgroup, HGRP_host_list);

      for_each_ep_lv(href, sub_list) {
         const char *href_name = lGetHost(href, HR_name);

         hgroup_list_show_elem(hgroup_list, href_name, indent + 1); 
      } 
   } 
   DRETURN_VOID;
}

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

/** @brief Print one host group
 *
 * @param answer_list used to return error messages
 * @param name the host group to print
 * @return true on success; false with `answer_list` filled otherwise
 */
bool hgroup_show(lList **answer_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lListElem *hgroup = hgroup_get_via_gdi(answer_list, name);
   
      if (hgroup != nullptr) {
         /*
          * CS-2680, N-I-14. In the machine readable output the three member
          * classes get keys of their own, so a program reading along need not
          * guess the class of an entry from its text. They are written *beside*
          * the unchanged "hostlist", not instead of it: that one is the
          * definition as it is stored, it is what -Mhgrp reads back, and the
          * round trip has to stay idempotent.
          *
          * Only a group carrying a matcher gets them. Splitting the entries of
          * every other group would change the output of installations that never
          * heard of this feature, which N-I-1 rules out - and a member list
          * without matchers holds nothing a reader cannot tell apart by its
          * leading '@' already.
          */
         lList *derived_hosts = nullptr;
         lList *derived_groups = nullptr;
         lList *derived_matchers = nullptr;
         const bool typed = qconf_opt_format == SP_FORM_JSON &&
                            hgroup_split_members(hgroup, &derived_hosts, &derived_groups,
                                                 &derived_matchers);

         if (typed) {
            const ocs_json_derived_list derived[] = {
                    {"hosts",      derived_hosts,    HR_name},
                    {"hostgroups", derived_groups,   HR_name},
                    {"matchers",   derived_matchers, HR_name},
                    {nullptr,      nullptr,          0}
            };
            dstring document = DSTRING_INIT;

            if (spool_json_write_object_ex(answer_list, hgroup, HGRP_fields, derived, &document)) {
               printf("%s", sge_dstring_get_string(&document));
            }
            sge_dstring_free(&document);
         } else {
            const char *filename;
            filename = spool_flatfile_write_object(answer_list, hgroup, false, HGRP_fields, &qconf_sfi, SP_DEST_STDOUT, qconf_opt_format, nullptr, false);

            sge_free(&filename);
         }
         lFreeList(&derived_hosts);
         lFreeList(&derived_groups);
         lFreeList(&derived_matchers);
         lFreeElem(&hgroup);

         if (answer_list_has_error(answer_list)) {
            DRETURN(false);
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_HGROUP_NOTEXIST_S, name);
         ret = false;
      }
   }
   DRETURN(ret);
}

/** @brief Say by which route a host is a member of a host group (CS-2680, N-I-7)
 *
 * As long as a member list was an enumeration the answer stood there to be read.
 * With a matcher it no longer does: a group may carry several, and the resolved
 * membership says only *who* - for a host admitted through a matcher it does not
 * even contain the host. The typical occasion is a host that is permitted
 * something nobody knowingly allowed it, and the question is which entry did it.
 *
 * The **definition** is walked, never the resolution, and the first route found
 * is reported (see hgroup_why()).
 *
 * @param answer_list used to return error messages
 * @param group the host group to ask
 * @param hostname the host in question, as it was written on the command line
 * @param[out] is_member receives whether a route was found, for the return value
 *        of the command (N-I-8); a caller uninterested in it may pass nullptr
 *
 * @return true when the question could be answered at all; false with
 *         @p answer_list filled when the group does not exist
 */
bool hgroup_show_why(lList **answer_list, const char *group, const char *hostname, bool *is_member) {
   DENTER(TOP_LAYER);

   if (is_member != nullptr) {
      *is_member = false;
   }
   if (group == nullptr || hostname == nullptr) {
      DRETURN(false);
   }

   lList *hgroup_list = nullptr;
   lEnumeration *what = lWhat("%T(ALL)", HGRP_Type);
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &hgroup_list, nullptr, what);
   lFreeWhat(&what);

   const lListElem *alep = lFirst(alp);
   answer_exit_if_not_recoverable(alep);
   if (answer_get_status(alep) != STATUS_OK) {
      fprintf(stderr, "%s\n", lGetString(alep, AN_text));
      lFreeList(&alp);
      lFreeList(&hgroup_list);
      DRETURN(false);
   }

   const lListElem *hgroup = lGetElemHost(hgroup_list, HGRP_name, group);
   bool ret = true;

   if (hgroup == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                              MSG_HGROUP_NOTEXIST_S, group);
      ret = false;
   } else {
      /*
       * The host is resolved the way every other host argument of this client is,
       * so that a short name and the name the group stores compare equal. A name
       * that does not resolve is not an error here: the question is answerable
       * for it, and the answer is that it is not a member.
       */
      lListElem *probe = lCreateElem(HR_Type);
      lSetHost(probe, HR_name, hostname);
      sge_resolve_host(probe, HR_name);

      dstring found_in = DSTRING_INIT;
      dstring matcher = DSTRING_INIT;
      const ocs::Matcher::Route route =
              hgroup_why(hgroup, lGetHost(probe, HR_name), hgroup_list, &found_in, &matcher);
      const char *host = lGetHost(probe, HR_name);
      const char *in = sge_dstring_get_string(&found_in);

      // a cleared dstring hands back an empty string rather than nothing once it
      // has held a value, so "no group to report" is emptiness and not nullptr
      if (in != nullptr && in[0] == '\0') {
         in = nullptr;
      }

      // the message macros expand to a catalogue lookup, not to a string literal,
      // so the newline cannot be concatenated onto them
      switch (route) {
         case ocs::Matcher::Route::LITERAL:
            printf(MSG_QCONF_WHY_LITERAL_S, host);
            break;
         case ocs::Matcher::Route::GROUP_REFERENCE:
            printf(MSG_QCONF_WHY_GROUP_SS, host, in != nullptr ? in : "");
            break;
         case ocs::Matcher::Route::MATCHER:
            if (in != nullptr) {
               printf(MSG_QCONF_WHY_MATCHER_GROUP_SSS, host,
                      sge_dstring_get_string(&matcher), in);
            } else {
               printf(MSG_QCONF_WHY_MATCHER_SS, host, sge_dstring_get_string(&matcher));
            }
            break;
         case ocs::Matcher::Route::NOT_A_MEMBER:
            printf(MSG_QCONF_WHY_NOT_S, host);
            break;
      }
      printf("\n");

      if (is_member != nullptr) {
         *is_member = route != ocs::Matcher::Route::NOT_A_MEMBER;
      }

      sge_dstring_free(&found_in);
      sge_dstring_free(&matcher);
      lFreeElem(&probe);
   }

   lFreeList(&alp);
   lFreeList(&hgroup_list);

   DRETURN(ret);
}

/** @brief Print a host group's members, either as a tree or resolved flat
 *
 * A host group may contain other host groups, so there are two useful views:
 * the tree, which shows where each host comes from, and the resolved list,
 * which shows what the group finally means. `qconf -shgrp_tree` asks for the
 * first, `-shgrp_resolved` for the second.
 *
 * @param answer_list used to return error messages
 * @param name the host group to print
 * @param show_tree true for the tree, false for the resolved list
 * @return true on success; false with `answer_list` filled otherwise
 */
bool hgroup_show_structure(lList **answer_list, const char *name, bool show_tree) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (name != nullptr) {
      lList *hgroup_list = nullptr;
      const lListElem *hgroup = nullptr;
      lEnumeration *what = nullptr;
      lList *alp = nullptr;
      const lListElem *alep = nullptr;

      what = lWhat("%T(ALL)", HGRP_Type);
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, &hgroup_list, nullptr, what);
      lFreeWhat(&what);

      alep = lFirst(alp);
      answer_exit_if_not_recoverable(alep);
      if (answer_get_status(alep) != STATUS_OK) {
         fprintf(stderr, "%s\n", lGetString(alep, AN_text));
         lFreeList(&alp);
         DRETURN(false);
      }

      hgroup = lGetElemHost(hgroup_list, HGRP_name, name); 
      if (hgroup != nullptr) {
         if (show_tree) {
            hgroup_list_show_elem(hgroup_list, name, 0);
         } else {
            dstring string = DSTRING_INIT;
            lList *sub_host_list = nullptr;
            lList *sub_hgroup_list = nullptr;
            lList *sub_matcher_list = nullptr;

            hgroup_find_all_references(hgroup, answer_list, hgroup_list, &sub_host_list, &sub_hgroup_list,
                                       &sub_matcher_list);
            href_list_make_uniq(sub_host_list, answer_list);
            href_list_append_to_dstring(sub_host_list, &string);
            if (sge_dstring_get_string(&string)) {
               printf("%s\n", sge_dstring_get_string(&string));
            }

            /*
             * CS-2680, N-I-5. The resolved membership says which hosts the system
             * knows are members, never which hosts the group would admit. For a
             * group carrying matchers those are two different things, and the gap
             * may be total -- a submit host group whose population is disjoint
             * from the execution host list resolves to nothing while admitting a
             * host on every request. So the matchers are shown with it, and the
             * output above is never left standing on its own as "the membership".
             * Without matchers nothing is added and the output is unchanged.
             */
            if (sub_matcher_list != nullptr) {
               dstring matchers = DSTRING_INIT;

               href_list_make_uniq(sub_matcher_list, answer_list);
               href_list_append_to_dstring(sub_matcher_list, &matchers);
               printf("matchers: %s\n", sge_dstring_get_string(&matchers));
               sge_dstring_free(&matchers);
            }

            sge_dstring_free(&string);
            lFreeList(&sub_host_list);
            lFreeList(&sub_hgroup_list);
            lFreeList(&sub_matcher_list);
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_HGROUP_NOTEXIST_S, name);
         ret = false;
      }

      lFreeList(&hgroup_list);
      lFreeList(&alp);
   }
   DRETURN(ret);
}
