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
 * @brief The read switches of `qconf`: show, list and export, see the header
 */

#include <cstring>
#include <memory>
#include <ostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "sgeobj/sge_answer.h"
#include "spool/flatfile/sge_flatfile.h"
#include "procedure/ocs_ProcedureController.h"
#include "procedure/ocs_ProcedureModel.h"
#include "procedure/ocs_ProcedureView.h"
#include "qconf/ocs_QConfController.h"
#include "qconf/table/ocs_QConfTableController.h"
#include "qconf/table/ocs_QConfTableView.h"
#include "qconf/threadpool/ocs_QConfThreadPoolController.h"
#include "qconf/threadpool/ocs_QConfThreadPoolView.h"
#include "qconf/sharetree/ocs_QConfShareTreeNodesController.h"
#include "qconf/sharetree/ocs_QConfShareTreeTreeController.h"
#include "qconf/sharetree/ocs_QConfShareTreeView.h"
#include "qconf/processors/ocs_QConfProcessorController.h"
#include "qconf/processors/ocs_QConfProcessorView.h"
#include "qconf/eventclient/ocs_QConfEventClientController.h"
#include "qconf/eventclient/ocs_QConfEventClientView.h"
#include "qconf/objl/ocs_QConfObjectFilterController.h"
#include "qconf/objl/ocs_QConfObjectFilterView.h"
#include "qconf/sick/ocs_QConfSickController.h"
#include "qconf/sick/ocs_QConfSickView.h"
#include "qconf/cqueue/ocs_QConfCQueueShowController.h"
#include "qconf/cqueue/ocs_QConfCQueueShowView.h"
#include "qconf/hgroup/ocs_QConfHGroupResolvedController.h"
#include "qconf/hgroup/ocs_QConfHGroupShowController.h"
#include "qconf/hgroup/ocs_QConfHGroupShowViewJSON.h"
#include "qconf/hgroup/ocs_QConfHGroupStructureView.h"
#include "qconf/hgroup/ocs_QConfHGroupTreeController.h"
#include "qconf/hgroup/ocs_QConfHGroupWhyController.h"
#include "qconf/hgroup/ocs_QConfHGroupWhyView.h"
#include "qconf/ocs_QConfModelClient.h"
#include "qconf/ocs_QConfViewJSON.h"
#include "qconf/ocs_QConfViewPlain.h"
#include "qconf/ocs_QConfParameterClient.h"
#include "msg_qconf.h"

#include "ocs_qconf_read.h"
#include "ocs_qconf_common.h"
#include "ocs_qconf_parse.h"

/* ================= CS-23xx: bulk export ( qconf -S<obj> name|dir ) =================
 *
 * The inverse of the -A<obj>/-M<obj> import: pull objects from qmaster with a read-only GDI
 * GET and write each to its own re-importable flatfile. Each file is written with the
 * same IMPORT field list that -A<obj>/-M<obj> read, so an exported file round-trips
 * by construction (KTD2). The argument disambiguates like the import side: an existing
 * directory or a trailing '/' selects directory mode (one file per object, named after
 * it); otherwise it is a single object name written to ./<name>.
 */

/**
 * @brief Run one export read as a stored procedure (CS-2826)
 *
 * The fetch and the rendering are qmaster's: the request names the export
 * switch, the read procedure resolves it to the object type through the same
 * table every other read uses, and the response carries one rendered text per
 * object together with the name its file is called after.
 *
 * Two things are refused here rather than attempted.
 *
 * The client execution context, because it has no reduction channel - a plain
 * GET returns the whole object and so reports "not reduced" whatever the caller
 * is allowed to see. That is the same reason the six modifying switches of
 * CS-2825 decline it, and it holds more strongly for an export: its files are
 * what a restore reads back, so a part of an object written as the whole of it
 * is a backup that silently drops what it could not see.
 *
 * A reduced answer, for the same reason, and before a single file is written.
 * The whole export is refused rather than the objects that were reduced,
 * because a directory holding some objects in full and others in part is worse
 * than no directory: nothing in it says which is which.
 *
 * @param param   the invocation, whose operation is set for the call
 * @param sw      the export switch, named in the refusals
 * @param name    the object to export, nullptr for the run over everything
 * @param label   what the refusal calls the target - the name or the directory
 * @param model   the caller's model; it owns the response the pairs live in
 * @param objects receives the name/text pairs, on success only
 * @return 0 when @p objects was filled, 1 when the export must not go ahead
 */
