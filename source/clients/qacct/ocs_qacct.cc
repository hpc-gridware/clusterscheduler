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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <grp.h>
#include <ctime>
#include <fnmatch.h>
#include <cerrno>
#include <filesystem>

#include "rapidjson/document.h"

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
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

#include "comm/commlib.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "sched/sge_select_queue.h"

#include "sge.h"
#include "sig_handlers.h"
#include "execution_states.h"
#include "sge_rusage.h"
#include "basis_types.h"
#include "msg_common.h"
#include "msg_history.h"
#include "msg_qacct.h"

typedef struct {
   size_t host;
   size_t queue;
   size_t group;
   size_t owner;
   size_t project;
   size_t department;
   size_t granted_pe;
   size_t slots;
   size_t arid;
} sge_qacct_columns;

typedef struct {
   char *group;
   char *host;
   const char *owner;
   const char *job_name;
   const char *project;
   const char *department;
   const char *account;
   const char *granted_pe;
   const char *complexes;
   u_long32 job_number;
   u_long32 slots;
   u_long32 ar_number;
   int jobflag;
   int ownerflag;
   int groupflag;
   int jobfound;
   int complexflag;
   int queueflag;
   int projectflag;
   int departmentflag;
   int hostflag;
   int accountflag;
   int granted_peflag;
   int slotsflag;
   int arflag;
   u_long32 taskstart;
   u_long32 taskend;
   u_long32 taskstep;
   u_long64 begin_time;
   u_long64 end_time;
   lList *queue_name_list;
} sge_qacct_options;

static void qacct_usage(FILE *err_fp);
static void print_full(int length, const char* string);
static void print_full_ulong(int length, u_long32 value); 
static void calc_column_sizes(const lListElem* ep, sge_qacct_columns* column_size_data );
static void showjob(sge_rusage_type *dusage);
static bool get_qacct_lists(lList **alpp,
                            lList **ppcomplex, lList **ppqeues, lList **ppexechosts,
                            lList **hgrp_l);
static void free_qacct_lists(lList **ppcomplex, lList **ppqeues, lList **ppexechosts, lList **hgrp_l);
static int sge_read_rusage(FILE *f, sge_rusage_type *d, sge_qacct_options *options, char *szLine, size_t size);

/*
** statics
*/
static FILE *fp = nullptr;

