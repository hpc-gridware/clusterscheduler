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
 * @brief qacct - reports accounting information for finished jobs
 */
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <grp.h>
#include <ctime>
#include <fnmatch.h>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "rapidjson/document.h"
#include "rapidjson/internal/dtoa.h"

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_QAJ.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "tom/ocs_Sort.h"

#include "comm/commlib.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "sched/sge_select_queue.h"

#include "sge.h"
#include "sig_handlers.h"
#include "execution_states.h"
#include "sge_rusage.h"
#include <cinttypes>
#include "msg_common.h"
#include "msg_history.h"
#include "msg_qacct.h"

/** @brief The width of each column of the grouped output
 *
 * `qacct -g`, `-o`, `-q` and so on group the records and print one line per
 * group, with the grouping key in the first column. The width of that column
 * depends on the longest value actually present, so the whole file is read
 * once to measure before anything is printed.
 */
typedef struct {
   size_t host;         ///< widest host name seen
   size_t queue;        ///< widest queue name seen
   size_t group;        ///< widest group name seen
   size_t owner;        ///< widest user name seen
   size_t project;      ///< widest project name seen
   size_t department;   ///< widest department name seen
   size_t granted_pe;   ///< widest parallel environment name seen
   size_t slots;        ///< widest slot count seen
   size_t arid;         ///< widest advance reservation id seen
} sge_qacct_columns;

/** @brief Everything the command line asked for, in one place
 *
 * Most options come in pairs: a value and a `*flag`. The flag is what says the
 * option was given at all, because for several of them the *absence* of a value
 * is meaningful - `-o` with no user means "group by owner", `-o fred` means
 * "only fred". A null value with the flag set therefore cannot be told from an
 * option that was never given, unless the flag is kept separately.
 */
typedef struct {
   char *group;                ///< `-g`: only this group, or the grouping key when empty
   char *host;                 ///< `-h`: only this execution host, or the grouping key when empty
   const char *owner;          ///< `-o`: only this user, or the grouping key when empty
   const char *job_name;       ///< `-j`: only jobs with this name
   const char *project;        ///< `-P`: only this project, or the grouping key when empty
   const char *department;     ///< `-D`: only this department, or the grouping key when empty
   const char *account;        ///< `-A`: only jobs with this account string
   const char *granted_pe;     ///< `-pe`: only jobs that ran in this parallel environment
   const char *complexes;      ///< `-l`: only jobs whose request matches this resource list
   uint32_t job_number;        ///< `-j`: only this job id
   uint32_t slots;             ///< `-slots`: only jobs granted this many slots
   uint32_t ar_number;         ///< `-ar`: only jobs that ran under this advance reservation
   int jobflag;                ///< `-j` was given
   int ownerflag;              ///< `-o` was given
   int groupflag;              ///< `-g` was given
   int jobfound;               ///< at least one record matched the requested job
   int complexflag;            ///< `-l` was given
   int queueflag;              ///< `-q` was given
   int projectflag;            ///< `-P` was given
   int departmentflag;         ///< `-D` was given
   int hostflag;               ///< `-h` was given
   int accountflag;            ///< `-A` was given
   int granted_peflag;         ///< `-pe` was given
   int slotsflag;              ///< `-slots` was given
   int arflag;                 ///< `-ar` was given
   uint32_t taskstart;         ///< `-t`: first array task of the range to report
   uint32_t taskend;           ///< `-t`: last array task of the range to report
   uint32_t taskstep;          ///< `-t`: step of that range
   uint64_t begin_time;        ///< `-b`: ignore records that ended before this
   uint64_t end_time;          ///< `-e`: ignore records that ended after this
   lList *queue_name_list;     ///< `-q`: the queues to report on (`QR_Type`)
} sge_qacct_options;

static void qacct_usage(FILE *err_fp);
static void print_full(int length, const char* string);
static void print_full_ulong(int length, uint32_t value);
static void calc_column_sizes(ocs::tom::ConstElement<ocs::tom::QAJ> ep, sge_qacct_columns *column_size_data);
static void showjob(sge_rusage_type *dusage);
static bool get_qacct_lists(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ListPtr<ocs::tom::CE> *ppcentries,
                            ocs::tom::ListPtr<ocs::tom::CQ> *ppqueues, ocs::tom::ListPtr<ocs::tom::EH> *ppexechosts,
                            ocs::tom::ListPtr<ocs::tom::HGRP> *hgrp_l);
static void free_qacct_lists(ocs::tom::ListPtr<ocs::tom::CE> &ppcentries, ocs::tom::ListPtr<ocs::tom::CQ> &ppqueues,
                             ocs::tom::ListPtr<ocs::tom::EH> &ppexechosts, ocs::tom::ListPtr<ocs::tom::HGRP> &hgrp_l);
static int sge_read_rusage(std::ifstream &acct_file_stream, sge_rusage_type *d, sge_qacct_options *options,
                           std::string &acct_file_line);