static int
qconf_export_procedure(ocs::QConfParameterClient &param, const char *sw, const char *name,
                       const char *label, ocs::ProcedureModel &model, const lList **objects) {
   DENTER(TOP_LAYER);

   if (param.get_exec_context() != ocs::ProcedureParameter::ExecContext::SERVER) {
      ERROR(MSG_QCONF_MODNEEDSSERVERCTX_S, sw);
      DRETURN(1);
   }

   qconf_op op;
   op.token = sw;
   if (name != nullptr) {
      op.args.emplace_back(name);
   }
   param.set_operation(op);
   param.set_sub_procedure_name(ocs::QConfParameter::OBJECT_EXPORT);
   param.set_for_export(true);

   lList *alp = nullptr;
   const bool ok = model.make_snapshot(&alp, param);

   param.set_for_export(false);
   param.set_sub_procedure_name("");

   if (!ok) {
      answer_list_output(&alp);
      lFreeList(&alp);
      DRETURN(1);
   }
   lFreeList(&alp);

   if (model.get_reduction() != ocs::ProcedureParameter::REDUCTION_NONE) {
      ERROR(MSG_QCONF_REDUCEDNOTEXPORTABLE_SS, sw, label);
      DRETURN(1);
   }

   /* The entry being absent is not an empty export - it is a response that never
    * answered the question, and reporting "0 objects exported" for it would hand
    * back a successful exit code for a backup that did not happen. */
   if (!model.get_export_objects(objects)) {
      ERROR(MSG_QCONF_EXPORTNORESULT_S, sw);
      DRETURN(1);
   }

   DRETURN(0);
}

/**
 * @brief True if @p name is safe to use verbatim as an export file name (R5/KTD7).
 *
 * Rejects empty, ".", "..", a name containing '/', and a name beginning with '-'
 * (which would be reparsed as an option on re-import). Never sanitizes - an unsafe
 * name is reported as an error for that object.
 *
 * @param name the object name
 * @return true when the name may be used as a file name
 */
static bool
qconf_name_is_fs_safe(const char *name) {
   if (name == nullptr || name[0] == '\0') {
      return false;
   }
   if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      return false;
   }
   if (name[0] == '-') {
      return false;
   }
   if (strchr(name, '/') != nullptr) {
      return false;
   }
   return true;
}

/**
 * @brief Write one exported object's text to its file (KTD4).
 *
 * Honors -f: an existing destination is skipped with a warning (non-fatal) unless
 * force is set. The text goes to a temporary file beside the destination and is
 * then relocated with rename(), so a reader never sees half a file and a failed
 * write leaves the previous one in place. Beside it rather than in the system
 * temporary directory, which is what makes the rename atomic: both names are on
 * the one filesystem, so the cross-filesystem copy the flatfile writer's
 * SP_DEST_TMP needed has no case left to serve.
 *
 * @param os   the stream the output is written to (CS-1841)
 * @param text the file content, rendered by qmaster; nullptr is the wire form of
 *             an empty text, because CULL packs an empty string as no string -
 *             so it is an empty file and not a missing one
 * @param dest the file to write
 * @return 0 on success or a (non-fatal) skip, 1 on a write/relocate failure.
 */
static int
qconf_export_write_one(std::ostream &os, const char *text, const char *dest) {
   if (text == nullptr) {
      text = "";
   }
   if (sge_is_file(dest)) {
      if (!qconf_opt_force) {
         WARNING(MSG_QCONF_EXPORTSKIPPEDEXISTS_S, dest);
         return 0;   /* -f overwrites (R4/KTD6); without it this is a non-fatal skip */
      }
   }

   dstring tmp_name = DSTRING_INIT;
   sge_dstring_sprintf(&tmp_name, "%s.%d.tmp", dest, static_cast<int>(getpid()));
   const char *tmp = sge_dstring_get_string(&tmp_name);

   int ret = 0;
   FILE *fp = fopen(tmp, "w");
   if (fp == nullptr) {
      ERROR(MSG_QCONF_EXPORTWRITEFAILED_S, dest);
      ret = 1;
   } else {
      const size_t len = strlen(text);
      /* fclose is checked as well: a write that only failed on the flush would
       * otherwise be renamed into place as a truncated file. */
      if ((len > 0 && fwrite(text, 1, len, fp) != len) || fclose(fp) != 0) {
         ERROR(MSG_QCONF_EXPORTWRITEFAILED_S, dest);
         unlink(tmp);
         ret = 1;
      } else if (rename(tmp, dest) != 0) {
         ERROR(MSG_QCONF_EXPORTWRITEFAILED_S, dest);
         unlink(tmp);
         ret = 1;
      }
   }
   sge_dstring_free(&tmp_name);

   if (ret == 0) {
      qconf_info(os, MSG_QCONF_EXPORTWROTE_S, dest);
   }
   return ret;
}