/*
** NAME
**   main
** PARAMETER
**
** RETURN
**    0     - ok
**   -1     - invalid command line
**   < -1   - errors
** EXTERNAL
**   fp
**   path
**   me
**   prognames
** DESCRIPTION
**   main routine for qacct SGE client
*/
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qacct");
   int ret = 0;
   u_long32 days;
   sge_qacct_columns column_sizes;
   int beginflag=0;
   int endflag=0;
   int daysflag=0;
   bool summary_view = false;
   int ignored_jobs = 0;             /* counter of ignored jobs for accounting */

   sge_qacct_options options;

   sge_rusage_type dusage;
   sge_rusage_type totals;
   int ii;
   lList *complex_options = nullptr;
   lList *centry_list = nullptr;
   lList *queue_list = nullptr;
   lList *exechost_list = nullptr;
   lList *hgrp_list = nullptr;
   lList *queueref_list = nullptr;
   lList *sorted_list = nullptr;
   lSortOrder *sort_order = nullptr;
   int is_path_setup = 0;   
   u_long32 line = 0;
   const char *acct_file = nullptr;
   std::string filename{};
   lList *alp = nullptr;

   char szLine[MAX_STRING_SIZE * 10];
   size_t szLine_size = sizeof(szLine);

   sge_setup_sig_handlers(QACCT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QACCT, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      goto QACCT_EXIT;
   }

   memset(&totals, 0, sizeof(totals));
   memset(&options, 0, sizeof(options));
   options.begin_time = U_LONG64_MAX;
   options.end_time = U_LONG64_MAX;

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
               u_long32 gid;
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
               lAddElemStr(&queueref_list, QR_name, argv[++ii], QR_Type);
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
               lList *answer_list = nullptr;
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
            lList* task_id_range_list = nullptr;
            lList* answer = nullptr;

            ii++;
            range_list_parse_from_string(&task_id_range_list, &answer,
                                         argv[ii], false, true, INF_NOT_ALLOWED);
            if (task_id_range_list == nullptr) {
               lFreeList(&answer);
               fprintf(stderr, MSG_HISTORY_INVALIDLISTOFTASKIDRANGES_S , argv[ii]);
               fprintf(stderr, "\n");
               qacct_usage(stderr);
               DRETURN(1);
            }
            options.taskstart = lGetUlong(lFirst(task_id_range_list), RN_min);
            options.taskend = lGetUlong(lFirst(task_id_range_list), RN_max);
            options.taskstep = lGetUlong(lFirst(task_id_range_list), RN_step);
            if (options.taskstep == 0) {
               options.taskstep = 1;
            }

            lFreeList(&task_id_range_list);
         }
      }
      /*
      ** time options
      ** begin time
      */
      else if (!strcmp("-b", argv[ii])) {
         if (argv[ii+1]) {
            u_long32 tmp_begin_time;

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
            u_long32 tmp_end_time;

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
         options.end_time = U_LONG64_MAX;
      }
   }
   if (!beginflag) {
      if (endflag && daysflag) {
         options.begin_time = options.end_time - sge_gmt32_to_gmt64(days*24*3600);
      } else if (daysflag) {
         options.begin_time = sge_get_gmt64() - sge_gmt32_to_gmt64(days*24*3600);
      } else {
         options.begin_time = U_LONG64_MAX;
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
      const lListElem *qref_pattern = nullptr;
      const char *name = nullptr;
      bool has_hostname = false;
      bool has_domain = true;

      for_each_ep(qref_pattern, queueref_list) {
         name = lGetString(qref_pattern, QR_name); 
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
         for_each_ep(qref_pattern, queueref_list) {
            dstring qi_name = DSTRING_INIT;
            const char *tmp_str = nullptr;
            name = lGetString(qref_pattern, QR_name); 
           
            sge_dstring_copy_string(&qi_name, name); 
           
            if ((tmp_str = strchr(name, '@')) == nullptr){
               sge_dstring_append(&qi_name, "@*");
            } else if (*(tmp_str+1) == '\0'){
               sge_dstring_append(&qi_name, "*");
            }
            
            lAddElemStr(&options.queue_name_list, QR_name, sge_dstring_get_string(&qi_name), QR_Type);

            sge_dstring_free(&qi_name);
         }   
      }
      if (options.complexflag || (queueref_list && has_domain)) {
         /*
         ** parsing complex flags and initialising complex list
         */
         bool found_something;

         complex_options = centry_list_parse_from_string(nullptr, options.complexes, true);
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

            qref_list_resolve(queueref_list, nullptr, &options.queue_name_list,
                           &found_something, queue_list, hgrp_list, true, true);
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
      lWriteListTo(centry_list, stdout);
      printf("queue instances:\n");
      lWriteListTo(queue_list, stdout);
      printf("exec hosts:\n");
      lWriteListTo(exechost_list, stdout);
      printf("host groups\n");
      lWriteListTo(hgrp_list, stdout);
   }

   fp = fopen(acct_file, "r");
   if (fp == nullptr) {
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
      sorted_list = lCreateList("sorted_list", QAJ_Type);
      sort_order = lParseSortOrderVarArg(QAJ_Type, "%I+ %I+ %I+ %I+ %I+ %I+ %I+ %I+ %I+",
                                         QAJ_host,
                                         QAJ_queue,
                                         QAJ_group,
                                         QAJ_owner,
                                         QAJ_project,
                                         QAJ_department,
                                         QAJ_granted_pe,
                                         QAJ_slots,
                                         QAJ_arid);
      summary_view = true;
      if (sorted_list == nullptr || sort_order == nullptr) {
         ERROR(SFNMAX, MSG_HISTORY_NOTENOUGTHMEMORYTOCREATELIST);
         goto QACCT_EXIT;
      }
   }

   memset(&dusage, 0, sizeof(dusage));

   /* main loop */
   while (!shut_me_down) {
      int i_ret;
      line++;

      i_ret = sge_read_rusage(fp, &dusage, &options, szLine, szLine_size);
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
         lListElem *queue;
         int selected;
     
         sge_dstring_sprintf(&qi,"%s@%s", dusage.qname, dusage.hostname );
         queue = cqueue_list_locate_qinstance_msg(queue_list, sge_dstring_get_string(&qi), false);
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

         if (centry_list_fill_request(complex_options, &alp, centry_list, true, true, false)) {
            answer_list_output(&alp);
            goto QACCT_EXIT;
         }

         selected = sge_select_queue(complex_options, queue, nullptr, exechost_list,
                    centry_list, true, 1, nullptr, nullptr, nullptr);
  
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
         lListElem *ep = lFirstRW(sorted_list);

         /*
         ** find either the correct place to insert the next element
         ** or the existing element to increase
         */

         while (ep && ((options.slotsflag && (lGetUlong(ep, QAJ_slots) != dusage.slots)) || 
                (options.granted_peflag && (sge_strnullcmp(lGetString(ep, QAJ_granted_pe), dusage.granted_pe))) || 
                (options.departmentflag && (sge_strnullcmp(lGetString(ep, QAJ_department), dusage.department))) ||
                (options.projectflag && (sge_strnullcmp(lGetString(ep, QAJ_project), dusage.project))) ||
                (options.ownerflag && (sge_strnullcmp(lGetString(ep, QAJ_owner), dusage.owner))) ||
                (options.hostflag && (sge_hostcmp(lGetHost(ep, QAJ_host), dusage.hostname))) || 
                (options.queueflag && (sge_strnullcmp(lGetString(ep, QAJ_queue), dusage.qname))) ||
                (options.groupflag && (sge_strnullcmp(lGetString(ep, QAJ_group) , dusage.group))) ||
                (options.arflag && (lGetUlong(ep, QAJ_arid) != dusage.ar))
                )){
             ep = lNextRW(ep);
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

            lAddDouble(ep, QAJ_ru_wallclock, dusage.ru_wallclock);

            lAddDouble(ep, QAJ_ru_utime, dusage.ru_utime);
            lAddDouble(ep, QAJ_ru_stime, dusage.ru_stime);
            lAddDouble(ep, QAJ_cpu, dusage.cpu);
            lAddDouble(ep, QAJ_mem, dusage.mem);
            lAddDouble(ep, QAJ_io, dusage.io);
            lAddDouble(ep, QAJ_iow, dusage.iow);
         } else {
            lListElem *new_ep;

            new_ep = lCreateElem(QAJ_Type);

            if (options.hostflag && dusage.hostname)
               lSetHost(new_ep, QAJ_host, dusage.hostname);
            if (options.queueflag && dusage.qname)
               lSetString(new_ep, QAJ_queue, dusage.qname);
            if (options.groupflag && dusage.group)
               lSetString(new_ep, QAJ_group, dusage.group);
            if (options.ownerflag && dusage.owner)
               lSetString(new_ep, QAJ_owner, dusage.owner);
            if (options.projectflag && dusage.project)
               lSetString(new_ep, QAJ_project, dusage.project);
            if (options.departmentflag && dusage.department)
               lSetString(new_ep, QAJ_department, dusage.department);
            if (options.granted_peflag && dusage.granted_pe)
               lSetString(new_ep, QAJ_granted_pe, dusage.granted_pe);
            if (options.slotsflag)
               lSetUlong(new_ep, QAJ_slots, dusage.slots);
            if (options.arflag)
               lSetUlong(new_ep, QAJ_arid, dusage.ar);

            lSetDouble(new_ep, QAJ_ru_wallclock, dusage.ru_wallclock);
            lSetDouble(new_ep, QAJ_ru_utime, dusage.ru_utime);
            lSetDouble(new_ep, QAJ_ru_stime, dusage.ru_stime);
            lSetDouble(new_ep, QAJ_cpu, dusage.cpu);
            lSetDouble(new_ep, QAJ_mem, dusage.mem);
            lSetDouble(new_ep, QAJ_io,  dusage.io);
            lSetDouble(new_ep, QAJ_iow, dusage.iow);                         

            lInsertSorted(sort_order, new_ep, sorted_list);
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
   FCLOSE(fp);
   fp = nullptr;

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
         free_qacct_lists(&centry_list, &queue_list, &exechost_list, &hgrp_list);
         sge_exit(0);
      }
   } else if (options.taskstart && options.taskend && options.taskstep) {
      ERROR(SFNMAX, MSG_HISTORY_TOPTIONREQUIRESJOPTION);
      qacct_usage(stderr);
      free_qacct_lists(&centry_list, &queue_list, &exechost_list, &hgrp_list);
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
 
   calc_column_sizes(lFirst(sorted_list), &column_sizes);
   {
      const lListElem *ep = nullptr;
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
         ep = lFirst(sorted_list);
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
            print_full(column_sizes.host, ((cp = lGetHost(ep, QAJ_host)) ? cp : ""));
         }
         if (options.queueflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.queue, ((cp = lGetString(ep, QAJ_queue)) ? cp : ""));
         }
         if (options.group != nullptr) {
            print_full(column_sizes.group, options.group);
         } else if (options.groupflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.group, ((cp = lGetString(ep, QAJ_group)) ? cp : ""));
         }
         if (options.owner != nullptr) {
            print_full(column_sizes.owner, options.owner);
         }
         else if (options.ownerflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.owner, ((cp = lGetString(ep, QAJ_owner)) ? cp : "") );
         }
         if (options.project != nullptr) {
              print_full(column_sizes.project, options.project);
         }
         else if (options.projectflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.project ,((cp = lGetString(ep, QAJ_project)) ? cp : ""));
         }
         if (options.department != nullptr) {
            print_full(column_sizes.department, options.department);
         } else if (options.departmentflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.department, ((cp = lGetString(ep, QAJ_department)) ? cp : ""));
         }
         if (options.granted_pe != nullptr) {
            print_full(column_sizes.granted_pe, options.granted_pe);
         } else if (options.granted_peflag) {
            if (ep == nullptr) {
               break;
            }
            print_full(column_sizes.granted_pe, ((cp = lGetString(ep, QAJ_granted_pe)) ? cp : ""));
         }         
         if (options.slots > 0) {
            print_full_ulong(column_sizes.slots, options.slots);
         } else if (options.slotsflag) {
            if (ep == nullptr) {
               break;
            }
            print_full_ulong(column_sizes.slots, lGetUlong(ep, QAJ_slots));
         }

         if (options.ar_number > 0) {
            print_full_ulong(column_sizes.arid, options.ar_number);
         } else if (options.arflag) {
            if (ep == nullptr) {
               break;
            }
            print_full_ulong(column_sizes.arid, lGetUlong(ep, QAJ_arid));
         }         
         
         if (summary_view) {
             printf("%13.0f %13.3f %13.3f %13.3f %18.3f %18.3f %18.3f\n",
                   lGetDouble(ep, QAJ_ru_wallclock),
                   lGetDouble(ep, QAJ_ru_utime),
                   lGetDouble(ep, QAJ_ru_stime),
                   lGetDouble(ep, QAJ_cpu),
                   lGetDouble(ep, QAJ_mem),
                   lGetDouble(ep, QAJ_io),
                   lGetDouble(ep, QAJ_iow));

            ep = lNext(ep);
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

   lFreeList(&sorted_list);
   lFreeSortOrder(&sort_order);
 
   /*
   ** problem: other clients evaluate some status here
   */
   sge_prof_cleanup();
   sge_free(&(options.group));
   sge_free(&(options.host));
   free_qacct_lists(&centry_list, &queue_list, &exechost_list, &hgrp_list);
   sge_exit(0);
   DRETURN(0);

FCLOSE_ERROR:
   ERROR(MSG_FILE_ERRORCLOSEINGXY_SS, acct_file, strerror(errno));
QACCT_EXIT:
   ret = 1;
QACCT_EXIT_BUT_NO_ERROR:
   sge_prof_cleanup();
   lFreeList(&sorted_list);
   lFreeSortOrder(&sort_order);
   sge_free(&(options.group));
   sge_free(&(options.host));
   free_qacct_lists(&centry_list, &queue_list, &exechost_list, &hgrp_list);
   sge_exit(ret);
   DRETURN(ret);
}

