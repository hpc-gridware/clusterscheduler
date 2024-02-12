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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_object.h"
#include "uti/sge_rmon.h"

#include "uti/sge_edit.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_unistd.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi2.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "msg_common.h"
#include "msg_qconf.h"
#include "oge_qconf_centry.h"

static bool centry_provide_modify_context(lListElem **this_elem, lList **answer_list);
static bool centry_list_provide_modify_context(lList **this_list, lList **answer_list);

bool centry_add_del_mod_via_gdi(lListElem *this_elem, lList **answer_list, u_long32 gdi_command) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (this_elem != nullptr) {
      lList *centry_list = nullptr;
      lList *gdi_answer_list = nullptr;

      centry_list = lCreateList("", CE_Type);
      lAppendElem(centry_list, this_elem);
      gdi_answer_list = sge_gdi2(SGE_CE_LIST, gdi_command, &centry_list, nullptr, nullptr);
      answer_list_replace(answer_list, &gdi_answer_list);
   }

   DRETURN(ret);
}

lListElem *centry_get_via_gdi(lList **answer_list, const char *name) {
   lListElem *ret = nullptr;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lList *gdi_answer_list = nullptr;
      lEnumeration *what = nullptr;
      lCondition *where = nullptr;
      lList *centry_list = nullptr;

      what = lWhat("%T(ALL)", CE_Type);
      where = lWhere("%T(%I==%s)", CE_Type, CE_name, name);
      gdi_answer_list = sge_gdi2(SGE_CE_LIST, SGE_GDI_GET, &centry_list, where, what);
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (!answer_list_has_error(&gdi_answer_list)) {
         ret = lFirstRW(centry_list);
      } else {
         answer_list_replace(answer_list, &gdi_answer_list);
      }
   }

   DRETURN(ret);
}

static bool centry_provide_modify_context(lListElem **this_elem, lList **answer_list) {
   bool ret = false;
   int status = 0;
   lList *alp = nullptr;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   DENTER(TOP_LAYER);

   if (this_elem != nullptr && *this_elem) {
      const char *filename = nullptr;

      filename = spool_flatfile_write_object(&alp, *this_elem, false, CE_fields, &qconf_ce_sfi, SP_DEST_TMP,
                                             SP_FORM_ASCII, filename, false);

      if (answer_list_output(&alp)) {
         if (filename != nullptr) {
            unlink(filename);
            sge_free(&filename);
         }
         DRETURN(false);
      }

      status = sge_edit(filename, uid, gid);
      if (status >= 0) {
         lListElem *centry;

         fields_out[0] = NoName;
         centry = spool_flatfile_read_object(&alp, CE_Type, nullptr, CE_fields, fields_out, true, &qconf_ce_sfi,
                                             SP_FORM_ASCII, nullptr, filename);

         if (answer_list_output(&alp)) {
            lFreeElem(&centry);
         }

         if (centry != nullptr) {
            missing_field = spool_get_unprocessed_field(CE_fields, fields_out, &alp);
         }

         if (missing_field != NoName) {
            lFreeElem(&centry);
            answer_list_output(&alp);
         }

         if (centry != nullptr) {
            lFreeElem(this_elem);
            *this_elem = centry;
            ret = true;
         } else {
            answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_PARSE_EDITFAILED, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
      unlink(filename);
      sge_free(&filename);
   }

   lFreeList(&alp);

   DRETURN(ret);
}

bool centry_add(lList **answer_list, const char *name) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lListElem *centry = centry_create(answer_list, name);

      if (centry == nullptr) {
         ret = false;
      }
      if (ret) {
         ret &= centry_provide_modify_context(&centry, answer_list);
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(centry, answer_list, SGE_GDI_ADD);
      }
   }

   DRETURN(ret);
}