/**
 * @brief Build the destination path for one exported object (R3).
 *
 * @param dest   receives the path
 * @param dir    directory in bulk mode, or nullptr in single mode (cwd).
 * @param name   object name (already FS-safe checked).
 * @param suffix ".json" under -fmt json, "" otherwise.
 */
static void
qconf_export_dest(dstring *dest, const char *dir, const char *name, const char *suffix) {
   sge_dstring_clear(dest);
   if (dir != nullptr) {
      sge_dstring_sprintf(dest, "%s/%s%s", dir, name, suffix);
   } else {
      sge_dstring_sprintf(dest, "%s%s", name, suffix);
   }
}

/**
 * @brief Whether the export argument names a directory of many files (R1/R2).
 *
 * An existing directory or a trailing '/' selects the run over every object of
 * the type; anything else is one object written to the working directory. The
 * same disambiguation the import side makes.
 *
 * @param path the name|dir argument
 * @return true for the run over everything
 */
static bool
qconf_export_is_bulk(const char *path) {
   const size_t plen = strlen(path);
   return sge_is_directory(path) || (plen > 0 && path[plen - 1] == '/');
}

/**
 * @brief Create the export directory if it is missing, and normalize its name.
 *
 * @param dir  receives the directory without a trailing '/'
 * @param size the size of @p dir
 * @param path the name|dir argument
 * @return true when the directory exists afterwards
 */
static bool
qconf_export_make_dir(char *dir, const size_t size, const char *path) {
   sge_strlcpy(dir, path, size);
   const size_t dl = strlen(dir);
   if (dl > 1 && dir[dl - 1] == '/') {
      dir[dl - 1] = '\0';
   }

   if (!sge_is_directory(dir)) {
      /* KTD3: 4-arg, non-recursive, exit_on_error=false */
      if (sge_mkdir(dir, 0755, false, false) != 0 || !sge_is_directory(dir)) {
         ERROR(MSG_QCONF_EXPORTCANTMKDIR_S, dir);
         return false;
      }
   }
   return true;
}

/**
 * @brief Export objects of one type to a file or a directory of files (R1-R5,R8).
 *
 * Directory mode exports every object of the type, one file each, creating the
 * directory if missing. Single mode exports the one object named by @p path to
 * `./<name>`. Per-object failures are counted and reported but do not abort the run
 * (continue-and-report, KTD9).
 *
 * Since CS-2826 the objects arrive rendered: one text per object, from the read
 * procedure, so what is left here is the file system - which name a text is
 * written under, whether that name is usable, and whether it collides. Only the
 * fetch and the rendering moved; every message this printed it still prints.
 *
 * @param os    the stream the output is written to (CS-1841)
 * @param param the invocation, used for the export read
 * @param sw    the export switch being served; it is also what names the object
 *              type, which the read procedure resolves - so the client no longer
 *              carries a descriptor for an export at all (CS-2826)
 * @param path  the name|dir argument
 * @return the number of objects that failed to export
 */