static void print_full_ulong(int full_length, u_long32 value) {
   char tmp_buf[100];

   DENTER(TOP_LAYER);
   snprintf(tmp_buf, sizeof(tmp_buf), "%5" sge_fu32, value);
   print_full(full_length, tmp_buf); 
   DRETURN_VOID;
}

static void print_full(int full_length, const char* string) {

   int string_length=0;

   DENTER(TOP_LAYER);
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

static void calc_column_sizes(const lListElem* ep, sge_qacct_columns* column_size_data) {
   const lListElem* lep = nullptr;
   DENTER(TOP_LAYER);
   
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
         tmp_string = lGetHost(lep, QAJ_host);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->host <= tmp_length) {
               column_size_data->host  = tmp_length + 1;
            }
         } 
         /* queue */
         tmp_string = lGetString(lep, QAJ_queue);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->queue <= tmp_length) {
               column_size_data->queue  = tmp_length + 1;
            }
         } 
         /* group */
         tmp_string = lGetString(lep, QAJ_group) ;
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->group <= tmp_length) {
               column_size_data->group  = tmp_length + 1;
            }
         } 
         /* owner */
         tmp_string = lGetString(lep, QAJ_owner);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->owner <= tmp_length) {
               column_size_data->owner  = tmp_length + 1;
            }
         } 
         /* project */
         tmp_string = lGetString(lep, QAJ_project);
         if (tmp_string != nullptr) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->project <= tmp_length) {
               column_size_data->project  = tmp_length + 1;
            }
         } 

         /* department  */
         tmp_string = lGetString(lep, QAJ_department);
         if ( tmp_string != nullptr ) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->department <= tmp_length) {
               column_size_data->department  = tmp_length + 1;
            }
         } 
         /* granted_pe */
         tmp_string = lGetString(lep, QAJ_granted_pe) ;
         if ( tmp_string != nullptr ) {
            tmp_length = strlen(tmp_string);
            if (column_size_data->granted_pe <= tmp_length) {
               column_size_data->granted_pe  = tmp_length + 1;
            }
         } 

         /* slots */
         snprintf(tmp_buf, sizeof(tmp_buf), "%5" sge_fu32, lGetUlong(lep, QAJ_slots));
         tmp_length = strlen(tmp_buf);
         if (column_size_data->slots <= tmp_length) {
            column_size_data->slots  = tmp_length + 1;
         }

         /* advance reservations */
         snprintf(tmp_buf, sizeof(tmp_buf), "%5" sge_fu32, lGetUlong(lep, QAJ_arid));
         tmp_length = strlen(tmp_buf);
         if (column_size_data->arid <= tmp_length) {
            column_size_data->arid = tmp_length + 1;
         }

         lep = lNext(lep);
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
static void qacct_usage(FILE *err_fp)
{
   dstring ds;
   char buffer[256];

   DENTER(TOP_LAYER);

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


/*
** NAME
**   showjob
** PARAMETER
**   dusage   - pointer to struct sge_rusage_type, representing
**              a line in the SGE accounting file
** RETURN
**   none
** EXTERNAL
**   none
** DESCRIPTION
**   detailed display of job accounting data
*/
#define SHOWJOB_STRING         "%-35.34s%s\n"
#define SHOWJOB_STRING_NO_DATA "%-35.34s-/-\n"
#define SHOWJOB_STRING_20      "%-35.34s%-20s\n"
#define SHOWJOB_STRING_20_NOLF "%-35.34s%-20s"
#define SHOWJOB_U32_20         "%-35.34s%-20" sge_fuu32 "\n"
#define SHOWJOB_U32_FAILED     "%-35.34s%-3" sge_fuu32" %s %s\n"
#define SHOWJOB_FLOAT_0        "%-35.34s%-13.0f\n"
#define SHOWJOB_FLOAT_3        "%-35.34s%-13.3f\n"
#define SHOWJOB_FLOAT_18_0     "%-35.34s%-18.0f\n"
#define SHOWJOB_FLOAT_18_3     "%-35.34s%-18.3f\n"
static void showjob(sge_rusage_type *dusage) {
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

   if (dusage->pe_taskid != nullptr) {
      printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_PE_TASKID, dusage->pe_taskid);
   } else {
      printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_PE_TASKID, NONE_STR);
   }

   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_ACCOUNT, (dusage->account ? dusage->account : MSG_HISTORY_SHOWJOB_NULL ));
   printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_PRIORITY, dusage->priority);
   printf(SHOWJOB_STRING_20,MSG_HISTORY_SHOWJOB_QSUBTIME, sge_ctime64(dusage->submission_time, pdstr_buffer));
   printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_SUBMITCMDLINE, dusage->submission_command_line);

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

   printf(SHOWJOB_FLOAT_3,   MSG_HISTORY_SHOWJOB_WALLCLOCK,      dusage->wallclock);
   printf(SHOWJOB_FLOAT_3,   MSG_HISTORY_SHOWJOB_CPU,          dusage->cpu);
   printf(SHOWJOB_FLOAT_18_3,   MSG_HISTORY_SHOWJOB_MEM,          dusage->mem);
   printf(SHOWJOB_FLOAT_18_3,   MSG_HISTORY_SHOWJOB_IO,           dusage->io);
   printf(SHOWJOB_FLOAT_18_3,   MSG_HISTORY_SHOWJOB_IOW,          dusage->iow);

