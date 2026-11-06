/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The attribute switches of `qconf`, see the header
 */

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_tmpnam.h"
#include "uti/sge.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/config.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_resource_quota.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "gdi/ocs_gdi_Client.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_attr.h"
#include "ocs_qconf_parse.h"

/** @brief One object type the attribute switches serve */
struct qconf_attr_entry {
   ocs::gdi::Target target;                  ///< which master list the object lives in
   const char *object_name;                  ///< the name the switches take, e.g. "queue"
   const lDescr *cull_descriptor;                  ///< the CULL type of the object
   const char *attribute_name;               ///< the attribute carrying the object's name, for the messages
   int nm_name;                              ///< the CULL field holding the object's name, its key
   spooling_field *fields;                   ///< which fields the attribute text is read with
   const struct spool_flatfile_instr *instr; ///< how the attribute text is laid out
   bool (*pre_gdi_function)(lList *list, lList **answer_list);   ///< validation to run before sending, or `nullptr`
};

/** @brief Write one attribute line "name<delimiter>value" into a new temporary file
 *
 * @param name the attribute name
 * @param value the attribute value
 * @param delimiter what separates name and value
 * @param error_message receives the reason when the file could not be written
 * @return the name of the file, allocated with sge_malloc() and owned by the
 *         caller, or nullptr on error
 */
static const char *
write_attr_tmp_file(const char *name, const char *value, const char *delimiter, dstring *error_message) {
   DENTER(TOP_LAYER);

   char *filename = sge_malloc(sizeof(char) * SGE_PATH_MAX);
   int fd = -1;
   if (sge_tmpnam(filename, &fd, error_message) == nullptr) {
      sge_free(&filename);
      DRETURN(nullptr);
   }

   errno = 0;
   FILE *fp = fdopen(fd, "w");
   const int my_errno = errno;

   if (fp == nullptr) {
      sge_dstring_sprintf(error_message, MSG_ERROROPENINGFILEFORWRITING_SS, filename, strerror(my_errno));
      sge_free(&filename);
      DRETURN(nullptr);
   }

   fprintf(fp, "%s", name);
   fprintf(fp, "%s", delimiter);
   fprintf(fp, "%s\n", value);

   FCLOSE(fp);

   DRETURN((const char *)filename);
FCLOSE_ERROR:
   sge_free(&filename);
   DRETURN(nullptr);
}

/**
 * @brief Send the delta of one attribute operation to qmaster
 *
 * The attributes come from a file ("-Mattr" and the other capital switches) or
 * from the command line as "name value", which is written to a temporary file
 * so that it is read the same way. The delta is the element read, copied once
 * per named object with that object's name set; the request carries only the
 * fields that were read and the name. Depending on the sub-command qmaster
 * modifies parts of a sublist, appends to it, removes from it or replaces it.
 *
 * @param alpp the answer list, receives qmaster's answer or the reason the request was not sent
 * @param op the operation; its arguments are the object type, the file or the
 *        attribute name and value, and the names of the objects
 * @param from_file whether the attributes come from a file
 * @param sub_command what the request does with the sublist: CHANGE modifies
 *        sublist elements, APPEND adds elements, REMOVE removes them, SET
 *        replaces the complete sublist
 * @param info_entry the object type
 * @return 1 when the request was not sent, 0 otherwise
 */