/**
 * @brief Main routine of the qacct client
 *
 * Uses the globals fp, path, me and prognames.
 *
 * @param argc number of command line arguments
 * @param argv the command line arguments
 * @return 0 if ok, -1 on an invalid command line, < -1 on other errors
 */
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qacct");

   int ret = 0;
   uint32_t days;
   sge_qacct_columns column_sizes;
   int beginflag=0;
   int endflag=0;
   int daysflag=0;
   bool summary_view = false;
   int ignored_jobs = 0;             /* counter of ignored jobs for accounting */

   sge_qacct_options options;

   sge_rusage_type dusage{}; // zeroed here, the exit code frees its other_usage
   sge_rusage_type totals;
   int ii;
   ocs::tom::ListPtr<ocs::tom::CE> complex_options;
   ocs::tom::ListPtr<ocs::tom::CE> centry_list;
   ocs::tom::ListPtr<ocs::tom::CQ> queue_list;
   ocs::tom::ListPtr<ocs::tom::EH> exechost_list;
   ocs::tom::ListPtr<ocs::tom::HGRP> hgrp_list;
   ocs::tom::ListPtr<ocs::tom::QR> queueref_list;
   ocs::tom::ListPtr<ocs::tom::QAJ> sorted_list;
   ocs::tom::SortOrder<ocs::tom::QAJ> sort_order;
   int is_path_setup = 0;
   uint32_t line = 0;
   const char *acct_file = nullptr;
   std::string filename{};
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   std::ifstream acct_file_stream;
   std::string acct_file_line{};

   sge_setup_sig_handlers(QACCT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QACCT, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      goto QACCT_EXIT;
   }

   memset(&totals, 0, sizeof(totals));
   memset(&options, 0, sizeof(options));
   options.begin_time = std::numeric_limits<uint64_t>::max();
   options.end_time = std::numeric_limits<uint64_t>::max();

   column_sizes.host       = strlen(MSG_HISTORY_HOST)+1;
   column_sizes.queue      = strlen(MSG_HISTORY_QUEUE)+1;
   column_sizes.group      = strlen(MSG_HISTORY_GROUP)+1;
   column_sizes.owner      = strlen(MSG_HISTORY_OWNER)+1;
   column_sizes.project    = strlen(MSG_HISTORY_PROJECT)+1;
   column_sizes.department = strlen(MSG_HISTORY_DEPARTMENT)+1;
   column_sizes.granted_pe = strlen(MSG_HISTORY_PE)+1;
   column_sizes.slots      = 5;
   column_sizes.arid       = 5;

   /*
   ** Read in the command line arguments.
   */
   for (ii = 1; ii < argc; ii++) {
      /*
      ** owner
      */
      if (!strcmp("-o", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1]) == '-') {
               options.ownerflag = 1;
            } else {
               options.owner = argv[++ii];
            }
         } else {
            options.ownerflag = 1;
         }
      }
      /*
      ** group
      */
      else if (!strcmp("-g", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1]) == '-') {
               options.groupflag = 1;
            } else {
               uint32_t gid;
               stringT buffer;

               if (sscanf(argv[++ii], sge_u32, &gid) == 1) {
                  if (sge_gid2group((gid_t)gid, buffer,
                                   MAX_STRING_SIZE, MAX_NIS_RETRIES) != 0) {
                     options.group = sge_strdup(options.group, argv[ii]);
                  } else {
                     options.group = sge_strdup(options.group, buffer);
                  }
               } else {
                  options.group = sge_strdup(options.group, argv[ii]);
               }
            }
         } else {
            options.groupflag = 1;
         }
      }
      /*
      ** queue
      */
      else if (!strcmp("-q", argv[ii])) {
         options.queueflag = 1;
         if (argv[ii+1]) {
            if (*(argv[ii+1]) != '-') {
               options.hostflag = 1;
               // created with the first queue, as the free emplace_back() created it
               if (!queueref_list) {
                  queueref_list = ocs::tom::ListPtr<ocs::tom::QR>::create("");
               }
               queueref_list->emplace_back<QR_name>(argv[++ii]);
            }
         }
      }
      /*
      ** host
      */
      else if (!strcmp("-h", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1])=='-') {
               options.hostflag = 1;
            } else {
               char unique[CL_MAXHOSTNAMELEN + 1];
               ocs::tom::ListPtr<ocs::tom::AN> answer_list;
               ocs::gdi::ClientBase::prepare_enroll(&answer_list);
               if (getuniquehostname(argv[++ii], unique, 0) != CL_RETVAL_OK) {
                   /*
                    * we can't resolve the hostname, but that's no drama for qacct.
                    * maybe the hostname is no longer active but the usage information
                    * is already available
                    */
                  options.host = sge_strdup(options.host, argv[ii]);
               } else {
                  options.host = sge_strdup(options.host, unique);
               }
            }
         } else {
            options.hostflag = 1;
         }
      }
      /*
      ** job
      */
      else if (!strcmp("-j", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1])=='-') {
               options.jobflag = 1;
            } else if (sscanf(argv[++ii], sge_u32, &options.job_number) != 1) {
               options.job_number = 0;
               options.job_name = argv[ii];
            }
         } else {
            options.jobflag = 1;
         }
      }
      /*
      ** task id
      */
      else if (!strcmp("-t", argv[ii])) {
         if (!argv[ii+1] || *(argv[ii+1])=='-') {
            fprintf(stderr, "%s\n", MSG_HISTORY_TOPTIONMUSTHAVELISTOFTASKIDRANGES );
            qacct_usage(stderr);
            DRETURN(1);
         } else {
            ocs::tom::ListPtr<ocs::tom::RN> task_id_ranges;
            ocs::tom::ListPtr<ocs::tom::AN> answer;

            ii++;
            range_list_parse_from_string(task_id_ranges, &answer, argv[ii], false, true, INF_NOT_ALLOWED);
            if (task_id_ranges == nullptr) {
               answer.reset();
               fprintf(stderr, MSG_HISTORY_INVALIDLISTOFTASKIDRANGES_S , argv[ii]);
               fprintf(stderr, "\n");
               qacct_usage(stderr);
               DRETURN(1);
            }
            const auto first_range = task_id_ranges->front();
            options.taskstart = ocs::tom::get<RN_min>(first_range);
            options.taskend = ocs::tom::get<RN_max>(first_range);
            options.taskstep = ocs::tom::get<RN_step>(first_range);
            if (options.taskstep == 0) {
               options.taskstep = 1;
            }
         }
      }
      /*
      ** time options
      ** begin time
      */
      else if (!strcmp("-b", argv[ii])) {
         if (argv[ii+1]) {
            uint32_t tmp_begin_time;

            if  (!ulong_parse_date_time_from_string(&tmp_begin_time, nullptr, argv[++ii])) {
               /*
               ** problem: insufficient error reporting
               */
               qacct_usage(stderr);
            }
            options.begin_time = sge_gmt32_to_gmt64(tmp_begin_time);
            DPRINTF("begin is: " sge_u64 "\n", options.begin_time);
            beginflag = 1;
         } else {
            qacct_usage(stderr);
         }
      }
      /*
      ** end time
      */
      else if (!strcmp("-e", argv[ii])) {
         if (argv[ii+1]) {
            uint32_t tmp_end_time;

            if  (!ulong_parse_date_time_from_string(&tmp_end_time, nullptr, argv[++ii])) {
               /*
               ** problem: insufficient error reporting
               */
               qacct_usage(stderr);
            }
            options.end_time = sge_gmt32_to_gmt64(tmp_end_time);
            DPRINTF("end is: " sge_u64 "\n", options.end_time);
            endflag = 1;
         } else {
            qacct_usage(stderr);
         }
      }
      /*
      ** days
      */
      else if (!strcmp("-d", argv[ii])) {
         if (argv[ii+1]) {
            if (sscanf(argv[++ii], sge_u32, &days) != 1) {
               /*
               ** problem: insufficient error reporting
               */
               qacct_usage(stderr);
            }
            DPRINTF("days is: %d\n", days);
            daysflag = 1;
         } else {
            qacct_usage(stderr);
         }
      }
      /*
      ** project
      */
      else if (!strcmp("-P", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1]) == '-') {
               options.projectflag = 1;
            } else {
               options.project = argv[++ii];
            }
         } else {
            options.projectflag = 1;
         }
      }
      /*
      ** department
      */
      else if (!strcmp("-D", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1]) == '-') {
               options.departmentflag = 1;
            } else {
               options.department = argv[++ii];
            }
         } else {
            options.departmentflag = 1;
         }
      } else if (!strcmp("-pe", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1]) == '-') {
               options.granted_peflag = 1;
            } else {
               options.granted_pe = argv[++ii];
            }
         } else {
            options.granted_peflag = 1;
         }
      } else if (!strcmp("-slots", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1]) == '-') {
               options.slotsflag = 1;
            } else {
               options.slots = SGE_STRTOU_LONG32(argv[++ii]);
            }
         } else {
            options.slotsflag = 1;
         }
      }
      /*
      ** advance reservation
      */
      else if (!strcmp("-ar", argv[ii])) {
         if (argv[ii+1]) {
            if (*(argv[ii+1])=='-') {
               options.arflag = 1;
            } else {
               if (sscanf(argv[++ii], sge_u32, &options.ar_number) != 1) {
                  fprintf(stderr, "%s\n", MSG_PARSE_INVALID_AR_MUSTBEUINT);
                  qacct_usage(stderr);
                  DRETURN(1);
               }
            }
         } else {
            options.arflag = 1;
         }
      }
      /*
      ** complex attributes
      ** option syntax is described as
      ** -l attr[=value],...
      */
      else if (!strcmp("-l",argv[ii])) {
         if (argv[ii+1]) {
            /*
            ** add blank cause no range can be specified
            ** as described in sge_resource.c
            */
            options.complexes = argv[++ii];
            options.complexflag = 1;
         } else {
            qacct_usage(stderr);
         }
      }
      /*
      ** alternative accounting file
      */
      else if (!strcmp("-f",argv[ii])) {
         if (argv[ii+1]) {
            acct_file = argv[++ii];
         } else {
            qacct_usage(stderr);
         }
      }
      /*
      ** -A account
      */
      else if (!strcmp("-A",argv[ii])) {
         if (argv[ii+1]) {
            options.account = argv[++ii];
            options.accountflag = 1;
         } else {
            qacct_usage(stderr);
         }
      } else if (!strcmp("-help",argv[ii])) {
         qacct_usage(stdout);
      } else {
         qacct_usage(stderr);
      }
   } /* end for */

   /*
   ** Note that this has to be a file on a local disk or a nfs
   ** mounted directory.
   */
   if (acct_file == nullptr) {
      // no filename given with the -f switch
      // look for accounting.jsonl first, if it doesn't exist, look for accounting
      filename = bootstrap_get_acct_file();
      filename += ".jsonl";
      if (std::filesystem::exists(filename)) {
         acct_file = filename.c_str();
      } else {
         acct_file = bootstrap_get_acct_file();
         if (!std::filesystem::exists(acct_file)) {
            printf("%s\n", MSG_HISTORY_NOJOBSRUNNINGSINCESTARTUP);
            goto QACCT_EXIT_BUT_NO_ERROR;
         }
      }
   } else {
      if (!std::filesystem::exists(acct_file)) {
         perror(acct_file);
         goto QACCT_EXIT;
      }
   }

   DPRINTF("acct_file: %s\n", (acct_file ? acct_file : "(nullptr)"));

   is_path_setup = 1;

   /*
   ** evaluation time period
   ** begin and end are evaluated later, so these
   ** are the ones that have to be set to default
   ** values
   */
   /*
   ** problem: if all 3 options are set, what do?
   ** at the moment, ignore days, so nothing
   ** has to be done here in this case
   */
   if (!endflag) {
      if (daysflag && beginflag) {
         options.end_time = options.begin_time + sge_gmt32_to_gmt64(days*24*3600);
      } else {
         options.end_time = std::numeric_limits<uint64_t>::max();
      }
   }
   if (!beginflag) {
      if (endflag && daysflag) {
         options.begin_time = options.end_time - sge_gmt32_to_gmt64(days*24*3600);
      } else if (daysflag) {
         options.begin_time = sge_get_gmt64() - sge_gmt32_to_gmt64(days*24*3600);
      } else {
         options.begin_time = std::numeric_limits<uint64_t>::max();
      }
   }

   if (DPRINTF_IS_ACTIVE) {
      DSTRING_STATIC(dstr, 64);
      DPRINTF(" begin_time: %s\n", sge_ctime64(options.begin_time, &dstr));
      DPRINTF(" end_time:   %s\n", sge_ctime64(options.end_time, &dstr));
   }

   {
      dstring cqueue_name = DSTRING_INIT;
      dstring host_or_hgroup = DSTRING_INIT;
      const char *name = nullptr;
      bool has_hostname = false;
      bool has_domain = true;

      for (const auto qref_pattern : *queueref_list) {
         name = ocs::tom::get<QR_name>(qref_pattern);
         cqueue_name_split(name, &cqueue_name, &host_or_hgroup,
                           &has_hostname, &has_domain);
         if (has_domain) {
            break;
         }
      }
      sge_dstring_free(&cqueue_name);
      sge_dstring_free(&host_or_hgroup);

      /* the user did not specify a queue domain, therefor we need no information
         from the qmaster, but we have to work on the user input and generate the
         same data structure, that we would have gotten with the qmaster functions*/
      if (!has_domain) {
         for (const auto qref_pattern : *queueref_list) {
            dstring qi_name = DSTRING_INIT;
            const char *tmp_str = nullptr;
            name = ocs::tom::get<QR_name>(qref_pattern);

            sge_dstring_copy_string(&qi_name, name);

            if ((tmp_str = strchr(name, '@')) == nullptr){
               sge_dstring_append(&qi_name, "@*");
            } else if (*(tmp_str+1) == '\0'){
               sge_dstring_append(&qi_name, "*");
            }

            ocs::tom::emplace_back<QR_name>(&options.queue_name_list, sge_dstring_get_string(&qi_name));

            sge_dstring_free(&qi_name);
         }
      }
      if (options.complexflag || (queueref_list && has_domain)) {
         /*
         ** parsing complex flags and initialising complex list
         */
         bool found_something;

         complex_options = centry_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::CE>(), options.complexes, true);
         if (!complex_options) {
            /*
            ** problem: still to tell some more to the user
            */
            qacct_usage(stderr);
         }
         /* lDumpList(stdout, complex_options, 0); */
         if (!is_path_setup) {
            ocs::gdi::ClientBase::prepare_enroll(&alp);
            ocs::gdi::ClientBase::gdi_get_act_master_host(true);
            if (ocs::gdi::ClientBase::gdi_is_alive(&alp) != CL_RETVAL_OK) {
               answer_list_output(&alp);
               goto QACCT_EXIT;
            }
            is_path_setup = 1;
         }
         if (queueref_list && has_domain){
            if (!get_qacct_lists(&alp, nullptr, &queue_list, nullptr, &hgrp_list)) {
               answer_list_output(&alp);
               goto QACCT_EXIT;
            }

            // qref_list_resolve() has no typed form for the destination list yet; it is a member of the
            // memset options struct
            qref_list_resolve(*queueref_list, nullptr, &options.queue_name_list,
                           &found_something, *queue_list, *hgrp_list, true, true);
            if (!found_something) {
               fprintf(stderr, "%s\n", MSG_QINSTANCE_NOQUEUES);
               goto QACCT_EXIT;
            }
         }
         if (options.complexflag) {
            if (!get_qacct_lists(&alp, &centry_list, &queue_list, &exechost_list, nullptr)) {
               answer_list_output(&alp);
               goto QACCT_EXIT;
            }
         }
      } /* endif complexflag */
   }

   /* debug output) */
   if (getenv("SGE_QACCT_DEBUG")) {
      printf("complex entries:\n");
      // print() throws on an empty handle where lWriteListTo() printed nothing
      if (centry_list) {
         centry_list->print(stdout);
      }
      printf("queue instances:\n");
      if (queue_list) {
         queue_list->print(stdout);
      }
      printf("exec hosts:\n");
      if (exechost_list) {
         exechost_list->print(stdout);
      }
      printf("host groups\n");
      if (hgrp_list) {
         hgrp_list->print(stdout);
      }
   }

   acct_file_stream.open(acct_file);
   if (!acct_file_stream.is_open()) {
      ERROR(MSG_HISTORY_ERRORUNABLETOOPENX_S ,acct_file);
      printf("%s\n", MSG_HISTORY_NOJOBSRUNNINGSINCESTARTUP);

      // file exists but cannot be opened -> error
      goto QACCT_EXIT;
   }

   totals.ru_wallclock = 0;
   totals.ru_utime =  0;
   totals.ru_stime = 0;
   totals.cpu = 0;
   totals.mem = 0;
   totals.io = 0;
   totals.iow = 0;

   if (options.hostflag || options.queueflag || options.groupflag || options.ownerflag || options.projectflag ||
       options.departmentflag || options.granted_peflag || options.slotsflag || options.arflag) {
      sorted_list = ocs::tom::ListPtr<ocs::tom::QAJ>::create("sorted_list");
      sort_order = ocs::tom::sort_order<ocs::tom::asc<QAJ_host>, ocs::tom::asc<QAJ_queue>, ocs::tom::asc<QAJ_group>,
                                        ocs::tom::asc<QAJ_owner>, ocs::tom::asc<QAJ_project>,
                                        ocs::tom::asc<QAJ_department>, ocs::tom::asc<QAJ_granted_pe>,
                                        ocs::tom::asc<QAJ_slots>, ocs::tom::asc<QAJ_arid>>();
      summary_view = true;
      if (!sorted_list || !sort_order) {
         ERROR(SFNMAX, MSG_HISTORY_NOTENOUGTHMEMORYTOCREATELIST);
         goto QACCT_EXIT;
      }
   }

   /* main loop */
   while (!shut_me_down) {
      int i_ret;
      line++;

      i_ret = sge_read_rusage(acct_file_stream, &dusage, &options, acct_file_line);
      if (i_ret == -2) {
         /* ignore, the line just doesn't match the command options */
         continue;
      } else if (i_ret > 0) {
	      break;
      } else if (i_ret < 0) {
	      ERROR(MSG_HISTORY_IGNORINGINVALIDENTRYINLINEX_U , line);
	      continue;
      }

      if (options.complexflag) {
         dstring qi = DSTRING_INIT;
         int selected;

         sge_dstring_sprintf(&qi,"%s@%s", dusage.qname, dusage.hostname );
         const auto queue = cqueue_list_locate_qinstance_msg(*queue_list, sge_dstring_get_string(&qi), false);
         sge_dstring_free(&qi);

         if (!queue) {
            /*
            * queue no longer exists, we can't get the complex attributes for this job,
            * we will ignore the job for accounting  and count the number of ignored jobs
            */
            ignored_jobs++;
            continue;
         }

         sconf_set_qs_state(QS_STATE_EMPTY);

         if (centry_list_fill_request(*complex_options, &alp, *centry_list, true, true, false)) {
            answer_list_output(&alp);
            goto QACCT_EXIT;
         }

         selected = sge_select_queue(*complex_options, queue, ocs::tom::ConstElement<ocs::tom::EH>(), *exechost_list,
                    *centry_list, true, 1, ocs::tom::ConstList<ocs::tom::ST>(), ocs::tom::ConstList<ocs::tom::US>(),
                    ocs::tom::ConstElement<ocs::tom::JB>());

         if (!selected) {
            continue;
         }
      } /* endif complexflag */

      if (options.jobflag || options.job_number || options.job_name != nullptr) {
         showjob(&dusage);
      }

      totals.ru_wallclock += dusage.ru_wallclock;
      totals.ru_utime  += dusage.ru_utime;
      totals.ru_stime  += dusage.ru_stime;
      totals.cpu += dusage.cpu;
      totals.mem += dusage.mem;
      totals.io  += dusage.io;
      totals.iow  += dusage.iow;

      /*
      ** collect information for sorted output and sums
      */
      if (summary_view) {
         auto ep = sorted_list->front();

         /*
         ** find either the correct place to insert the next element
         ** or the existing element to increase
         */

         while (ep && ((options.slotsflag && (ocs::tom::get<QAJ_slots>(ep) != dusage.slots)) ||
                (options.granted_peflag && (sge_strnullcmp(ocs::tom::get<QAJ_granted_pe>(ep), dusage.granted_pe))) ||
                (options.departmentflag && (sge_strnullcmp(ocs::tom::get<QAJ_department>(ep), dusage.department))) ||
                (options.projectflag && (sge_strnullcmp(ocs::tom::get<QAJ_project>(ep), dusage.project))) ||
                (options.ownerflag && (sge_strnullcmp(ocs::tom::get<QAJ_owner>(ep), dusage.owner))) ||
                (options.hostflag && (sge_hostcmp(ocs::tom::get<QAJ_host>(ep), dusage.hostname))) ||
                (options.queueflag && (sge_strnullcmp(ocs::tom::get<QAJ_queue>(ep), dusage.qname))) ||
                (options.groupflag && (sge_strnullcmp(ocs::tom::get<QAJ_group>(ep) , dusage.group))) ||
                (options.arflag && (ocs::tom::get<QAJ_arid>(ep) != dusage.ar))
                )){
             ep = ep.next();
         }
         /*
         ** is this now the element that we want
         ** or do we have to insert one?
         */
         if (ep != nullptr) {

            DPRINTF("found element h:%s - q:%s - g:%s - o:%s - p:%s - d:%s - pe:%s - slots:%d - ar:%d\n", \
                    (dusage.hostname ? dusage.hostname : "(nullptr)"), \
                    (dusage.qname ? dusage.qname : "(nullptr)"), \
                    (dusage.group ? dusage.group : "(nullptr)"), \
                    (dusage.owner ? dusage.owner : "(nullptr)"), \
                    (dusage.project ? dusage.project : "(nullptr)"), \
                    (dusage.department ? dusage.department : "(nullptr)"), \
                    (dusage.granted_pe ? dusage.granted_pe : "(nullptr)"), \
                    (int) dusage.slots, \
                    (int) dusage.ar);

            ocs::tom::add<QAJ_ru_wallclock>(ep, dusage.ru_wallclock);

            ocs::tom::add<QAJ_ru_utime>(ep, dusage.ru_utime);
            ocs::tom::add<QAJ_ru_stime>(ep, dusage.ru_stime);
            ocs::tom::add<QAJ_cpu>(ep, dusage.cpu);
            ocs::tom::add<QAJ_mem>(ep, dusage.mem);
            ocs::tom::add<QAJ_io>(ep, dusage.io);
            ocs::tom::add<QAJ_iow>(ep, dusage.iow);
         } else {
            auto new_ep = ocs::tom::ElementPtr<ocs::tom::QAJ>::create();

            if (options.hostflag && dusage.hostname)
               ocs::tom::set<QAJ_host>(new_ep, dusage.hostname);
            if (options.queueflag && dusage.qname)
               ocs::tom::set<QAJ_queue>(new_ep, dusage.qname);
            if (options.groupflag && dusage.group)
               ocs::tom::set<QAJ_group>(new_ep, dusage.group);
            if (options.ownerflag && dusage.owner)
               ocs::tom::set<QAJ_owner>(new_ep, dusage.owner);
            if (options.projectflag && dusage.project)
               ocs::tom::set<QAJ_project>(new_ep, dusage.project);
            if (options.departmentflag && dusage.department)
               ocs::tom::set<QAJ_department>(new_ep, dusage.department);
            if (options.granted_peflag && dusage.granted_pe)
               ocs::tom::set<QAJ_granted_pe>(new_ep, dusage.granted_pe);
            if (options.slotsflag)
               ocs::tom::set<QAJ_slots>(new_ep, dusage.slots);
            if (options.arflag)
               ocs::tom::set<QAJ_arid>(new_ep, dusage.ar);

            ocs::tom::set<QAJ_ru_wallclock>(new_ep, dusage.ru_wallclock);
            ocs::tom::set<QAJ_ru_utime>(new_ep, dusage.ru_utime);
            ocs::tom::set<QAJ_ru_stime>(new_ep, dusage.ru_stime);
            ocs::tom::set<QAJ_cpu>(new_ep, dusage.cpu);
            ocs::tom::set<QAJ_mem>(new_ep, dusage.mem);
            ocs::tom::set<QAJ_io>(new_ep,  dusage.io);
            ocs::tom::set<QAJ_iow>(new_ep, dusage.iow);

            sorted_list->insert_sorted(sort_order, std::move(new_ep));
         }
      } /* endif sortflags */
   } /* end while sge_read_rusage */

   /*
   * print the warning about the count of ignored jobs for accounting
   */
   if (ignored_jobs > 0) {
      WARNING(MSG_HISTORY_IGNORINGJOBXFORACCOUNTINGMASTERQUEUEYNOTEXISTS_IS, ignored_jobs);
      printf("\n");
   }

   /*
   ** exit routine attempts to close file if not nullptr
   */
   acct_file_stream.close();

   if (shut_me_down) {
      printf("%s\n", MSG_USER_ABORT);
      goto QACCT_EXIT;
   }
   if (options.job_number || options.job_name != nullptr) {
      if (!options.jobfound) {
         if (options.job_number) {
            if (options.taskstart && options.taskend && options.taskstep) {
               ERROR(MSG_HISTORY_JOBARRAYTASKSWXYZNOTFOUND_UUUU, options.job_number, options.taskstart, options.taskend, options.taskstep);
            } else {
               ERROR(MSG_HISTORY_JOBIDXNOTFOUND_U, options.job_number);
            }
         } else {
            if (options.taskstart && options.taskend && options.taskstep) {
               ERROR(MSG_HISTORY_JOBARRAYTASKSWXYZNOTFOUND_SUUU, options.job_name, options.taskstart, options.taskend, options.taskstep);
            } else {
               ERROR(MSG_HISTORY_JOBNAMEXNOTFOUND_S, options.job_name);
            }
         }
         goto QACCT_EXIT;
      } else {
         free_qacct_lists(centry_list, queue_list, exechost_list, hgrp_list);
         sge_exit(0);
      }
   } else if (options.taskstart && options.taskend && options.taskstep) {
      ERROR(SFNMAX, MSG_HISTORY_TOPTIONREQUIRESJOPTION);
      qacct_usage(stderr);
      free_qacct_lists(centry_list, queue_list, exechost_list, hgrp_list);
      sge_exit(0);
   }

   /*
   ** assorted output of statistics
   */
   if (options.host != nullptr) {
      column_sizes.host = strlen(options.host) + 1;
   }
   if (options.group != nullptr) {
      column_sizes.group = strlen(options.group) + 1;
   }
   if (options.owner != nullptr) {
      column_sizes.owner = strlen(options.owner) + 1;
   }
   if (options.project != nullptr) {
      column_sizes.project = strlen(options.project) + 1;
   }
   if (options.department != nullptr) {
      column_sizes.department = strlen(options.department) + 1;
   }
   if (options.granted_pe != nullptr) {
      column_sizes.granted_pe = strlen(options.granted_pe) + 1;
   }

   calc_column_sizes(sorted_list->front(), &column_sizes);
   {
      ocs::tom::ConstElement<ocs::tom::QAJ> ep;
      int dashcnt = 0;
      char title_array[500];

      if (options.host != nullptr || options.hostflag) {
         print_full(column_sizes.host , MSG_HISTORY_HOST);
         dashcnt += column_sizes.host ;
      }
      if (options.queueflag) {
         print_full(column_sizes.queue ,MSG_HISTORY_QUEUE );
         dashcnt += column_sizes.queue ;
      }
      if (options.group != nullptr || options.groupflag) {
         print_full(column_sizes.group , MSG_HISTORY_GROUP);
         dashcnt += column_sizes.group ;
      }
      if (options.owner != nullptr || options.ownerflag) {
         print_full(column_sizes.owner ,MSG_HISTORY_OWNER );
         dashcnt += column_sizes.owner ;
      }
      if (options.project != nullptr || options.projectflag) {
         print_full(column_sizes.project, MSG_HISTORY_PROJECT);
         dashcnt += column_sizes.project ;
      }
      if (options.department != nullptr || options.departmentflag) {
         print_full(column_sizes.department, MSG_HISTORY_DEPARTMENT);
         dashcnt += column_sizes.department;
      }
      if (options.granted_pe != nullptr || options.granted_peflag) {
         print_full(column_sizes.granted_pe, MSG_HISTORY_PE);
         dashcnt += column_sizes.granted_pe;
      }
      if (options.slots > 0 || options.slotsflag) {
         print_full(column_sizes.slots, MSG_HISTORY_SLOTS);
         dashcnt += column_sizes.slots;
      }
      if (options.ar_number > 0 || options.arflag) {
         print_full(column_sizes.slots, MSG_HISTORY_AR);
         dashcnt += column_sizes.slots;
      }

      if (!dashcnt) {
         printf("%s\n", MSG_HISTORY_TOTSYSTEMUSAGE);
      }

      snprintf(title_array, sizeof(title_array), "%13.13s %13.13s %13.13s %13.13s %18.18s %18.18s %18.18s",
               "WALLCLOCK", "UTIME", "STIME", "CPU", "MEMORY", "IO", "IOW");

      printf("%s\n", title_array);

      dashcnt += strlen(title_array);
      for (ii=0; ii < dashcnt; ii++) {
         printf("=");
      }
      printf("\n");

      if (summary_view) {
         ep = sorted_list->front();
      }

      while (totals.ru_wallclock) {
         const char *cp;

         if (options.host != nullptr) {
            print_full(column_sizes.host,  options.host);
         } else if (options.hostflag) {
            if (ep == nullptr) {
               break;
            }
            /*
            ** if file has empty fields and parsing results in nullptr
            ** then we have a nullptr list entry here
            ** we can't ignore it because it was a line in the
            ** accounting file
            */
            print_full(column_sizes.host, ((cp = ocs::tom::get<QAJ_host>(ep)) ? cp : ""));
         }
         if (options.queueflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.queue, ((cp = ocs::tom::get<QAJ_queue>(ep)) ? cp : ""));
         }
         if (options.group != nullptr) {
            print_full(column_sizes.group, options.group);
         } else if (options.groupflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.group, ((cp = ocs::tom::get<QAJ_group>(ep)) ? cp : ""));
         }
         if (options.owner != nullptr) {
            print_full(column_sizes.owner, options.owner);
         }
         else if (options.ownerflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.owner, ((cp = ocs::tom::get<QAJ_owner>(ep)) ? cp : "") );
         }
         if (options.project != nullptr) {
              print_full(column_sizes.project, options.project);
         }
         else if (options.projectflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.project ,((cp = ocs::tom::get<QAJ_project>(ep)) ? cp : ""));
         }
         if (options.department != nullptr) {
            print_full(column_sizes.department, options.department);
         } else if (options.departmentflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.department, ((cp = ocs::tom::get<QAJ_department>(ep)) ? cp : ""));
         }
         if (options.granted_pe != nullptr) {
            print_full(column_sizes.granted_pe, options.granted_pe);
         } else if (options.granted_peflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.granted_pe, ((cp = ocs::tom::get<QAJ_granted_pe>(ep)) ? cp : ""));
         }
         if (options.slots > 0) {
            print_full_ulong(column_sizes.slots, options.slots);
         } else if (options.slotsflag) {
            if (ep == nullptr) {
               break;
            }
            print_full_ulong(column_sizes.slots, ocs::tom::get<QAJ_slots>(ep));
         }

         if (options.ar_number > 0) {
            print_full_ulong(column_sizes.arid, options.ar_number);
         } else if (options.arflag) {
            if (ep == nullptr) {
               break;
            }
            print_full_ulong(column_sizes.arid, ocs::tom::get<QAJ_arid>(ep));
         }

         if (summary_view) {
             printf("%13.0f %13.3f %13.3f %13.3f %18.3f %18.3f %18.3f\n",
                   ocs::tom::get<QAJ_ru_wallclock>(ep),
                   ocs::tom::get<QAJ_ru_utime>(ep),
                   ocs::tom::get<QAJ_ru_stime>(ep),
                   ocs::tom::get<QAJ_cpu>(ep),
                   ocs::tom::get<QAJ_mem>(ep),
                   ocs::tom::get<QAJ_io>(ep),
                   ocs::tom::get<QAJ_iow>(ep));

            ep = ep.next();
            if (ep == nullptr) {
               break;
            }
         } else {
            printf("%13.0f %13.3f %13.3f %13.3f %18.3f %18.3f %18.3f\n",
                totals.ru_wallclock,
                totals.ru_utime,
                totals.ru_stime,
                totals.cpu,
                totals.mem,
                totals.io,
                totals.iow);
            break;
         }
      } /* end while */
   } /* end block */

   sorted_list.reset();
   sort_order = ocs::tom::SortOrder<ocs::tom::QAJ>();

   /*
   ** problem: other clients evaluate some status here
   */
   sge_free(&(options.group));
   sge_free(&(options.host));
   free_qacct_lists(centry_list, queue_list, exechost_list, hgrp_list);
   sge_exit(0);
   DRETURN(0);