#if 0
   /* enable this to get unit of memory value (G,M,K) */
   /* CR TODO: create units for complete qacct output: IZ: #1047 */
   sge_dstring_clear(pdstr_buffer);
   double_print_memory_to_dstring(dusage->maxvmem, pdstr_buffer);
   printf(SHOWJOB_STRING,        MSG_HISTORY_SHOWJOB_MAXVMEM,      sge_dstring_get_string(pdstr_buffer));
#else
   printf(SHOWJOB_FLOAT_18_0,   MSG_HISTORY_SHOWJOB_MAXVMEM,      dusage->maxvmem);
#endif

   printf(SHOWJOB_FLOAT_18_0,   MSG_HISTORY_SHOWJOB_MAXRSS,       dusage->maxrss);

   if (dusage->ar != 0) {
      printf(SHOWJOB_U32_20,MSG_HISTORY_SHOWJOB_ARID, dusage->ar);              /* job-array task number */
   } else {
      printf(SHOWJOB_STRING,MSG_HISTORY_SHOWJOB_ARID, "undefined");
   }

   // print further usage values
   if (dusage->other_usage != nullptr) {
      const lListElem *ep;
      for_each_ep(ep, dusage->other_usage) {
         printf(SHOWJOB_FLOAT_18_3, lGetString(ep, UA_name), lGetDouble(ep, UA_value));
      }
   }

   sge_dstring_free(pdstr_buffer);
}