static int
qconf_modify_attribute(lList **alpp, const qconf_op &op, const bool from_file, const ocs::gdi::SubCommand sub_command,
                       const qconf_attr_entry *info_entry) {
   DENTER(TOP_LAYER);

   int fields[150];
   fields[0] = NoName;
   lListElem *ep = nullptr;
   size_t first_name;

   if (from_file) {
      first_name = 2;
      ep = spool_flatfile_read_object(alpp, info_entry->cull_descriptor, nullptr, info_entry->fields, fields, true,
                                      info_entry->instr, qconf_opt_format, nullptr, op.args[1].c_str());
      if (answer_list_has_error(alpp)) {
         lFreeElem(&ep);
         DRETURN(1);
      }
      if (ep == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, SFNMAX, MSG_FILE_ERRORREADINGINFILE);
         DRETURN(1);
      }
   } else {
      first_name = 3;
      const char *name = op.args[1].c_str();
      const char *value = op.args[2].c_str();
      DSTRING_STATIC(delim, 100);

      if (!strcmp(info_entry->object_name, SGE_OBJ_RQS) && !strcmp(name, "limit")) {
         sge_dstring_append(&delim, " to ");
      } else {
         sge_dstring_append_char(&delim, info_entry->instr->name_value_delimiter);
      }
      dstring write_attr_tmp_file_error = DSTRING_INIT;
      const char *filename = write_attr_tmp_file(name, value, sge_dstring_get_string(&delim),
                                                 &write_attr_tmp_file_error);
      if (filename == nullptr && sge_dstring_get_string(&write_attr_tmp_file_error) != nullptr) {
         answer_list_add_sprintf(alpp, STATUS_EDISK, ANSWER_QUALITY_ERROR, "%s",
                                 sge_dstring_get_string(&write_attr_tmp_file_error));
      } else if (filename != nullptr) {
         ep = spool_flatfile_read_object(alpp, info_entry->cull_descriptor, nullptr, info_entry->fields, fields,
                                         true, info_entry->instr, SP_FORM_ASCII, nullptr, filename);
         unlink(filename);
         sge_free(&filename);
      }
      sge_dstring_free(&write_attr_tmp_file_error);

      /* Bugfix: Issuezilla #1005
       * Since we're writing the information from the command line to a file so
       * we can read it, the error messages that come back from the flatfile
       * spooling will sound a little odd.  To avoid this, we hijack the answer
       * list and replace the error messages with ones that will make better
       * sense. */
      if (answer_list_has_error(alpp)) {
         for_each_rw_lv(aep, *alpp) {
            if (answer_has_quality(aep, ANSWER_QUALITY_ERROR) && (answer_get_status(aep) == STATUS_ESYNTAX)) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_QCONF_BAD_ATTR_ARGS_SS, name, value);
               lSetString(aep, AN_text, SGE_EVENT);
            }
         }
         sge_dstring_free(&delim);
         lFreeElem(&ep);
         DRETURN(1);
      }
      sge_dstring_free(&delim);
      if (ep == nullptr) {
         DRETURN(1);
      }
   }

   /* add object name to int vector and transform it into an lEnumeration */
   if (add_nm_to_set(fields, info_entry->nm_name) < 0) {
      answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_QCONF_CANTCHANGEOBJECTNAME_SS, "qconf",
                              info_entry->attribute_name);
      lFreeElem(&ep);
      DRETURN(1);
   }

   lEnumeration *what = lIntVector2What(info_entry->cull_descriptor, fields);
   if (what == nullptr) {
      lFreeElem(&ep);
      DRETURN(1);
   }

   /* one copy of the delta per named object */
   lList *qlp = nullptr;
   for (size_t i = first_name; i < op.args.size(); i++) {
      const char *object = op.args[i].c_str();
      if (qlp == nullptr) {
         qlp = lCreateList("list", info_entry->cull_descriptor);
      }
      lListElem *add_qp = lCopyElem(ep);
      switch (lGetType(add_qp->descr, info_entry->nm_name)) {
         case lUlongT:
            lSetUlong(add_qp, info_entry->nm_name, atol(object));
            break;
         case lLongT:
            lSetLong(add_qp, info_entry->nm_name, atol(object));
            break;
         case lIntT:
            lSetInt(add_qp, info_entry->nm_name, atoi(object));
            break;
         case lDoubleT:
            lSetDouble(add_qp, info_entry->nm_name, atof(object));
            break;
         case lStringT:
            lSetString(add_qp, info_entry->nm_name, object);
            break;
         case lHostT:
            lSetHost(add_qp, info_entry->nm_name, object);
            break;
         default:
            answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_QCONF_INTERNALFAILURE_S, "qconf");
            lFreeElem(&add_qp);
            lFreeList(&qlp);
            lFreeWhat(&what);
            lFreeElem(&ep);
            DRETURN(1);
      }
      lAppendElem(qlp, add_qp);
   }

   if (qlp == nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_QCONF_MQATTR_MISSINGOBJECTLIST_S,
                              "qconf");
      lFreeElem(&ep);
      lFreeWhat(&what);
      DRETURN(1);
   }

   /* Bugfix: Issuezilla #1025
    * If we get a list value of NONE with -mattr, complain.  This comes after
    * the complaint about not including the object list on purpose.  That order
    * seems to make the most sense for error reporting, even though it means we
    * do some unnecessary work. */
   if ((sub_command & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE &&
       lGetType(ep->descr, fields[0]) == lListT) {
      const lList *lp = lGetList(ep, fields[0]);

      if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, SFNMAX, MSG_QCONF_CANT_MODIFY_NONE);
         lFreeElem(&ep);
         lFreeWhat(&what);
         lFreeList(&qlp);
         DRETURN(1);
      } else if (lGetNumberOfElem(lp) == 1) {
         const lListElem *first = lFirst(lp);

         /* Look for the list and see if it's nullptr.  These should all be very
          * simple CULL structures which should only contain an lHostT and an
          * lListT. */
         for (const auto f : ocs_cull_present_fields(first->descr)) {
            const int nm = first->descr[f.pos].nm;
            // for CE_Type objects which are no RSMAPs CE_resource_map_list will be nullptr - this is OK
            if (lGetType(first->descr, nm) == lListT && lGetList(first, nm) == nullptr && nm != CE_resource_map_list) {
               answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, SFNMAX,
                                       MSG_QCONF_CANT_MODIFY_NONE);
               lFreeElem(&ep);
               lFreeWhat(&what);
               lFreeList(&qlp);
               DRETURN(1);
            }
         }
      }
   }

   if (info_entry->pre_gdi_function == nullptr || info_entry->pre_gdi_function(qlp, alpp)) {
      *alpp = ocs::gdi::Client::sge_gdi(info_entry->target, ocs::gdi::Command::MOD, sub_command, &qlp, nullptr, what);
   }
   lFreeElem(&ep);
   lFreeWhat(&what);
   lFreeList(&qlp);

   DRETURN(0);
}