QACCT_EXIT:
   ret = 1;
QACCT_EXIT_BUT_NO_ERROR:
   lFreeList(&(dusage.other_usage));
   sorted_list.reset();
   sort_order = ocs::tom::SortOrder<ocs::tom::QAJ>();
   sge_free(&(options.group));
   sge_free(&(options.host));
   free_qacct_lists(centry_list, queue_list, exechost_list, hgrp_list);
   sge_exit(ret);
   DRETURN(ret);
}

static void print_full_ulong(int full_length, uint32_t value) {
   DENTER(TOP_LAYER);

   char tmp_buf[100];

   snprintf(tmp_buf, sizeof(tmp_buf), "%5" sge_fu32, value);
   print_full(full_length, tmp_buf);
   DRETURN_VOID;
}

static void print_full(int full_length, const char* string) {
   DENTER(TOP_LAYER);

   int string_length=0;

   if ( string != nullptr) {
      printf("%s",string);
      string_length = strlen(string);
   }
   while (full_length > string_length) {
      printf(" ");
      string_length++;
   }
   DRETURN_VOID;
}

/**
 * @brief Widen the columns of the summary view to the values of the summary elements
 * @param ep the first summary element; it and the ones after it are measured, an empty handle measures nothing
 * @param column_size_data the column widths, raised where a value is wider
 * @return none
 */