/*
** NAME
**   get_qacct_lists
** PARAMETER
**   ctx         - communication context
**   alpp        - list pointer-pointer answer list
**   ppcentries  - list pointer-pointer to be set to the complex list, CX_Type, can be nullptr
**   ppqueues    - list pointer-pointer to be set to the queues list, QU_Type, has to be set
**   ppexechosts - list pointer-pointer to be set to the exechosts list,EH_Type, can be nullptr
**   hgrp_l      - host group list, HGRP_Type, can be nullptr
**
** RETURN
**   none
**
** EXTERNAL
**
** DESCRIPTION
**   retrieves the lists from qmaster
**   programmed after the get_all_lists() function in qstat.c,
**   but gets queue list in a different way
**   none of the old list types is explicitly used
**   function does its own error handling by calling SGE_EXIT
**   problem: exiting is against the philosophy, restricts usage to clients
*/
static bool get_qacct_lists(
lList **alpp,
lList **ppcentries,
lList **ppqueues,
lList **ppexechosts,
lList **hgrp_l
) {
   lCondition *where = nullptr;
   lEnumeration *what = nullptr;
   int ce_id = 0, eh_id = 0, q_id = 0, hgrp_id = 0;
   ocs::gdi::Request gdi_multi{};

   DENTER(TOP_LAYER);

   /*
   ** GET SGE_CE_LIST
   */
   if (ppcentries) {
      what = lWhat("%T(ALL)", CE_Type);
      ce_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::TargetValue::SGE_CE_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
      lFreeWhat(&what);

      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }
   /*
   ** GET SGE_EH_LIST
   */
   if (ppexechosts) {
      where = lWhere("%T(%I!=%s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
      what = lWhat("%T(ALL)", EH_Type);
      eh_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, what, true);
      lFreeWhat(&what);
      lFreeWhere(&where);

      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }

   /*
   ** hgroup
   */
   if (hgrp_l) {
      what = lWhat("%T(ALL)", HGRP_Type);
      hgrp_id = gdi_multi.request(alpp, ocs::Mode::RECORD, ocs::gdi::Target::SGE_HGRP_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
      lFreeWhat(&what);

      if (answer_list_has_error(alpp)) {
         DRETURN(false);
      }
   }
   /*
   ** GET SGE_QUEUE_LIST
   */
   what = lWhat("%T(ALL)", QU_Type);
   q_id = gdi_multi.request(alpp, ocs::Mode::SEND, ocs::gdi::Target::SGE_CQ_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, what, true);
   gdi_multi.wait();
   lFreeWhat(&what);

   if (answer_list_has_error(alpp)) {
      DRETURN(false);
   }

   /*
   ** handle results
   */
   /* --- complex */
   if (ppcentries) {
      gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_CE_LIST, ce_id, ppcentries);
      if (answer_list_has_error(alpp)) { 
         DRETURN(false);
      }
   }

   /* --- exec host */
   if (ppexechosts) {
      gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_EH_LIST, eh_id, ppexechosts);
      if (answer_list_has_error(alpp)) { 
         DRETURN(false);
      }
   }

   /* --- queue */
   if (ppqueues) {
      gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_CQ_LIST, q_id, ppqueues);
      if (answer_list_has_error(alpp)) { 
         DRETURN(false);
      }
   }

   /* --- hgrp */
   if (hgrp_l) {
      gdi_multi.get_response(alpp, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_HGRP_LIST, hgrp_id, hgrp_l);
      if (answer_list_has_error(alpp)) { 
         DRETURN(false);
      }
   }
   /* --- end */
   DRETURN(true);
}