QConfOpResult
qconf_run_attr(const qconf_op &op, lList **alpp) {
   DENTER(TOP_LAYER);

   const std::string &t = op.token;
   if (t != "-mattr" && t != "-Mattr" && t != "-aattr" && t != "-Aattr" && t != "-rattr" && t != "-Rattr" &&
       t != "-dattr" && t != "-Dattr") {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   if (qconf_missing_argument(op, alpp)) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }

   /* the field lists are globals, so the table is filled at run time; the exec
    * host's is built and has to be freed */
   qconf_attr_entry info_entry[] = {
      {ocs::gdi::Target::CQ_LIST,   SGE_OBJ_CQUEUE,   CQ_Type,   SGE_ATTR_QNAME,     CQ_name,   CQ_fields,
       &qconf_sfi,     cqueue_xattr_pre_gdi},
      {ocs::gdi::Target::EH_LIST,   SGE_OBJ_EXECHOST, EH_Type,   SGE_ATTR_HOSTNAME,  EH_name,
       sge_build_EH_field_list(false, false, false, false), &qconf_sfi, nullptr},
      {ocs::gdi::Target::PE_LIST,   SGE_OBJ_PE,       PE_Type,   SGE_ATTR_PE_NAME,   PE_name,   PE_fields,
       &qconf_sfi,     nullptr},
      {ocs::gdi::Target::CK_LIST,   SGE_OBJ_CKPT,     CK_Type,   SGE_ATTR_CKPT_NAME, CK_name,   CK_fields,
       &qconf_sfi,     nullptr},
      {ocs::gdi::Target::HGRP_LIST, SGE_OBJ_HGROUP,   HGRP_Type, SGE_ATTR_HGRP_NAME, HGRP_name, HGRP_fields,
       &qconf_sfi,     nullptr},
      {ocs::gdi::Target::RQS_LIST,  SGE_OBJ_RQS,      RQS_Type,  SGE_ATTR_RQS_NAME,  RQS_name,  RQS_fields,
       &qconf_rqs_sfi, rqs_xattr_pre_gdi},
   };
   spooling_field *eh_fields = info_entry[1].fields;

   /* a capital letter reads the attributes from a file; the letter after the
    * dash is what is done with them */
   const bool from_file = isupper(t[1]);
   ocs::gdi::SubCommand sub_command = ocs::gdi::SubCommand::NONE;
   switch (tolower(t[1])) {
      case 'm':
         sub_command = ocs::gdi::SubCommand::CHANGE;
         break;
      case 'a':
         sub_command = ocs::gdi::SubCommand::APPEND;
         break;
      case 'd':
         sub_command = ocs::gdi::SubCommand::REMOVE;
         break;
      case 'r':
         sub_command = ocs::gdi::SubCommand::SET;
         break;
      default:
         break;
   }

   const qconf_attr_entry *entry = nullptr;
   for (const qconf_attr_entry &e : info_entry) {
      if (op.args[0] == e.object_name) {
         entry = &e;
         break;
      }
   }
   if (entry == nullptr) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_QCONF_MODIFICATIONOFOBJECTNOTSUPPORTED_S, op.args[0].c_str());
      sge_free(&eh_fields);
      DRETURN(QConfOpResult::FAILED);
   }

   lList *alp = nullptr;
   const int ret = qconf_modify_attribute(&alp, op, from_file, sub_command, entry);
   sge_free(&eh_fields);

   /* these switches report a success on stdout, so the list is printed here,
    * with the informations on stdout (CS-2971) */
   bool failed = false;
   if (ret != 0 || lGetNumberOfElem(alp) > 0) {
      failed = qconf_answer_take(alpp, &alp) != 0;
      qconf_answer_print(alpp, stdout);
   }
   lFreeList(&alp);

   DRETURN(failed ? QConfOpResult::FAILED : QConfOpResult::OK);
}