static void calc_column_sizes(const ocs::tom::ConstElement<ocs::tom::QAJ> ep, sge_qacct_columns *column_size_data) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::QAJ> lep;
   if (column_size_data == nullptr) {
      DPRINTF("no column size data!\n");
      DRETURN_VOID;
   }

/*   column_size_data->host = 30;
   column_size_data->queue = 15;
   column_size_data->group = 10;
   column_size_data->owner = 10;
   column_size_data->project = 17;
   column_size_data->department = 20;
   column_size_data->granted_pe = 15;
   column_size_data->slots = 6; */

   if (column_size_data->host < strlen(MSG_HISTORY_HOST)+1) {
      column_size_data->host = strlen(MSG_HISTORY_HOST)+1;
   }
   if (column_size_data->queue < strlen(MSG_HISTORY_QUEUE)+1) {
      column_size_data->queue = strlen(MSG_HISTORY_QUEUE)+1;
   }
   if (column_size_data->group < strlen(MSG_HISTORY_GROUP)+1) {
      column_size_data->group = strlen(MSG_HISTORY_GROUP)+1;
   }
   if (column_size_data->owner < strlen(MSG_HISTORY_OWNER)+1) {
      column_size_data->owner = strlen(MSG_HISTORY_OWNER)+1;
   }
   if (column_size_data->project < strlen(MSG_HISTORY_PROJECT)+1) {
      column_size_data->project = strlen(MSG_HISTORY_PROJECT)+1;
   }
   if (column_size_data->department < strlen(MSG_HISTORY_DEPARTMENT)+1) {
      column_size_data->department = strlen(MSG_HISTORY_DEPARTMENT)+1;
   }
   if (column_size_data->granted_pe < strlen(MSG_HISTORY_PE)+1) {
      column_size_data->granted_pe = strlen(MSG_HISTORY_PE)+1;
   }
   if (column_size_data->slots < 5) {
      column_size_data->slots = 5;
   }
   if (column_size_data->arid < 5) {
      column_size_data->arid = 5;
   }

   if (ep != nullptr) {
      char tmp_buf[100];
      size_t tmp_length = 0;
      const char* tmp_string = nullptr;
      lep = ep;
      while (lep) {
         /* host  */
         tmp_string = ocs::tom::get<QAJ_host>(lep);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->host <= tmp_length) {
               column_size_data->host  = tmp_length + 1;
            }
         }
         /* queue */
         tmp_string = ocs::tom::get<QAJ_queue>(lep);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->queue <= tmp_length) {
               column_size_data->queue  = tmp_length + 1;
            }
         }
         /* group */
         tmp_string = ocs::tom::get<QAJ_group>(lep) ;
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->group <= tmp_length) {
               column_size_data->group  = tmp_length + 1;
            }
         }
         /* owner */
         tmp_string = ocs::tom::get<QAJ_owner>(lep);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->owner <= tmp_length) {
               column_size_data->owner  = tmp_length + 1;
            }
         }
         /* project */
         tmp_string = ocs::tom::get<QAJ_project>(lep);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->project <= tmp_length) {
               column_size_data->project  = tmp_length + 1;
            }
         }

         /* department  */
         tmp_string = ocs::tom::get<QAJ_department>(lep);
         if ( tmp_string != nullptr ) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->department <= tmp_length) {
               column_size_data->department  = tmp_length + 1;
            }
         }
         /* granted_pe */
         tmp_string = ocs::tom::get<QAJ_granted_pe>(lep) ;
         if ( tmp_string != nullptr ) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->granted_pe <= tmp_length) {
               column_size_data->granted_pe  = tmp_length + 1;
            }
         }

         /* slots */
         snprintf(tmp_buf, sizeof(tmp_buf), "%5" sge_fu32, ocs::tom::get<QAJ_slots>(lep));
         tmp_length = strlen(tmp_buf);
         if (column_size_data->slots <= tmp_length) {
            column_size_data->slots  = tmp_length + 1;
         }

         /* advance reservations */
         snprintf(tmp_buf, sizeof(tmp_buf), "%5" sge_fu32, ocs::tom::get<QAJ_arid>(lep));
         tmp_length = strlen(tmp_buf);
         if (column_size_data->arid <= tmp_length) {
            column_size_data->arid = tmp_length + 1;
         }

         lep = lep.next();
      }
   } else {
     DPRINTF("got nullptr list\n");
   }

   DRETURN_VOID;
}