static int
qconf_export_path(std::ostream &os, ocs::QConfParameterClient &param,
                  const char *sw, const char *path) {
   const char *suffix = (qconf_opt_format == SP_FORM_JSON) ? ".json" : "";
   const bool is_bulk = qconf_export_is_bulk(path);

   /* The directory is created before the read, as it was: a directory that
    * cannot be created is reported on its own and no request is sent for it. */
   char dir[4096] = {};
   if (is_bulk && !qconf_export_make_dir(dir, sizeof(dir), path)) {
      return 1;
   }

   if (!is_bulk && !qconf_name_is_fs_safe(path)) {
      ERROR(MSG_QCONF_EXPORTUNSAFENAME_S, path);
      return 1;
   }

   ocs::ProcedureModel model;
   const lList *objects = nullptr;
   if (qconf_export_procedure(param, sw, is_bulk ? nullptr : path,
                              is_bulk ? dir : path, model, &objects) != 0) {
      return 1;
   }

   int failures = 0;
   dstring dest = DSTRING_INIT;

   if (is_bulk) {
      /* R5/KTD7: case-insensitive basename collision guard across the run. The
       * seen list holds the lowercased names already used. */
      lList *seen = lCreateList("export seen", ST_Type);
      const int n_total = (objects != nullptr) ? lGetNumberOfElem(objects) : 0;

      for_each_ep_lv(pair, objects) {
         const char *name = lGetString(pair, VA_variable);
         if (!qconf_name_is_fs_safe(name)) {
            ERROR(MSG_QCONF_EXPORTUNSAFENAME_S, name != nullptr ? name : "");
            failures++;
            continue;
         }
         char lcbuf[1024];
         sge_strlcpy(lcbuf, name, sizeof(lcbuf));
         sge_strtolower(lcbuf, sizeof(lcbuf));
         if (lGetElemStr(seen, ST_name, lcbuf) != nullptr) {
            ERROR(MSG_QCONF_EXPORTCOLLISION_SS, name, lcbuf);
            failures++;
            continue;
         }
         lAddElemStr(&seen, ST_name, lcbuf, ST_Type);

         qconf_export_dest(&dest, dir, name, suffix);
         failures += qconf_export_write_one(os, lGetString(pair, VA_value),
                                            sge_dstring_get_string(&dest));
      }
      lFreeList(&seen);

      qconf_info(os, MSG_QCONF_EXPORTSUMMARY_SII, dir, n_total - failures, failures);
   } else {
      /* Single mode keeps the spelling the user typed as the file name, the way
       * it always did: "qconf -Sconf GLOBAL" writes GLOBAL, and that file
       * re-imports to the same host key because the lookup folds case too. */
      const lListElem *pair = lFirst(objects);
      if (pair == nullptr) {
         ERROR(MSG_QCONF_EXPORTNOOBJECT_S, path);
         failures++;
      } else {
         qconf_export_dest(&dest, nullptr, path, suffix);
         failures += qconf_export_write_one(os, lGetString(pair, VA_value),
                                            sge_dstring_get_string(&dest));
      }
   }

   sge_dstring_free(&dest);
   return failures;
}

/**
 * @brief Export a true singleton (sharetree, sched_conf) to a single file (R6).
 *
 * A directory argument is rejected - singletons have no directory-of-many form.
 * The one object is read and written verbatim to the given path.
 *
 * @param os    the stream the output is written to (CS-1841)
 * @param param the invocation, used for the export read
 * @param sw    the export switch being served, see qconf_export_path()
 * @param path  the file to write
 * @return the number of failures (0 on success).
 */
static int
qconf_export_singleton(std::ostream &os, ocs::QConfParameterClient &param,
                       const char *sw, const char *path) {
   if (qconf_export_is_bulk(path)) {
      ERROR(MSG_QCONF_EXPORTNODIRALLOWED_S, path);
      return 1;
   }

   ocs::ProcedureModel model;
   const lList *objects = nullptr;
   if (qconf_export_procedure(param, sw, nullptr, path, model, &objects) != 0) {
      return 1;
   }

   /* A singleton that is not there is reported by the path that names a file for
    * it, because that is the name the user gave - the read answers an absent
    * object with an empty result and leaves the wording to the caller. */
   const lListElem *pair = lFirst(objects);
   if (pair == nullptr) {
      ERROR(MSG_QCONF_EXPORTNOOBJECT_S, path);
      return 1;
   }

   return qconf_export_write_one(os, lGetString(pair, VA_value), path);
}