bool centry_add_from_file(lList **answer_list, const char *filename) {
   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER);
   if (filename != nullptr) {
      lListElem *centry;

      fields_out[0] = NoName;
      centry = spool_flatfile_read_object(answer_list, CE_Type, nullptr, CE_fields, fields_out, true, &qconf_ce_sfi,
                                          SP_FORM_ASCII, nullptr, filename);

      if (answer_list_output(answer_list)) {
         lFreeElem(&centry);
      }

      if (centry != nullptr) {
         missing_field = spool_get_unprocessed_field(CE_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&centry);
         answer_list_output(answer_list);
      }

      if (centry == nullptr) {
         ret = false;
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(centry, answer_list, SGE_GDI_ADD);
      }
   }

   DRETURN(ret);
}

bool centry_modify(lList **answer_list, const char *name) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lListElem *centry = centry_get_via_gdi(answer_list, name);

      if (centry == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_CENTRY_DOESNOTEXIST_S, name);
         ret = false;
      }
      if (ret) {
         ret &= centry_provide_modify_context(&centry, answer_list);
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(centry, answer_list, SGE_GDI_MOD);
      }
      if (centry) {
         lFreeElem(&centry);
      }
   }

   DRETURN(ret);
}

bool centry_modify_from_file(lList **answer_list, const char *filename) {
   bool ret = true;
   int fields_out[MAX_NUM_FIELDS];
   int missing_field = NoName;

   DENTER(TOP_LAYER);
   if (filename != nullptr) {
      lListElem *centry;

      fields_out[0] = NoName;
      centry = spool_flatfile_read_object(answer_list, CE_Type, nullptr, CE_fields, fields_out, true, &qconf_ce_sfi,
                                          SP_FORM_ASCII, nullptr, filename);

      if (answer_list_output(answer_list)) {
         lFreeElem(&centry);
      }

      if (centry != nullptr) {
         missing_field = spool_get_unprocessed_field(CE_fields, fields_out, answer_list);
      }

      if (missing_field != NoName) {
         lFreeElem(&centry);
         answer_list_output(answer_list);
      }

      if (centry == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_CENTRY_FILENOTCORRECT_S,
                                 filename);
         ret = false;
      }
      if (ret) {
         ret &= centry_add_del_mod_via_gdi(centry, answer_list, SGE_GDI_MOD);
      }
      if (centry) {
         lFreeElem(&centry);
      }
   }

   DRETURN(ret);
}

bool centry_delete(lList **answer_list, const char *name) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lListElem *centry = centry_create(answer_list, name);

      if (centry != nullptr) {
         ret &= centry_add_del_mod_via_gdi(centry, answer_list, SGE_GDI_DEL);
      }
   }

   DRETURN(ret);
}

bool centry_show(lList **answer_list, const char *name) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (name != nullptr) {
      lListElem *centry = centry_get_via_gdi(answer_list, name);

      if (centry != nullptr) {
         const char *filename;
         filename = spool_flatfile_write_object(answer_list, centry, false, CE_fields, &qconf_ce_sfi, SP_DEST_STDOUT,
                                                SP_FORM_ASCII, nullptr, false);
         sge_free(&filename);
         lFreeElem(&centry);
         if (answer_list_has_error(answer_list)) {
            DRETURN(false);
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_CENTRY_DOESNOTEXIST_S, name);
         ret = false;
      }
   }
   DRETURN(ret);
}