static void free_qacct_lists(lList **ppcentries, lList **ppqueues, lList **ppexechosts, lList **hgrp_l)
{
   lFreeList(ppcentries);
   lFreeList(ppqueues);
   lFreeList(ppexechosts);
   lFreeList(hgrp_l);
}

static int
sge_read_rusage_classic(char *line, sge_rusage_type *d, sge_qacct_options *options);

static int
sge_read_rusage_json(const char *line, sge_rusage_type *d, sge_qacct_options *options);

static int
sge_read_rusage(FILE *f, sge_rusage_type *d, sge_qacct_options *options, char *szLine, size_t size) {
   DENTER(TOP_LAYER);

   int ret = 0;
   char *pc;
   int len;

   do {
      pc = fgets(szLine, size, f);
      if (pc == nullptr) {
         DRETURN(2);
      }
      len = strlen(szLine);
      if (szLine[len] == '\n') {
         szLine[len] = '\0';
      }
   } while (len <= 1 || szLine[0] == COMMENT_CHAR);

   /*
    * qname
    */
   if (*pc == '{') {
      // JSON @todo we could determine this once
      ret = sge_read_rusage_json(szLine, d, options);
   } else {
      // old colon separated file
      ret = sge_read_rusage_classic(szLine, d, options);
   }

   DRETURN(ret);
}