bool
qconf_dispatch_procedure(std::ostream &os, ocs::QConfParameterClient &param, lList **alpp,
                         uint32_t *reduction) {
   DENTER(TOP_LAYER);

   /* CS-2822: the client context has no reduction channel at all - a plain GET
    * returns the whole object - so it answers "not reduced" and would be the
    * wrong basis for a decision that depends on it. The write path therefore
    * refuses that context rather than trusting this value; see
    * qconf_edit_obj_procedure(). */
   if (reduction != nullptr) {
      *reduction = ocs::ProcedureParameter::REDUCTION_NONE;
   }

   bool ok;
   if (param.get_exec_context() == ocs::ProcedureParameter::ExecContext::SERVER) {
      ocs::ProcedureModel model;
      ok = model.make_snapshot(alpp, param);

      /* CS-2821 opened this channel, CS-2822 is its first consumer: the write
       * path refuses to offer an object for editing that the caller can only
       * see in part. Read on every call, not only where it is wanted, so a
       * traced run shows that the entry survived the GDI round trip. */
      const uint32_t mask = model.get_reduction();
      DPRINTF("procedure reduction mask: " sge_u32 "\n", mask);
      if (reduction != nullptr) {
         *reduction = mask;
      }

      /* Rendered unconditionally since CS-2820: whatever qmaster produced is
       * printed even when it also reported a problem, because a procedure may
       * mean both - see the note in ProcedureModel::make_snapshot(). A read that
       * failed produced no text, so this prints nothing for it, and the caller
       * still sees "ok" as false and reports the error as before. */
      ocs::ProcedureView view(param);
      ocs::ProcedureController controller(os);
      controller.process_request(param, model, view);
   } else {
      ocs::QConfModelClient model;
      ok = model.make_snapshot(alpp, param);
      if (ok) {
         /* CS-2816: the client side mirror of the traits table. A sub-procedure
          * brings its own view and controller, so picking them by output format
          * alone is not enough any more. */
         const std::string &sub = param.get_sub_procedure_name();

         if (sub == ocs::QConfParameter::HGROUP_TREE) {
            ocs::QConfHGroupStructureView view(param);
            ocs::QConfHGroupTreeController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::HGROUP_RESOLVED) {
            ocs::QConfHGroupStructureView view(param);
            ocs::QConfHGroupResolvedController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::HGROUP_WHY) {
            ocs::QConfHGroupWhyView view(param);
            ocs::QConfHGroupWhyController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::CENTRY_LIST ||
                    sub == ocs::QConfParameter::CATEGORY_LIST) {
            ocs::QConfTableView view(param);
            ocs::QConfTableController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::QUEUE_SICK) {
            ocs::QConfSickView view(param);
            ocs::QConfSickController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::EVENT_CLIENT_LIST) {
            ocs::QConfEventClientView view(param);
            ocs::QConfEventClientController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::PROCESSOR_LIST) {
            ocs::QConfProcessorView view(param);
            ocs::QConfProcessorController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::THREAD_POOL_LIST) {
            ocs::QConfThreadPoolView view(param);
            ocs::QConfThreadPoolController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::SHARETREE_TREE) {
            ocs::QConfShareTreeView view(param);
            ocs::QConfShareTreeTreeController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::SHARETREE_NODES) {
            ocs::QConfShareTreeView view(param);
            ocs::QConfShareTreeNodesController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::OBJECT_FILTER) {
            ocs::QConfObjectFilterView view(param);
            ocs::QConfObjectFilterController controller(os);
            controller.process_request(param, model, view);
         } else if (sub == ocs::QConfParameter::CQUEUE_SHOW) {
            ocs::QConfCQueueShowView view(param);
            ocs::QConfCQueueShowController controller(os);
            controller.process_request(param, model, view);
         } else {
            std::unique_ptr<ocs::QConfViewBase> view;
            if (param.get_output_format() == ocs::ProcedureParameter::OutputFormat::JSON) {
               if (sub == ocs::QConfParameter::HGROUP_SHOW) {
                  view = std::make_unique<ocs::QConfHGroupShowViewJSON>(param);
               } else {
                  view = std::make_unique<ocs::QConfViewJSON>(param);
               }
            } else {
               view = std::make_unique<ocs::QConfViewPlain>(param);
            }

            if (sub == ocs::QConfParameter::HGROUP_SHOW) {
               ocs::QConfHGroupShowController controller(os);
               controller.process_request(param, model, *view);
            } else {
               ocs::QConfController controller(os);
               controller.process_request(param, model, *view);
            }
         }
         /* CS-2957: a snapshot may succeed and still report a problem - a list
          * read shows the names it found and reports the ones it did not. The
          * server context decides on the answer list for that reason, see
          * ProcedureModel::make_snapshot(), and this context has to decide the
          * same way or the two would disagree on the exit status. */
         ok = !answer_list_has_error(alpp);
      }
   }

   DRETURN(ok);
}