bool centry_list_show(lList **answer_list) {
   bool ret = true;
   lList *centry_list = nullptr;

   DENTER(TOP_LAYER);
   centry_list = centry_list_get_via_gdi(answer_list);
   if (centry_list != nullptr) {
      const char *filename;

      spool_flatfile_align_list(answer_list, (const lList *)centry_list, CE_fields, 3);
      filename = spool_flatfile_write_list(answer_list, centry_list, CE_fields, &qconf_ce_list_sfi, SP_DEST_STDOUT,
                                           SP_FORM_ASCII, nullptr, false);

      sge_free(&filename);
      lFreeList(&centry_list);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   DRETURN(ret);
}

lList *centry_list_get_via_gdi(lList **answer_list) {
   lList *ret = nullptr;
   lList *gdi_answer_list = nullptr;
   lEnumeration *what = nullptr;

   DENTER(TOP_LAYER);
   what = lWhat("%T(ALL)", CE_Type);
   gdi_answer_list = sge_gdi2(SGE_CE_LIST, SGE_GDI_GET, &ret, nullptr, what);
   lFreeWhat(&what);

   if (answer_list_has_error(&gdi_answer_list)) {
      answer_list_replace(answer_list, &gdi_answer_list);
   }

   centry_list_sort(ret);

   lFreeList(&gdi_answer_list);

   DRETURN(ret);
}

bool
centry_list_add_del_mod_via_gdi(lList **this_list, lList **answer_list, lList **old_list) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (!this_list || !old_list) {
      SGE_ADD_MSG_ID(sprintf(SGE_EVENT, MSG_INAVLID_PARAMETER_IN_S, __func__));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (*this_list != nullptr) {
      lList *modify_list = nullptr;
      lList *add_list = nullptr;
      lListElem *centry_elem = nullptr;
      lListElem *next_centry_elem = nullptr;
      bool cont = true;

      /* check for duplicate names */
      next_centry_elem = lFirstRW(*this_list);
      while ((centry_elem = next_centry_elem)) {
         lListElem *cmp_elem = lFirstRW(*this_list);

         while ((centry_elem != cmp_elem)) {
            const char *name1 = nullptr;
            const char *name2 = nullptr;
            const char *shortcut1 = nullptr;
            const char *shortcut2 = nullptr;

            const char *attrname;
            double dval;
            char error_msg[200];

            const char *urgency1 = nullptr;
            const char *urgency2 = nullptr;

            /* Bugfix: Issuezilla 1161
             * Previously it was assumed that name and shortcut would never be
             * nullptr.  In the course of testing for duplicate names, each name
             * would potentially be accessed twice.  Now, in order to check for
             * nullptr without making a mess, I access each name exactly once.  In
             * some cases, that may be 50% more than the previous code, but in
             * what I expect is the usual case, it will be 50% less. */
            name1 = lGetString(centry_elem, CE_name);
            name2 = lGetString(cmp_elem, CE_name);
            shortcut1 = lGetString(centry_elem, CE_shortcut);
            shortcut2 = lGetString(cmp_elem, CE_shortcut);

            if (name1 == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                       MSG_CENTRY_NULL_NAME);
               DRETURN(false);
            } else if (name2 == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                       MSG_CENTRY_NULL_NAME);
               DRETURN(false);
            } else if (shortcut1 == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CENTRY_NULL_SHORTCUT_S,
                                       name1);
               DRETURN(false);
            } else if (shortcut2 == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CENTRY_NULL_SHORTCUT_S,
                                       name2);
               DRETURN(false);
            } else if ((strcmp(name1, name2) == 0) || (strcmp(name1, shortcut2) == 0) ||
                       (strcmp(shortcut1, name2) == 0) || (strcmp(shortcut1, shortcut2) == 0)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_ANSWER_COMPLEXXALREADYEXISTS_SS, name1, shortcut1);
               cont = false;
            }
            /* Let's also check if the new urgency is nullptr */
            /* Check first that the entry is not nullptr  */
            urgency1 = lGetString(centry_elem, CE_urgency_weight);
            urgency2 = lGetString(cmp_elem, CE_urgency_weight);

            if (urgency1 == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                       MSG_CENTRY_NULL_URGENCY);
               DRETURN(false);
            } else if (urgency2 == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, SFNMAX,
                                       MSG_CENTRY_NULL_URGENCY);
               DRETURN(false);
            }

            /* Check then that the entry is valid  */
            error_msg[0] = '\0';
            attrname = lGetString(centry_elem, CE_name);

            if (!parse_ulong_val(&dval, nullptr, TYPE_DOUBLE, urgency1, error_msg, 199) ||
                !parse_ulong_val(&dval, nullptr, TYPE_DOUBLE, urgency2, error_msg, 199)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_PARSE_URGENCY_SS, attrname, error_msg);
               DRETURN(false);
            }

            cmp_elem = lNextRW(cmp_elem);
         }

         if (!centry_elem_validate(centry_elem, nullptr, answer_list)) {
            cont = false;
         }

         next_centry_elem = lNextRW(centry_elem);
      }

      if (!cont) {
         DRETURN(false);
      }

      modify_list = lCreateList("", CE_Type);
      add_list = lCreateList("", CE_Type);
      next_centry_elem = lFirstRW(*this_list);
      while ((centry_elem = next_centry_elem)) {
         const char *name = lGetString(centry_elem, CE_name);
         lListElem *tmp_elem = centry_list_locate(*old_list, name);

         next_centry_elem = lNextRW(centry_elem);
         if (tmp_elem != nullptr) {
            lDechainElem(*this_list, centry_elem);
            if (object_has_differences(centry_elem, nullptr, tmp_elem, false)) {
               lAppendElem(modify_list, centry_elem);
            } else {
               lFreeElem(&centry_elem);
            }
            lRemoveElem(*old_list, &tmp_elem);
         } else {
            lDechainElem(*this_list, centry_elem);
            lAppendElem(add_list, centry_elem);
         }
      }
      lFreeList(this_list);

      {
         lList *gdi_answer_list = nullptr;
         lList *mal_answer_list = nullptr;
         state_gdi_multi state = STATE_GDI_MULTI_INIT;
         int del_id = -1;
         int mod_id = -1;
         int add_id = -1;
         int number_req = 0;
         bool do_del = false;
         bool do_mod = false;
         bool do_add = false;

         /*
          * How many requests are summarized to one multi request?
          */
         if (lGetNumberOfElem(*old_list) > 0) {
            number_req++;
            do_del = true;
         }
         if (lGetNumberOfElem(modify_list) > 0) {
            number_req++;
            do_mod = true;
         }
         if (lGetNumberOfElem(add_list) > 0) {
            number_req++;
            do_add = true;
         }

         /*
          * Do the multi request
          */
         if (ret && do_del) {
            int mode = (--number_req > 0) ? SGE_GDI_RECORD : SGE_GDI_SEND;
            del_id = sge_gdi2_multi(&gdi_answer_list, mode, SGE_CE_LIST, SGE_GDI_DEL, old_list, nullptr, nullptr, &state,
                                    false);
            if (mode == SGE_GDI_SEND) {
               sge_gdi2_wait(&gdi_answer_list, &mal_answer_list, &state);
            }
            if (answer_list_has_error(&gdi_answer_list)) {
               answer_list_append_list(answer_list, &gdi_answer_list);
               DTRACE;
               ret = false;
            }
            lFreeList(old_list);
         }
         if (ret && do_mod) {
            int mode = (--number_req > 0) ? SGE_GDI_RECORD : SGE_GDI_SEND;
            mod_id = sge_gdi2_multi(&gdi_answer_list, mode, SGE_CE_LIST, SGE_GDI_MOD, &modify_list, nullptr, nullptr,
                                    &state, false);
            if (mode == SGE_GDI_SEND) {
               sge_gdi2_wait(&gdi_answer_list, &mal_answer_list, &state);
            }
            if (answer_list_has_error(&gdi_answer_list)) {
               answer_list_append_list(answer_list, &gdi_answer_list);
               DTRACE;
               ret = false;
            }
            lFreeList(&modify_list);
         }
         if (ret && do_add) {
            int mode = (--number_req > 0) ? SGE_GDI_RECORD : SGE_GDI_SEND;
            add_id = sge_gdi2_multi(&gdi_answer_list, mode, SGE_CE_LIST, SGE_GDI_ADD, &add_list, nullptr, nullptr,
                                    &state, false);
            if (mode == SGE_GDI_SEND) {
               sge_gdi2_wait(&gdi_answer_list, &mal_answer_list, &state);
            }
            if (answer_list_has_error(&gdi_answer_list)) {
               answer_list_append_list(answer_list, &gdi_answer_list);
               DTRACE;
               ret = false;
            }
            lFreeList(&add_list);
         }

         /*
          * Verify that the parts of the multi request are successful
          */
         if (do_del && ret) {
            sge_gdi_extract_answer(&gdi_answer_list, SGE_GDI_DEL, SGE_CE_LIST, del_id, mal_answer_list, nullptr);
            answer_list_append_list(answer_list, &gdi_answer_list);
         }
         if (do_mod && ret) {
            sge_gdi_extract_answer(&gdi_answer_list, SGE_GDI_MOD, SGE_CE_LIST, mod_id, mal_answer_list, nullptr);
            answer_list_append_list(answer_list, &gdi_answer_list);
         }
         if (do_add && ret) {
            sge_gdi_extract_answer(&gdi_answer_list, SGE_GDI_ADD, SGE_CE_LIST, add_id, mal_answer_list, nullptr);
            answer_list_append_list(answer_list, &gdi_answer_list);
         }

         /*
          * Provide an overall summary for the callee
          */
         if (lGetNumberOfElem(*answer_list) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_OK, ANSWER_QUALITY_INFO, SFNMAX, MSG_CENTRY_NOTCHANGED);
         }

         lFreeList(&gdi_answer_list);
         lFreeList(&mal_answer_list);
      }

      lFreeList(&add_list);
      lFreeList(&modify_list);
   }

   DRETURN(ret);
}