static bool matches_queue_name_list(sge_rusage_type *d, const lList *queue_name_list) {
   bool ret = false;

   DSTRING_STATIC(dstr, MAX_STRING_SIZE);
   const char *qinstance;
   qinstance = sge_dstring_sprintf(&dstr, "%s@%s", d->qname, d->hostname);

   const lListElem *ep;
   for_each_ep(ep, queue_name_list) {
      if (fnmatch(lGetString(ep, QR_name), qinstance, 0) == 0) {
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
   if (options->begin_time != U_LONG64_MAX && d->start_time < options->begin_time) {
      DRETURN(-2);
   }
   if (options->end_time != U_LONG64_MAX && d->start_time > options->end_time) {
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

   // we do not have wallclock but ru_wallclock
   d->wallclock = d->ru_wallclock;

   /* ... */ 
   options->jobfound=1;

   DRETURN(0);
}

static u_long32
read_json(const rapidjson::Value &json, const char *name, u_long32 default_value) {
   if (json.HasMember(name)) {
      return json[name].GetUint();
   }

   return default_value;
}

static u_long64
read_json(const rapidjson::Value &json, const char *name, u_long64 default_value) {
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
      d->job_number = read_json(document, "job_number", (u_long32)0);
      if (!options->jobflag && (options->job_number || options->job_name != nullptr)) {
         if (((d->job_number != options->job_number) && sge_patternnullcmp(d->job_name, options->job_name))) {
            DRETURN(-2);
         }
      }

      d->task_number = read_json(document, "task_number", (u_long32)0);
      if (!options->jobflag) {
         if (options->taskstart && options->taskend && options->taskstep) {
            if (d->task_number < options->taskstart || d->task_number > options->taskend ||
                !(((d->task_number - options->taskstart) % options->taskstep) == 0)) {
               DRETURN(-2);
            }
         }
      }

      d->start_time = read_json(document, "start_time", (u_long64)0);
      /*
      ** skipping jobs that never ran
      */
      if ((d->start_time == 0) && options->complexflag) {
         DPRINTF("skipping job that never ran\n");
         DRETURN(-2);
      }
      if (options->begin_time != U_LONG64_MAX && d->start_time < options->begin_time) {
         DRETURN(-2);
      }
      if (options->end_time != U_LONG64_MAX && d->start_time > options->end_time) {
         DRETURN(-2);
      }
      d->end_time = read_json(document, "end_time", (u_long64)0);

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

      d->slots = read_json(document, "slots", (u_long32)0);
      if ((options->slots > 0) && (options->slots != d->slots)) {
         DRETURN(-2);
      }

      d->ar = read_json(document, "arid", (u_long32)0);
      if ((options->ar_number > 0) && (options->ar_number != d->ar)) {
         DRETURN(-2);
      }

      d->priority = read_json(document, "priority", (u_long32)0);
      d->submission_time = read_json(document, "submission_time", (u_long64)0);
      d->submission_command_line = read_json(document, "submit_cmd_line", nullptr);
      //d->ar_submission_time = read_json(document, "ar_submission_time", (u_long64)0);

      //d->category = read_json(document, "category", NONE_STR);

      d->failed = read_json(document, "failed", (u_long32)0);
      d->exit_status = read_json(document, "exit_status", (u_long32)0);

      if (document.HasMember("usage")) {
         const rapidjson::Value &json_usage_list = document["usage"].GetObject();
         for (rapidjson::Value::ConstMemberIterator itr = json_usage_list.MemberBegin(); itr != json_usage_list.MemberEnd(); ++itr) {
            if (sge_strnullcmp(itr->name.GetString(), "rusage") == 0) {
               const rapidjson::Value &json_usage = itr->value;
               d->ru_wallclock = read_json(json_usage, "ru_wallclock", (u_long32) 0);
               d->ru_utime = read_json(json_usage, "ru_utime", 0.0);
               d->ru_stime = read_json(json_usage, "ru_stime", 0.0);
               d->ru_maxrss = read_json(json_usage, "ru_maxrss", (u_long32) 0);
               d->ru_ixrss = read_json(json_usage, "ru_ixrss", (u_long32) 0);
               d->ru_ismrss = read_json(json_usage, "ru_ismrss", (u_long32) 0);
               d->ru_idrss = read_json(json_usage, "ru_idrss", (u_long32) 0);
               d->ru_isrss = read_json(json_usage, "ru_isrss", (u_long32) 0);
               d->ru_minflt = read_json(json_usage, "ru_minflt", (u_long32) 0);
               d->ru_majflt = read_json(json_usage, "ru_majflt", (u_long32) 0);
               d->ru_nswap = read_json(json_usage, "ru_nswap", (u_long32) 0);
               d->ru_inblock = read_json(json_usage, "ru_inblock", (u_long32) 0);
               d->ru_oublock = read_json(json_usage, "ru_oublock", (u_long32) 0);
               d->ru_msgsnd = read_json(json_usage, "ru_msgsnd", (u_long32) 0);
               d->ru_msgrcv = read_json(json_usage, "ru_msgrcv", (u_long32) 0);
               d->ru_nsignals = read_json(json_usage, "ru_nsignals", (u_long32) 0);
               d->ru_nvcsw = read_json(json_usage, "ru_nvcsw", (u_long32) 0);
               d->ru_nivcsw = read_json(json_usage, "ru_nivcsw", (u_long32) 0);
            } else if (sge_strnullcmp(itr->name.GetString(), "eusage") == 0 ||
                       sge_strnullcmp(itr->name.GetString(), "usage") == 0) { // for backward compatibility
               const rapidjson::Value &json_usage = itr->value;
               d->wallclock = read_json(json_usage, "wallclock", 0.0);
               d->cpu = read_json(json_usage, "cpu", 0.0);
               d->mem = read_json(json_usage, "mem", 0.0);
               d->io = read_json(json_usage, "io", 0.0);
               d->iow = read_json(json_usage, "iow", 0.0);
               d->maxvmem = read_json(json_usage, "maxvmem", 0.0);
               d->maxrss = read_json(json_usage, "maxrss", 0.0);
            } else {
               const rapidjson::Value &json_usage = itr->value;
               for (rapidjson::Value::ConstMemberIterator usage_itr = json_usage.MemberBegin(); usage_itr != json_usage.MemberEnd(); ++usage_itr) {
                  const char *name = usage_itr->name.GetString();
                  double value = usage_itr->value.GetDouble();
                  lListElem *ep = lGetElemStrRW(d->other_usage, UA_name, name);
                  if (ep == nullptr) {
                     ep = lAddElemStr(&(d->other_usage), UA_name, name, UA_Type);
                     if (ep != nullptr) {
                        lSetDouble(ep, UA_value, value);
                     }
                  }
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