QConfShowStatus
qconf_show_object(std::ostream &os, ocs::QConfParameterClient &param, const QConfObj o, const char *name,
                  uint32_t *reduction, const bool report_errors) {
   DENTER(TOP_LAYER);

   /* CS-2799: the body of this function is a stored procedure call now. The
    * signature and the status contract stay, because the option blocks above
    * disagree about what a failure means and each keeps its own reaction.
    *
    * The status is recovered from the answer list: the model reports a missing
    * object with STATUS_EEXIST, everything else is a fetch failure. That is the
    * only categorisation that survives the trip back from qmaster, and it is
    * the one the callers distinguish. */
   /* A type without a name field is read as a singleton, because that is the
    * only read it has: the scheduler configuration and the share tree are one
    * object each and no name selects them (CS-2823). Derived from the descriptor
    * rather than passed in, so a caller cannot ask for the kind the type does
    * not have. */
   const ocs::QConfReadKind kind = (qconf_desc(o).name_nm == NoName)
                                   ? ocs::QConfReadKind::SINGLETON
                                   : ocs::QConfReadKind::SINGLE_OBJECT;
   const char *switch_name = ocs::QConfModelBase::switch_for(o, kind);
   DPRINTF("showing object type %d through %s\n", static_cast<int>(o),
           switch_name != nullptr ? switch_name : "<none>");
   if (switch_name == nullptr) {
      DRETURN(QConfShowStatus::GDI_FAILED);
   }

   qconf_op op;
   op.token = switch_name;
   if (name != nullptr) {
      op.args.emplace_back(name);
   }
   param.set_operation(op);

   lList *alp = nullptr;
   const bool ok = qconf_dispatch_procedure(os, param, &alp, reduction);
   if (ok) {
      lFreeList(&alp);
      DRETURN(QConfShowStatus::OK);
   }

   const bool not_found = answer_list_has_status(&alp, STATUS_EEXIST);
   /* Every caller wants to see the reason. The editor path used to suppress a
    * missing object here, because it turned one into an upsert and the GET this
    * path replaced reported no match as an empty list rather than an error - a
    * difference that put an "x is not a calendar" in front of the editor
    * (CS-2822). Since CS-2823 the read answers such a name with the type's
    * template, so a missing object no longer reaches this point for a type that
    * creates one, and for a type that does not the complaint is the answer. */
   if (report_errors) {
      answer_list_output(&alp);
   }
   lFreeList(&alp);
   DRETURN(not_found ? QConfShowStatus::NOT_FOUND : QConfShowStatus::GDI_FAILED);
}

QConfOpResult
qconf_run_read_procedure(std::ostream &os, ocs::QConfParameterClient &param, const qconf_op &op) {
   DENTER(TOP_LAYER);

   const ocs::qconf_read_entry *entry = ocs::QConfModelBase::resolve(op.token.c_str());
   if (entry != nullptr && entry->kind != ocs::QConfReadKind::SINGLE_OBJECT && qconf_missing_argument(op)) {
      DRETURN(QConfOpResult::FAILED);
   }
   if (entry != nullptr && entry->export_only) {
      const char *path = op.args.empty() ? nullptr : op.args.front().c_str();
      const int failed = (entry->kind == ocs::QConfReadKind::SINGLETON)
                         ? qconf_export_singleton(os, param, op.token.c_str(), path)
                         : qconf_export_path(os, param, op.token.c_str(), path);
      DRETURN(failed != 0 ? QConfOpResult::FAILED : QConfOpResult::OK);
   }
   if (entry == nullptr || entry->kind == ocs::QConfReadKind::SINGLE_OBJECT) {
      /* The named single-object reads are not taken here. The plain ones are
       * rows of qconf_sub_procedure_table, which prints the message the way
       * each of them always has; the rows that serve a modify switch belong to
       * the write path; -se resolves the host name first, and -suser and
       * -sconf take a list of names. Every one of them answers non-zero for an
       * object that does not exist (CS-2928). */
      DRETURN(QConfOpResult::NOT_HANDLED);
   }

   param.set_operation(op);
   lList *alp = nullptr;
   const bool ok = qconf_dispatch_procedure(os, param, &alp, nullptr);

   if (!ok) {
      /* CS-2808: a singleton that finds nothing keeps the bare wording its hand
       * written block used - "no sharetree element", not "error: no sharetree
       * element". The prefix comes from the ERROR() macro inside answer_log(),
       * so the text cannot travel as an answer and be printed unchanged. It
       * does not have to: the wording sits in the descriptor table, which is
       * linked into the client as well, so it is written here and the answer
       * carrying the same text is dropped.
       *
       * The model raises STATUS_EEXIST for exactly this case and for nothing
       * else in a singleton read, which is what makes the answer safe to
       * swallow. */
      const qconf_object_desc &desc = qconf_desc(entry->object);
      if (entry->kind == ocs::QConfReadKind::SINGLETON &&
          desc.read.empty_msg != nullptr &&
          answer_list_has_status(&alp, STATUS_EEXIST)) {
         fprintf(stderr, "%s\n", desc.read.empty_msg);
      } else {
         /* the messages the engine used to write to stderr itself; an empty
          * list for plain output is one of them */
         answer_list_output(&alp);
      }
   }
   lFreeList(&alp);

   DRETURN(ok ? QConfOpResult::OK : QConfOpResult::FAILED);
}