bool centry_list_modify(lList **answer_list) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (ret) {
      lList *centry_list = centry_list_get_via_gdi(answer_list);
      lList *old_centry_list = lCopyList("", centry_list);

      ret &= centry_list_provide_modify_context(&centry_list, answer_list);
      if (ret) {
         ret &= centry_list_add_del_mod_via_gdi(&centry_list, answer_list, &old_centry_list);
      }

      lFreeList(&centry_list);
      lFreeList(&old_centry_list);
   }
   DRETURN(ret);
}

bool centry_list_modify_from_file(lList **answer_list, const char *filename) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (ret) {
      lList *old_centry_list = nullptr;
      lList *centry_list = nullptr;

      centry_list = spool_flatfile_read_list(answer_list, CE_Type, CE_fields, nullptr, true, &qconf_ce_list_sfi,
                                             SP_FORM_ASCII, nullptr, filename);

      if (answer_list_output(answer_list)) {
         lFreeList(&centry_list);
      }

      old_centry_list = centry_list_get_via_gdi(answer_list);

      if (centry_list == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_CENTRY_FILENOTCORRECT_S,
                                 filename);
         ret = false;
      }
      if (ret) {
         ret &= centry_list_add_del_mod_via_gdi(&centry_list, answer_list, &old_centry_list);
      }

      lFreeList(&centry_list);
      lFreeList(&old_centry_list);
   }
   DRETURN(ret);
}