/*
** NAME
**   qacct_usage
** PARAMETER
**   none
** RETURN
**   none
** EXTERNAL
**   none
** DESCRIPTION
**   displays usage for qacct client
**   note that the other clients use a common function
**   for this. output was adapted to a similar look.
*/
static void qacct_usage(FILE *err_fp) {
   DENTER(TOP_LAYER);

   dstring ds;
   char buffer[256];

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(err_fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

   fprintf(err_fp, "%s qacct [options]\n", MSG_HISTORY_USAGE);
   fprintf(err_fp, " [-ar [ar_id]]                     %s\n", MSG_HISTORY_ar_OPT_USAGE);
   fprintf(err_fp, " [-A account_string]               %s\n", MSG_HISTORY_A_OPT_USAGE);
   fprintf(err_fp, " [-b begin_time]                   %s\n", MSG_HISTORY_b_OPT_USAGE);
   fprintf(err_fp, " [-d days]                         %s\n", MSG_HISTORY_d_OPT_USAGE );
   fprintf(err_fp, " [-D [department]]                 %s\n", MSG_HISTORY_D_OPT_USAGE);
   fprintf(err_fp, " [-e end_time]                     %s\n", MSG_HISTORY_e_OPT_USAGE);
   fprintf(err_fp, " [-g [groupid|groupname]]          %s\n", MSG_HISTORY_g_OPT_USAGE );
   fprintf(err_fp, " [-h [host]]                       %s\n", MSG_HISTORY_h_OPT_USAGE );
   fprintf(err_fp, " [-help]                           %s\n", MSG_HISTORY_help_OPT_USAGE);
   fprintf(err_fp, " [-j [job_id|job_name|pattern]]    %s\n", MSG_HISTORY_j_OPT_USAGE);
   fprintf(err_fp, " [-l attr=val,...]                 %s\n", MSG_HISTORY_l_OPT_USAGE );
   fprintf(err_fp, " [-o [owner]]                      %s\n", MSG_HISTORY_o_OPT_USAGE);
   fprintf(err_fp, " [-pe [pe_name]]                   %s\n", MSG_HISTORY_pe_OPT_USAGE );
   fprintf(err_fp, " [-P [project]]                    %s\n", MSG_HISTORY_P_OPT_USAGE );
   fprintf(err_fp, " [-q [queue]]                      %s\n", MSG_HISTORY_q_OPT_USAGE );
   fprintf(err_fp, " [-slots [slots]]                  %s\n", MSG_HISTORY_slots_OPT_USAGE);
   fprintf(err_fp, " [-t taskid[-taskid[:step]]]       %s\n", MSG_HISTORY_t_OPT_USAGE );
   fprintf(err_fp, " [[-f] acctfile]                   %s\n", MSG_HISTORY_f_OPT_USAGE );

   fprintf(err_fp, "\n");
   fprintf(err_fp, " begin_time, end_time              %s\n", MSG_HISTORY_beginend_OPT_USAGE );
   fprintf(err_fp, " queue                             [cluster_queue|queue_instance|queue_domain|pattern]\n");
   if (err_fp == stderr) {
      sge_exit(1);
   } else {
      sge_exit(0);
   }

   DRETURN_VOID;
}


/** @name Line formats of the `qacct -j` detail output
 *
 * Every line is a 35-column label followed by the value, so the labels line up
 * whatever the value is. The variants differ only in how the value is
 * formatted.
 * @{
 */
#define SHOWJOB_STRING         "%-35.34s%s\n"                       ///< label and a string of any length
#define SHOWJOB_STRING_NO_DATA "%-35.34s-/-\n"                      ///< label and `-/-`, for a value the record does not carry
#define SHOWJOB_STRING_20      "%-35.34s%-20s\n"                    ///< label and a string padded to 20 columns
#define SHOWJOB_STRING_20_NOLF "%-35.34s%-20s"                      ///< #SHOWJOB_STRING_20 without the newline, so more can be appended
#define SHOWJOB_U32_20         "%-35.34s%-20" sge_fuu32 "\n"        ///< label and a `uint32_t` padded to 20 columns
#define SHOWJOB_U32_FAILED     "%-35.34s%-3" sge_fuu32" %s %s\n"    ///< label, the failure code, and the two strings that explain it
#define SHOWJOB_FLOAT_0        "%-35.34s%-13.0f\n"                  ///< label and a float, no decimals
#define SHOWJOB_FLOAT_3        "%-35.34s%-13.3f\n"                  ///< label and a float, three decimals
#define SHOWJOB_FLOAT_18_0     "%-35.34s%-18.0f\n"                  ///< #SHOWJOB_FLOAT_0 in a wider column
#define SHOWJOB_FLOAT_18_3     "%-35.34s%-18.3f\n"                  ///< #SHOWJOB_FLOAT_3 in a wider column
#define SHOWJOB_CUSTOM_USAGE   "%-34s %s\n"                         ///< custom usage value
/** @} */

/**
 * @brief Check if a name is printed as a standard attribute by showjob().
 *
 * A custom usage value (reporting_params usage_patterns) can have the name of a standard
 * attribute, e.g. with a pattern like io* or *. Such a value is already shown as standard
 * attribute and must not be printed a second time.
 *
 * @param[in] name the name of a custom usage value
 * @return true if showjob() prints an attribute of this name
 */
static bool
is_showjob_standard_name(const char *name) {
   DENTER(TOP_LAYER);
   static const char *const standard_names[] = {
      MSG_HISTORY_SHOWJOB_QNAME, MSG_HISTORY_SHOWJOB_HOSTNAME, MSG_HISTORY_SHOWJOB_GROUP,
      MSG_HISTORY_SHOWJOB_OWNER, MSG_HISTORY_SHOWJOB_PROJECT, MSG_HISTORY_SHOWJOB_DEPARTMENT,
      MSG_HISTORY_SHOWJOB_JOBNAME, MSG_HISTORY_SHOWJOB_JOBNUMBER, MSG_HISTORY_SHOWJOB_TASKID,
      MSG_HISTORY_SHOWJOB_PE_TASKID, MSG_HISTORY_SHOWJOB_ARID, MSG_HISTORY_SHOWJOB_ACCOUNT,
      MSG_HISTORY_SHOWJOB_PRIORITY, MSG_HISTORY_SHOWJOB_QSUBTIME, MSG_HISTORY_SHOWJOB_SUBMITCMDLINE,
      MSG_HISTORY_SHOWJOB_STARTTIME, MSG_HISTORY_SHOWJOB_ENDTIME, MSG_HISTORY_SHOWJOB_GRANTEDPE,
      MSG_HISTORY_SHOWJOB_SLOTS, MSG_HISTORY_SHOWJOB_FAILED, MSG_HISTORY_SHOWJOB_EXITSTATUS,
      MSG_HISTORY_SHOWJOB_DELETED_BY, MSG_HISTORY_SHOWJOB_RUWALLCLOCK, MSG_HISTORY_SHOWJOB_RUUTIME,
      MSG_HISTORY_SHOWJOB_RUSTIME, MSG_HISTORY_SHOWJOB_RUMAXRSS, MSG_HISTORY_SHOWJOB_RUIXRSS,
      MSG_HISTORY_SHOWJOB_RUISMRSS, MSG_HISTORY_SHOWJOB_RUIDRSS, MSG_HISTORY_SHOWJOB_RUISRSS,
      MSG_HISTORY_SHOWJOB_RUMINFLT, MSG_HISTORY_SHOWJOB_RUMAJFLT, MSG_HISTORY_SHOWJOB_RUNSWAP,
      MSG_HISTORY_SHOWJOB_RUINBLOCK, MSG_HISTORY_SHOWJOB_RUOUBLOCK, MSG_HISTORY_SHOWJOB_RUMSGSND,
      MSG_HISTORY_SHOWJOB_RUMSGRCV, MSG_HISTORY_SHOWJOB_RUNSIGNALS, MSG_HISTORY_SHOWJOB_RUNVCSW,
      MSG_HISTORY_SHOWJOB_RUNIVCSW, MSG_HISTORY_SHOWJOB_WALLCLOCK, MSG_HISTORY_SHOWJOB_CPU,
      MSG_HISTORY_SHOWJOB_MEM, MSG_HISTORY_SHOWJOB_IO, MSG_HISTORY_SHOWJOB_IOOPS,
      MSG_HISTORY_SHOWJOB_IOW, MSG_HISTORY_SHOWJOB_MAXVMEM, MSG_HISTORY_SHOWJOB_MAXRSS,
      MSG_HISTORY_SHOWJOB_MAXPSS
   };

   for (const char *standard_name : standard_names) {
      if (strcmp(name, standard_name) == 0) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

/**
 * @brief Format a custom usage value the way it is written to the accounting file.
 *
 * Uses the shortest representation which reads back as the same double, as the JSON writer
 * of the accounting file does (e.g. 0.42, 1e-7, 123456789012345), so small values do not
 * show as 0 and integral values get no decimals. The ".0" the JSON writer appends to an
 * integral value is left out.
 *
 * @param[in] value the custom usage value
 * @param[out] buffer receives the formatted value
 * @return the formatted value, pointing into buffer
 */
static const char *
format_custom_usage(double value, char (&buffer)[64]) {
   DENTER(TOP_LAYER);
   if (!std::isfinite(value)) {
      snprintf(buffer, sizeof(buffer), "%g", value);
      DRETURN(buffer);
   }

   char *end = rapidjson::internal::dtoa(value, buffer);
   *end = '\0';
   const size_t len = static_cast<size_t>(end - buffer);
   if (len > 2 && strcmp(end - 2, ".0") == 0) {
      end[-2] = '\0';
   }
   DRETURN(buffer);
}

/** @brief Print one accounting record in full, the `qacct -j` output
 *
 * @param dusage one line of the accounting file, already parsed
 * @return none
 */
static void showjob(sge_rusage_type *dusage) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(dstr_buffer, 100);
   dstring *pdstr_buffer = &dstr_buffer;

   printf("==============================================================\n");
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_QNAME, (dusage->qname ? dusage->qname : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_HOSTNAME, (dusage->hostname ? dusage->hostname : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_GROUP, (dusage->group ? dusage->group : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_OWNER, (dusage->owner ? dusage->owner : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_PROJECT, (dusage->project ? dusage->project : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_DEPARTMENT, (dusage->department ? dusage->department : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_JOBNAME, (dusage->job_name ? dusage->job_name : MSG_HISTORY_SHOWJOB_NULL));
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_JOBNUMBER, dusage->job_number);

   if (dusage->task_number != 0) {
      printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_TASKID, dusage->task_number);              /* job-array task number */
   } else {
      printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_TASKID, "undefined");
   }

   if (!dusage->is_classic) {
      if (dusage->pe_taskid != nullptr) {
         printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_PE_TASKID, dusage->pe_taskid);
      } else {
         printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_PE_TASKID, NONE_STR);
      }
   }

   if (dusage->ar != 0) {
      printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_ARID, dusage->ar);              /* job-array task number */
   } else {
      printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_ARID, "undefined");
   }

   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_ACCOUNT, (dusage->account ? dusage->account : MSG_HISTORY_SHOWJOB_NULL ));
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_PRIORITY, dusage->priority);
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_QSUBTIME, sge_ctime64(dusage->submission_time, pdstr_buffer));
   if (!dusage->is_classic) {
      printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_SUBMITCMDLINE, dusage->submission_command_line);
   }

   if (dusage->start_time)
      printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_STARTTIME, sge_ctime64(dusage->start_time, pdstr_buffer));
   else
      printf(SHOWJOB_STRING_NO_DATA,MSG_HISTORY_SHOWJOB_STARTTIME);

   if (dusage->end_time)
      printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_ENDTIME, sge_ctime64(dusage->end_time, pdstr_buffer));
   else
      printf(SHOWJOB_STRING_NO_DATA,MSG_HISTORY_SHOWJOB_ENDTIME);


   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_GRANTEDPE, dusage->granted_pe);
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_SLOTS, dusage->slots);
   printf(SHOWJOB_U32_FAILED,MSG_HISTORY_SHOWJOB_FAILED, dusage->failed, (dusage->failed ? ":" : ""), get_sstate_description(dusage->failed));
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_EXITSTATUS, dusage->exit_status);
   if (!dusage->is_classic && dusage->deleted_by != nullptr) {
      printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_DELETED_BY, dusage->deleted_by);
   }
   printf(SHOWJOB_FLOAT_0,MSG_HISTORY_SHOWJOB_RUWALLCLOCK, dusage->ru_wallclock);

   printf(SHOWJOB_FLOAT_3,MSG_HISTORY_SHOWJOB_RUUTIME, dusage->ru_utime);    /* user time used */
   printf(SHOWJOB_FLOAT_3, MSG_HISTORY_SHOWJOB_RUSTIME, dusage->ru_stime);    /* system time used */
      printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUMAXRSS,  dusage->ru_maxrss);     /* maximum resident set size */
      printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUIXRSS,  dusage->ru_ixrss);       /* integral shared text size */
      printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUISMRSS,  dusage->ru_ismrss);     /* integral shared memory size*/
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUIDRSS,      dusage->ru_idrss);      /* integral unshared data "  */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUISRSS,      dusage->ru_isrss);      /* integral unshared stack "  */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUMINFLT,     dusage->ru_minflt);     /* page reclaims */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUMAJFLT,     dusage->ru_majflt);     /* page faults */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUNSWAP,      dusage->ru_nswap);      /* swaps */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUINBLOCK,    dusage->ru_inblock);    /* block input operations */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUOUBLOCK,    dusage->ru_oublock);    /* block output operations */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUMSGSND,     dusage->ru_msgsnd);     /* messages sent */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUMSGRCV,     dusage->ru_msgrcv);     /* messages received */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUNSIGNALS,   dusage->ru_nsignals);   /* signals received */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUNVCSW,      dusage->ru_nvcsw);      /* voluntary context switches */
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_RUNIVCSW,     dusage->ru_nivcsw);     /* involuntary */

   if (!dusage->is_classic) {
      printf(SHOWJOB_FLOAT_3,    MSG_HISTORY_SHOWJOB_WALLCLOCK,   dusage->wallclock);
   }
   printf(SHOWJOB_FLOAT_3,    MSG_HISTORY_SHOWJOB_CPU,         dusage->cpu);
   printf(SHOWJOB_FLOAT_18_3, MSG_HISTORY_SHOWJOB_MEM,         dusage->mem);
   printf(SHOWJOB_FLOAT_18_3, MSG_HISTORY_SHOWJOB_IO,          dusage->io);
   printf(SHOWJOB_FLOAT_0,    MSG_HISTORY_SHOWJOB_IOOPS,       dusage->ioops);
   printf(SHOWJOB_FLOAT_18_3, MSG_HISTORY_SHOWJOB_IOW,         dusage->iow);

   printf(SHOWJOB_FLOAT_18_0,   MSG_HISTORY_SHOWJOB_MAXVMEM,      dusage->maxvmem);

   if (!dusage->is_classic) {
      printf(SHOWJOB_FLOAT_18_0,   MSG_HISTORY_SHOWJOB_MAXRSS,       dusage->maxrss);
      if (dusage->maxpss != DBL_MAX) {
         printf(SHOWJOB_FLOAT_18_0,   MSG_HISTORY_SHOWJOB_MAXPSS,       dusage->maxpss);
      }

      // print the custom usage values (reporting_params usage_patterns) which are not
      // already printed as standard attribute. CS-849: a string value is printed as it is.
      // the record parser builds the custom usage with UA_Type, so it is wrapped unchecked
      for (const auto ep : ocs::tom::ConstList<ocs::tom::UA>::from_typed(dusage->other_usage)) {
         const char *name = ocs::tom::get<UA_name>(ep);
         if (!is_showjob_standard_name(name)) {
            const char *svalue = ocs::tom::get<UA_svalue>(ep);
            char buffer[64];
            printf(SHOWJOB_CUSTOM_USAGE, name,
                   svalue != nullptr ? svalue : format_custom_usage(ocs::tom::get<UA_value>(ep), buffer));
         }
      }
   }

   sge_dstring_free(pdstr_buffer);

   DRETURN_VOID;
}