/** @brief How a sub-procedure read reports its answer list and exit status
 *
 * Four ways, kept per switch exactly as the hand written blocks had them
 * (CS-2955). Which one a switch uses is the history of its ticket. Since
 * CS-2950 they agree on what a failure does to the command line; whether every
 * message should carry the "error: " prefix is a decision of its own.
 */
enum class QConfAnswerReport {
   SHOW_AND_STATUS,    ///< show_answer() always, its result or-ed in; a failed call sets 1
   SHOW_ON_FAILURE,    ///< a failed call: show_answer(), status 1
   OUTPUT_ON_FAILURE,  ///< a failed call: answer_list_output(), with the "error: " prefix, status 1
   STATUS_FROM_ANSWER  ///< show_answer() always, its result is the status; the call's result is not used
};

/** @brief One read switch served by qconf_run_sub_procedure() */
struct qconf_sub_procedure_entry {
   const char *switch_name;     ///< the switch as typed
   const char *sub_procedure;   ///< the sub-procedure qmaster runs, nullptr for a plain read
   QConfAnswerReport report;    ///< how the answer list and the status are reported
   qconf_names_hook names;      ///< the names to read one by one, nullptr for the argument as typed
   bool blank_line_between;     ///< with names: the outputs are separated by a blank line
};

/** @brief The read switches that are one procedure call each, outside the read table
 *
 * Their output is not one row of qconf_read_table - a table with its own header,
 * a tree, a view over every host group, a filter whose match depends on the kind
 * of entry - so qmaster runs a sub-procedure of its own for each of them
 * (CS-2816 to CS-2820). "-sce" is a read table row of kind SINGLE_OBJECT, which
 * qconf_run_read_procedure() leaves alone; it is here because it prints the bare
 * message where qconf_show_object() would add an "error: " prefix (CS-2817).
 * "-scal", "-sckpt", "-srole", "-sp" and "-sprj" are plain single-object reads
 * as well; they had blocks of their own only because some of them stopped the
 * command line on a missing object and some did not, which CS-2950 made one
 * rule. "-se" and "-suser" are such reads with a names hook: the host resolved
 * first, or each user of a list read on its own with a blank line between them
 * (CS-2967). Otherwise the option table fixes the arguments, so the operation
 * is sent as parsed.
 */