static bool centry_list_provide_modify_context(lList **this_list, lList **answer_list) {
   bool ret = false;
   int status = 0;
   uid_t uid = component_get_uid();
   gid_t gid = component_get_gid();

   DENTER(TOP_LAYER);
   if (this_list != nullptr) {
      const char *filename;

      spool_flatfile_align_list(answer_list, (const lList *)*this_list, CE_fields, 3);
      filename = spool_flatfile_write_list(answer_list, *this_list, CE_fields, &qconf_ce_list_sfi, SP_DEST_TMP,
                                           SP_FORM_ASCII, nullptr, false);

      if (answer_list_output(answer_list)) {
         if (filename != nullptr) {
            unlink(filename);
            sge_free(&filename);
         }
         DRETURN(false);
      }

      status = sge_edit(filename, uid, gid);
      if (status >= 0) {
         lList *centry_list;

         centry_list = spool_flatfile_read_list(answer_list, CE_Type, CE_fields, nullptr, true, &qconf_ce_list_sfi,
                                                SP_FORM_ASCII, nullptr, filename);

         if (answer_list_output(answer_list)) {
            lFreeList(&centry_list);
         }

         if (centry_list != nullptr) {
            lFreeList(this_list);
            *this_list = centry_list;
            ret = true;
         } else {
            answer_list_add(answer_list, MSG_FILE_ERRORREADINGINFILE, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         }
      } else {
         answer_list_add(answer_list, MSG_PARSE_EDITFAILED, STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
      unlink(filename);
      sge_free(&filename);
   }
   DRETURN(ret);
}