/**
 * @brief Retrieve the lists qacct needs from qmaster
 *
 * Programmed after the get_all_lists() function in qstat, but gets the queue
 * list in a different way. The queue list is always requested.
 *
 * @param alpp the answer list
 * @param ppcentries receives the complex list (CE_Type), or nullptr if it is not needed
 * @param ppqueues receives the cluster queue list (CQ_Type); must not be nullptr
 * @param ppexechosts receives the exec host list (EH_Type), or nullptr if it is not needed
 * @param hgrp_l receives the host group list (HGRP_Type), or nullptr if it is not needed
 *
 * @return true on success, false with @p alpp filled otherwise
 */
static bool get_qacct_lists(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ListPtr<ocs::tom::CE> *ppcentries,
                            ocs::tom::ListPtr<ocs::tom::CQ> *ppqueues, ocs::tom::ListPtr<ocs::tom::EH> *ppexechosts,
                            ocs::tom::ListPtr<ocs::tom::HGRP> *hgrp_l) {
   DENTER(TOP_LAYER);

   int ce_id = 0, eh_id = 0, q_id = 0, hgrp_id = 0;
   ocs::gdi::Request gdi_multi{};

   // Request has no typed form yet; it does not change the condition and the selection
   /*
   ** GET SGE_CE_LIST
   */
   if (ppcentries) {
      const auto what = ocs::tom::what_all<ocs::tom::CE>();
      ce_id = gdi_multi.request(alpp, ocs::gdi::Mode::RECORD, ocs::gdi::Target::CE_LIST, ocs::gdi::Command::GET,
                                ocs::gdi::SubCommand::NONE, nullptr, nullptr,
                                const_cast<lEnumeration *>(what.get()), true);

      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }
   /*
   ** GET SGE_EH_LIST
   */
   if (ppexechosts) {
      const auto where = ocs::tom::field<EH_name>() != SGE_TEMPLATE_NAME;
      const auto what = ocs::tom::what_all<ocs::tom::EH>();
      eh_id = gdi_multi.request(alpp, ocs::gdi::Mode::RECORD, ocs::gdi::Target::EH_LIST, ocs::gdi::Command::GET,
                                ocs::gdi::SubCommand::NONE, nullptr, const_cast<lCondition *>(where.get()),
                                const_cast<lEnumeration *>(what.get()), true);

      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   /*
   ** hgroup
   */
   if (hgrp_l) {
      const auto what = ocs::tom::what_all<ocs::tom::HGRP>();
      hgrp_id = gdi_multi.request(alpp, ocs::gdi::Mode::RECORD, ocs::gdi::Target::HGRP_LIST,
                                  ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, nullptr, nullptr,
                                  const_cast<lEnumeration *>(what.get()), true);

      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }
   /*
   ** GET SGE_QUEUE_LIST
   */
   {
      // the selection of every field is built for QU_Type, as it always was, and asks for all of a cluster queue
      const auto what = ocs::tom::what_all<ocs::tom::QU>();
      q_id = gdi_multi.request(alpp, ocs::gdi::Mode::SEND, ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::GET,
                               ocs::gdi::SubCommand::NONE, nullptr, nullptr,
                               const_cast<lEnumeration *>(what.get()), true);
      gdi_multi.wait();
   }

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   /*
   ** handle results; get_response() has no typed form yet, it sets the raw list of the response, which is
   ** taken over with the type of the request; a list held before is freed (it was leaked)
   */
   /* --- complex */
   if (ppcentries) {
      lList *lp = nullptr;
      gdi_multi.get_response(alpp, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, ocs::gdi::Target::CE_LIST,
                             ce_id, &lp);
      *ppcentries = ocs::tom::ListPtr<ocs::tom::CE>::from_typed(lp);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   /* --- exec host */
   if (ppexechosts) {
      lList *lp = nullptr;
      gdi_multi.get_response(alpp, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, ocs::gdi::Target::EH_LIST,
                             eh_id, &lp);
      *ppexechosts = ocs::tom::ListPtr<ocs::tom::EH>::from_typed(lp);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   /* --- queue */
   if (ppqueues) {
      lList *lp = nullptr;
      gdi_multi.get_response(alpp, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, ocs::gdi::Target::CQ_LIST,
                             q_id, &lp);
      *ppqueues = ocs::tom::ListPtr<ocs::tom::CQ>::from_typed(lp);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   /* --- hgrp */
   if (hgrp_l) {
      lList *lp = nullptr;
      gdi_multi.get_response(alpp, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, ocs::gdi::Target::HGRP_LIST,
                             hgrp_id, &lp);
      *hgrp_l = ocs::tom::ListPtr<ocs::tom::HGRP>::from_typed(lp);
      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }
   /* --- end */
   DRETURN(true);
}

/**
 * @brief Free the lists get_qacct_lists() retrieved, before the process ends with sge_exit()
 *
 * @param ppcentries the complex list
 * @param ppqueues the cluster queue list
 * @param ppexechosts the exec host list
 * @param hgrp_l the host group list
 */
static void free_qacct_lists(ocs::tom::ListPtr<ocs::tom::CE> &ppcentries, ocs::tom::ListPtr<ocs::tom::CQ> &ppqueues,
                             ocs::tom::ListPtr<ocs::tom::EH> &ppexechosts, ocs::tom::ListPtr<ocs::tom::HGRP> &hgrp_l) {
   DENTER(TOP_LAYER);
   ppcentries.reset();
   ppqueues.reset();
   ppexechosts.reset();
   hgrp_l.reset();
   DRETURN_VOID;
}

static int
sge_read_rusage_classic(char *line, sge_rusage_type *d, sge_qacct_options *options);

static int
sge_read_rusage_json(const char *line, sge_rusage_type *d, sge_qacct_options *options);

static int
sge_read_rusage(std::ifstream &acct_file_stream, sge_rusage_type *d, sge_qacct_options *options,
                std::string &acct_file_line) {
   DENTER(TOP_LAYER);

   int ret = 0;
   size_t len;

   do {
      if (!std::getline(acct_file_stream, acct_file_line)) {
         DRETURN(2);
      }
      len = acct_file_line.length();
      if (acct_file_line[len] == '\n') {
         acct_file_line[len] = '\0';
      }
   } while (len <= 1 || acct_file_line[0] == COMMENT_CHAR);

   // the custom usage belongs to one record, don't show it for the following ones
   lFreeList(&(d->other_usage));

   /*
    * qname
    */
   if (acct_file_line[0] == '{') {
      // JSON @todo we could determine this once
      ret = sge_read_rusage_json(acct_file_line.c_str(), d, options);
   } else {
      // old colon separated file
      // for parsing the old accounting file format we need a writable copy of the input data (one line)
      // have it in memory once
      static char old_acct_buffer[MAX_STRING_SIZE * 10];
      sge_strlcpy(old_acct_buffer, acct_file_line.c_str(), sizeof(old_acct_buffer));
      ret = sge_read_rusage_classic(old_acct_buffer, d, options);
   }

   DRETURN(ret);
}

/**
 * @brief Whether the queue instance of an accounting record matches one of the -q patterns
 *
 * Called for every record read, so it carries no DENTER.
 *
 * @param d the accounting record
 * @param queue_name_list the queue instance patterns (`QR_Type`)
 * @return true if a pattern matches
 */
static bool matches_queue_name_list(sge_rusage_type *d, const lList *queue_name_list) {
   bool ret = false;

   DSTRING_STATIC(dstr, MAX_STRING_SIZE);
   const char *qinstance;
   qinstance = sge_dstring_sprintf(&dstr, "%s@%s", d->qname, d->hostname);

   /* emplace_back<QR_name>() and qref_list_resolve() create the list with QR_Type, so it is wrapped unchecked */
   for (const auto ep : ocs::tom::ConstList<ocs::tom::QR>::from_typed(queue_name_list)) {
      if (fnmatch(ocs::tom::get<QR_name>(ep), qinstance, 0) == 0) {
         ret = true;
         break;
      }
   }

   return ret;
}

static int
sge_read_rusage_classic(char *line, sge_rusage_type *d, sge_qacct_options *options) {
   DENTER(TOP_LAYER);

   char *pc = strtok(line, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->qname = pc;

   /*
    * hostname
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->hostname = pc;
   if (options->host != nullptr && sge_hostcmp(options->host, d->hostname)) {
      DRETURN(-2);
   }

   if (options->queue_name_list != nullptr) {
      if (!matches_queue_name_list(d, options->queue_name_list)) {
         DRETURN(-2);
      }
   }

   /*
    * group
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->group = pc;
   if (options->group != nullptr && sge_strnullcmp(options->group, d->group)) {
      DRETURN(-2);
   }

   /*
    * owner
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->owner = pc;
   if (options->owner != nullptr && sge_strnullcmp(options->owner, d->owner)) {
      DRETURN(-2);
   }

   /*
    * job_name
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->job_name = pc;

   /*
    * job_number
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }

   d->job_number = SGE_STRTOU_LONG32(pc);
   if (!options->jobflag && (options->job_number || options->job_name != nullptr)) {
      if (((d->job_number != options->job_number) && sge_patternnullcmp(d->job_name, options->job_name))) {
         DRETURN(-2);
      }
   }

   /*
    * account
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->account = pc;
   if (options->accountflag && sge_strnullcmp(options->account, d->account)) {
      DRETURN(-2);
   }

   /*
    * priority
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->priority = SGE_STRTOU_LONG32(pc);

   /*
    * submission_time
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->submission_time = sge_gmt32_to_gmt64(SGE_STRTOU_LONG32(pc));

   /*
    * start_time
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->start_time = sge_gmt32_to_gmt64(SGE_STRTOU_LONG32(pc));
   /*
   ** skipping jobs that never ran
   */
   if ((d->start_time == 0) && options->complexflag)  {
      DPRINTF("skipping job that never ran\n");
      DRETURN(-2);
   }
   if (options->begin_time != std::numeric_limits<uint64_t>::max() && d->start_time < options->begin_time) {
      DRETURN(-2);
   }
   if (options->end_time != std::numeric_limits<uint64_t>::max() && d->start_time > options->end_time) {
      DRETURN(-2);
   }

   /*
    * end_time
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->end_time = sge_gmt32_to_gmt64(SGE_STRTOU_LONG32(pc));

   /*
    * failed
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->failed = SGE_STRTOU_LONG32(pc);

   /*
    * exit_status
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->exit_status = SGE_STRTOU_LONG32(pc);

   /*
    * ru_wallclock
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_wallclock = atof(pc);

   /*
    * ru_utime
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_utime = atof(pc);

   /*
    * ru_stime
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_stime = atof(pc);

   /*
    * ru_maxrss
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_maxrss = SGE_STRTOU_LONG32(pc);

   /*
    * ru_ixrss
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_ixrss = SGE_STRTOU_LONG32(pc);

   /*
    * ru_ismrss
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_ismrss = SGE_STRTOU_LONG32(pc);

   /*
    * ru_idrss
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_idrss = SGE_STRTOU_LONG32(pc);

   /*
    * ru_isrss
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_isrss = SGE_STRTOU_LONG32(pc);

   /*
    * ru_minflt
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_minflt = SGE_STRTOU_LONG32(pc);

   /*
    * ru_majflt
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_majflt = SGE_STRTOU_LONG32(pc);

   /*
    * ru_nswap
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_nswap = SGE_STRTOU_LONG32(pc);

   /*
    * ru_inblock
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_inblock = SGE_STRTOU_LONG32(pc);

   /*
    * ru_oublock
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_oublock = SGE_STRTOU_LONG32(pc);

   /*
    * ru_msgsnd
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_msgsnd = SGE_STRTOU_LONG32(pc);

   /*
    * ru_msgrcv
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_msgrcv = SGE_STRTOU_LONG32(pc);

   /*
    * ru_nsignals
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_nsignals = SGE_STRTOU_LONG32(pc);

   /*
    * ru_nvcsw
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_nvcsw = SGE_STRTOU_LONG32(pc);

   /*
    * ru_nivcsw
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->ru_nivcsw = SGE_STRTOU_LONG32(pc);

   /*
    * project
    */
   pc = strtok(nullptr, ":");
   if (!pc) {
      DRETURN(-1);
   }
   d->project = pc;
   if (options->project != nullptr && sge_strnullcmp(options->project, d->project)) {
      DRETURN(-2);
   }

   /*
    * department
    */
   pc = strtok(nullptr, ":\n");
   if (!pc) {
      DRETURN(-1);
   }
   d->department = pc;
   if (options->department != nullptr && sge_strnullcmp(options->department, d->department)) {
      DRETURN(-2);
   }

   /* PE name */
   pc = strtok(nullptr, ":");
   if (pc) {
      d->granted_pe = pc;
   } else {
      d->granted_pe = (char *)"none";
   }
   if (options->granted_pe != nullptr && sge_strnullcmp(options->granted_pe, d->granted_pe)) {
      DRETURN(-2);
   }

   /* slots */
   pc = strtok(nullptr, ":");
   if (pc) {
      d->slots = SGE_STRTOU_LONG32(pc);
   } else {
      d->slots = 0;
   }
   if ((options->slots > 0) && (options->slots != d->slots)) {
      DRETURN(-2);
   }

   /* task number */
   pc = strtok(nullptr, ":");
   if (pc) {
      d->task_number = SGE_STRTOU_LONG32(pc);
   } else {
      d->task_number = 0;
   }

   if (!options->jobflag) {

      if (options->taskstart && options->taskend && options->taskstep) {
         if (d->task_number < options->taskstart || d->task_number > options->taskend ||
             !(((d->task_number-options->taskstart)%options->taskstep) == 0)) {
            DRETURN(-2);
         }
      }
   }

   d->cpu = ((pc=strtok(nullptr, ":")))?atof(pc):0;
   d->mem = ((pc=strtok(nullptr, ":")))?atof(pc):0;
   d->io = ((pc=strtok(nullptr, ":")))?atof(pc):0;

   /* skip job category */
   pc=strtok(nullptr, ":");
#if 0
   while ((pc=strtok(nullptr, ":")) &&
          strlen(pc) &&
          pc[strlen(pc)-1] != ' ' &&
          strcmp(pc, "none")) {
      /*
       * The job category field might contain colons (':').
       * Therefore we have to skip all colons until we find a " :".
       * Only if the category is "none" then ":" is the real delimiter.
       */
      ;
   }
#endif
   d->iow = ((pc=strtok(nullptr, ":")))?atof(pc):0;

   /* skip pe_taskid */
   pc=strtok(nullptr, ":");

   d->maxvmem = ((pc=strtok(nullptr, ":")))?atof(pc):0;
   d->ar = ((pc=strtok(nullptr, ":")))?SGE_STRTOU_LONG32(pc):0;
   if ((options->ar_number > 0) && (options->ar_number != d->ar)) {
      DRETURN(-2);
   }

   d->is_classic = true;
   /* deleted_by exists only in the JSONL accounting format */
   d->deleted_by = nullptr;

   /* ... */
   options->jobfound=1;

   DRETURN(0);
}

static uint32_t
read_json(const rapidjson::Value &json, const char *name, uint32_t default_value) {
   if (json.HasMember(name)) {
      return json[name].GetUint();
   }

   return default_value;
}

static uint64_t
read_json(const rapidjson::Value &json, const char *name, uint64_t default_value) {
   if (json.HasMember(name)) {
      return json[name].GetUint64();
   }

   return default_value;
}

static double
read_json(const rapidjson::Value &json, const char *name, double default_value) {
   if (json.HasMember(name)) {
      return json[name].GetDouble();
   }

   return default_value;
}

static const char *
read_json(const rapidjson::Value &json, const char *name, const char *default_value) {
   if (json.HasMember(name)) {
      return json[name].GetString();
   }

   return default_value;
}

static rapidjson::Document document;

static int
sge_read_rusage_json(const char *line, sge_rusage_type *d, sge_qacct_options *options) {
   DENTER(TOP_LAYER);

   document.SetNull();
   document.Parse(line);

   if (document.IsObject()) {
      // parse the JSON document, do filtering and store values in sge_rusage_type *d
      d->job_name = (char *) read_json(document, "job_name", nullptr);
      d->job_number = read_json(document, "job_number", (uint32_t)0);
      if (!options->jobflag && (options->job_number || options->job_name != nullptr)) {
         if (((d->job_number != options->job_number) && sge_patternnullcmp(d->job_name, options->job_name))) {
            DRETURN(-2);
         }
      }

      d->task_number = read_json(document, "task_number", (uint32_t)0);
      if (!options->jobflag) {
         if (options->taskstart && options->taskend && options->taskstep) {
            if (d->task_number < options->taskstart || d->task_number > options->taskend ||
                !(((d->task_number - options->taskstart) % options->taskstep) == 0)) {
               DRETURN(-2);
            }
         }
      }

      d->start_time = read_json(document, "start_time", (uint64_t)0);
      /*
      ** skipping jobs that never ran
      */
      if ((d->start_time == 0) && options->complexflag) {
         DPRINTF("skipping job that never ran\n");
         DRETURN(-2);
      }
      if (options->begin_time != std::numeric_limits<uint64_t>::max() && d->start_time < options->begin_time) {
         DRETURN(-2);
      }
      if (options->end_time != std::numeric_limits<uint64_t>::max() && d->start_time > options->end_time) {
         DRETURN(-2);
      }
      d->end_time = read_json(document, "end_time", (uint64_t)0);

      d->owner = (char *) read_json(document, "owner", nullptr);
      if (options->owner != nullptr && sge_strnullcmp(options->owner, d->owner)) {
         DRETURN(-2);
      }

      d->group = (char *) read_json(document, "group", nullptr);
      if (options->group != nullptr && sge_strnullcmp(options->group, d->group)) {
         DRETURN(-2);
      }

      d->account = (char *) read_json(document, "account", nullptr);
      if (options->accountflag && sge_strnullcmp(options->account, d->account)) {
         DRETURN(-2);
      }

      d->qname = (char *) read_json(document, "qname", nullptr);
      d->hostname = (char *) read_json(document, "hostname", nullptr);
      if (options->host != nullptr && sge_hostcmp(options->host, d->hostname) != 0) {
         DRETURN(-2);
      }
      if (options->queue_name_list != nullptr) {
         if (!matches_queue_name_list(d, options->queue_name_list)) {
            DRETURN(-2);
         }
      }

      d->project = (char *) read_json(document, "project", NONE_STR);
      if (options->project != nullptr && sge_strnullcmp(options->project, d->project)) {
         DRETURN(-2);
      }

      d->department = (char *) read_json(document, "department", nullptr);
      if (options->department != nullptr && sge_strnullcmp(options->department, d->department)) {
         DRETURN(-2);
      }

      d->granted_pe = (char *) read_json(document, "granted_pe", NONE_STR);
      if (options->granted_pe != nullptr && sge_strnullcmp(options->granted_pe, d->granted_pe)) {
         DRETURN(-2);
      }

      d->slots = read_json(document, "slots", (uint32_t)0);
      if ((options->slots > 0) && (options->slots != d->slots)) {
         DRETURN(-2);
      }

      d->ar = read_json(document, "arid", (uint32_t)0);
      if ((options->ar_number > 0) && (options->ar_number != d->ar)) {
         DRETURN(-2);
      }

      d->priority = read_json(document, "priority", (uint32_t)0);
      d->submission_time = read_json(document, "submission_time", (uint64_t)0);
      d->submission_command_line = read_json(document, "submit_cmd_line", nullptr);
      //d->ar_submission_time = read_json(document, "ar_submission_time", (uint64_t)0);

      //d->category = read_json(document, "category", NONE_STR);

      d->failed = read_json(document, "failed", (uint32_t)0);
      d->exit_status = read_json(document, "exit_status", (uint32_t)0);
      d->deleted_by = read_json(document, "deleted_by", nullptr);

      if (document.HasMember("usage")) {
         const rapidjson::Value &json_usage_list = document["usage"].GetObject();
         // Records written before custom usage group names were checked can contain a
         // custom group named rusage, eusage or usage next to the built-in objects.
         // The built-in object is the first one of its name, and usage stands for eusage
         // only in records of older versions, which have no eusage object.
         bool have_rusage = false;
         bool have_eusage = false;
         const bool usage_is_eusage = !json_usage_list.HasMember("eusage");
         for (rapidjson::Value::ConstMemberIterator itr = json_usage_list.MemberBegin(); itr != json_usage_list.MemberEnd(); ++itr) {
            const char *group_name = itr->name.GetString();
            if (!have_rusage && sge_strnullcmp(group_name, "rusage") == 0) {
               have_rusage = true;
               const rapidjson::Value &json_usage = itr->value;
               d->ru_wallclock = read_json(json_usage, "ru_wallclock", (uint32_t) 0);
               d->ru_utime = read_json(json_usage, "ru_utime", 0.0);
               d->ru_stime = read_json(json_usage, "ru_stime", 0.0);
               d->ru_maxrss = read_json(json_usage, "ru_maxrss", (uint32_t) 0);
               d->ru_ixrss = read_json(json_usage, "ru_ixrss", (uint32_t) 0);
               d->ru_ismrss = read_json(json_usage, "ru_ismrss", (uint32_t) 0);
               d->ru_idrss = read_json(json_usage, "ru_idrss", (uint32_t) 0);
               d->ru_isrss = read_json(json_usage, "ru_isrss", (uint32_t) 0);
               d->ru_minflt = read_json(json_usage, "ru_minflt", (uint32_t) 0);
               d->ru_majflt = read_json(json_usage, "ru_majflt", (uint32_t) 0);
               d->ru_nswap = read_json(json_usage, "ru_nswap", (uint32_t) 0);
               d->ru_inblock = read_json(json_usage, "ru_inblock", (uint32_t) 0);
               d->ru_oublock = read_json(json_usage, "ru_oublock", (uint32_t) 0);
               d->ru_msgsnd = read_json(json_usage, "ru_msgsnd", (uint32_t) 0);
               d->ru_msgrcv = read_json(json_usage, "ru_msgrcv", (uint32_t) 0);
               d->ru_nsignals = read_json(json_usage, "ru_nsignals", (uint32_t) 0);
               d->ru_nvcsw = read_json(json_usage, "ru_nvcsw", (uint32_t) 0);
               d->ru_nivcsw = read_json(json_usage, "ru_nivcsw", (uint32_t) 0);
            } else if (!have_eusage && (sge_strnullcmp(group_name, "eusage") == 0 ||
                                        (usage_is_eusage && sge_strnullcmp(group_name, "usage") == 0))) {
               have_eusage = true;
               const rapidjson::Value &json_usage = itr->value;
               d->wallclock = read_json(json_usage, "wallclock", 0.0);
               d->cpu = read_json(json_usage, "cpu", 0.0);
               d->mem = read_json(json_usage, "mem", 0.0);
               d->io = read_json(json_usage, "io", 0.0);
               d->ioops = read_json(json_usage, "ioops", 0.0);
               d->iow = read_json(json_usage, "iow", 0.0);
               d->maxvmem = read_json(json_usage, "maxvmem", 0.0);
               d->maxrss = read_json(json_usage, "maxrss", 0.0);

               // maxpss is only available if ENABLE_MEM_DETAILS is set to TRUE
               if (json_usage.HasMember("maxpss")) {
                  d->maxpss = read_json(json_usage, "maxpss", 0.0);
               } else {
                  d->maxpss = DBL_MAX;
               }
            } else {
               // Non-standard usage sub-object (e.g. "process", "gpu", or any
               // custom usage_patterns group). Each entry is either a JSON
               // number (populates UA_value) or a JSON string (populates
               // UA_svalue, CS-849). Other JSON types are skipped.
               const rapidjson::Value &json_usage = itr->value;
               for (rapidjson::Value::ConstMemberIterator usage_itr = json_usage.MemberBegin(); usage_itr != json_usage.MemberEnd(); ++usage_itr) {
                  const char *name = usage_itr->name.GetString();
                  // other_usage is a raw member of sge_rusage_type; the free forms keep its creation
                  if (ocs::tom::find_first<UA_name>(d->other_usage, name)) {
                     continue;   // first-wins per pre-CS-849 semantics
                  }
                  ocs::tom::Element<ocs::tom::UA> ep_ua = ocs::tom::emplace_back<UA_name>(&(d->other_usage), name);
                  if (ep_ua == nullptr) {
                     continue;
                  }
                  if (usage_itr->value.IsString()) {
                     ocs::tom::set<UA_svalue>(ep_ua, usage_itr->value.GetString());
                  } else if (usage_itr->value.IsNumber()) {
                     ocs::tom::set<UA_value>(ep_ua, usage_itr->value.GetDouble());
                  }
                  // other types (bool, array, object, null): entry created
                  // with a name but no value — matches the shepherd-side
                  // "drop unexpected types" policy
               }
            }
         }
      }

      d->pe_taskid = read_json(document, "pe_taskid", nullptr);

      // if we got here then we found at least one matching job
      options->jobfound = 1;
   }

   DRETURN(0);
}