static constexpr qconf_sub_procedure_entry qconf_sub_procedure_table[] = {
   {"-sobjl",          ocs::QConfParameter::OBJECT_FILTER,     QConfAnswerReport::SHOW_AND_STATUS},
   {"-sstnode",        ocs::QConfParameter::SHARETREE_NODES,   QConfAnswerReport::SHOW_AND_STATUS},
   {"-rsstnode",       ocs::QConfParameter::SHARETREE_NODES,   QConfAnswerReport::SHOW_AND_STATUS},
   {"-bonsai",         ocs::QConfParameter::SHARETREE_TREE,    QConfAnswerReport::SHOW_AND_STATUS},
   {"-sst",            ocs::QConfParameter::SHARETREE_TREE,    QConfAnswerReport::SHOW_AND_STATUS},
   {"-sq",             ocs::QConfParameter::CQUEUE_SHOW,       QConfAnswerReport::SHOW_AND_STATUS},
   {"-sds",            ocs::QConfParameter::QUEUE_SICK,        QConfAnswerReport::SHOW_AND_STATUS},
   {"-sc",             ocs::QConfParameter::CENTRY_LIST,       QConfAnswerReport::SHOW_ON_FAILURE},
   {"-scatl",          ocs::QConfParameter::CATEGORY_LIST,     QConfAnswerReport::SHOW_ON_FAILURE},
   {"-sce",            nullptr,                                QConfAnswerReport::SHOW_ON_FAILURE},
   {"-scal",           nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-sckpt",          nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-srole",          nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-sp",             nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-sprj",           nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-se",             nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE, qconf_names_one_host, false},
   {"-suser",          nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE, qconf_names_user_list, true},
   {"-sconf",          nullptr,                                QConfAnswerReport::OUTPUT_ON_FAILURE, qconf_names_conf_strict, true},
   {"-secl",           ocs::QConfParameter::EVENT_CLIENT_LIST, QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-sep",            ocs::QConfParameter::PROCESSOR_LIST,    QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-stl",            ocs::QConfParameter::THREAD_POOL_LIST,  QConfAnswerReport::OUTPUT_ON_FAILURE},
   {"-shgrp",          ocs::QConfParameter::HGROUP_SHOW,       QConfAnswerReport::STATUS_FROM_ANSWER},
   {"-shgrp_tree",     ocs::QConfParameter::HGROUP_TREE,       QConfAnswerReport::STATUS_FROM_ANSWER},
   {"-shgrp_resolved", ocs::QConfParameter::HGROUP_RESOLVED,   QConfAnswerReport::STATUS_FROM_ANSWER},
   {"-shgrp_why",      ocs::QConfParameter::HGROUP_WHY,        QConfAnswerReport::STATUS_FROM_ANSWER},
};

/** @brief Send one read of qconf_sub_procedure_table and report it the way its row says
 * @param os    the stream the output is written to
 * @param param the invocation, whose operation is set to @p op for the call
 * @param entry the row
 * @param op    the operation to send
 * @return true when the read failed
 */
static bool
qconf_sub_procedure_once(std::ostream &os, ocs::QConfParameterClient &param, const qconf_sub_procedure_entry &entry,
                         const qconf_op &op) {
   DENTER(TOP_LAYER);

   param.set_operation(op);
   if (entry.sub_procedure != nullptr) {
      param.set_sub_procedure_name(entry.sub_procedure);
   }
   lList *alp = nullptr;
   const bool ok = qconf_dispatch_procedure(os, param, &alp);
   param.set_sub_procedure_name("");

   bool failed = false;
   switch (entry.report) {
      case QConfAnswerReport::SHOW_AND_STATUS:
         failed = (show_answer(alp) != 0) || !ok;
         break;
      case QConfAnswerReport::SHOW_ON_FAILURE:
         if (!ok) {
            show_answer(alp);
            failed = true;
         }
         break;
      case QConfAnswerReport::OUTPUT_ON_FAILURE:
         if (!ok) {
            answer_list_output(&alp);
            failed = true;
         }
         break;
      case QConfAnswerReport::STATUS_FROM_ANSWER:
         failed = (show_answer(alp) != 0);
         break;
   }
   lFreeList(&alp);

   DRETURN(failed);
}

QConfOpResult
qconf_run_sub_procedure(std::ostream &os, ocs::QConfParameterClient &param, const qconf_op &op) {
   DENTER(TOP_LAYER);

   const qconf_sub_procedure_entry *entry = nullptr;
   for (const qconf_sub_procedure_entry &e : qconf_sub_procedure_table) {
      if (op.token == e.switch_name) {
         entry = &e;
         break;
      }
   }
   if (entry == nullptr) {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   if (qconf_missing_argument(op)) {
      DRETURN(QConfOpResult::FAILED);
   }

   if (entry->names == nullptr) {
      DRETURN(qconf_sub_procedure_once(os, param, *entry, op) ? QConfOpResult::FAILED : QConfOpResult::OK);
   }

   /* one read per name; a name that fails does not stop the others */
   std::vector<std::string> names;
   bool failed = false;
   entry->names(op, names, failed);
   bool first = true;
   for (const std::string &name : names) {
      if (entry->blank_line_between && !first) {
         os << "\n";
      }
      first = false;
      qconf_op one;
      one.spec = op.spec;
      one.token = op.token;
      one.args.push_back(name);
      if (qconf_sub_procedure_once(os, param, *entry, one)) {
         failed = true;
      }
   }
   DRETURN(failed ? QConfOpResult::FAILED : QConfOpResult::OK);
}
