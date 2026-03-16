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
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <memory>

#include "uti/ocs_Pattern.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_job.h"

#include "sched/sge_schedd_text.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QStatParameter.h"
#include "ocs_QStatModel.h"
#include "ocs_QStatSelectViewBase.h"
#include "ocs_QStatSelectViewPlain.h"
#include "ocs_QStatSelectViewXML.h"
#include "ocs_QStatSelectController.h"
#include "ocs_QStatGroupViewBase.h"
#include "ocs_QStatGroupViewPlain.h"
#include "ocs_QStatGroupViewXML.h"
#include "ocs_QStatGroupController.h"
#include "ocs_QStatDefaultController.h"
#include "ocs_QStatDefaultViewBase.h"
#include "ocs_QStatDefaultViewPlain.h"
#include "ocs_QStatDefaultViewXML.h"
#include "sig_handlers.h"
#include "ocs_client_job.h"
#include "ocs_client_print.h"
#include "basis_types.h"
#include "ocs_qstat_filter.h"
#include "ocs_qstat_xml.h"
#include "msg_clients_common.h"
#include "msg_qstat.h"


#define FORMAT_I_20 "%I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I "
#define FORMAT_I_10 "%I %I %I %I %I %I %I %I %I %I "
#define FORMAT_I_5 "%I %I %I %I %I "
#define FORMAT_I_2 "%I %I "
#define FORMAT_I_1 "%I "

static int qstat_show_job(lList *jid, ocs::QStatParameter &parameter, ocs::QStatModel &model);
static int qstat_show_job_info(ocs::QStatParameter &parameter);

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "qstat");
   lList *alp = nullptr;

   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(QSTAT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QSTAT, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   // parse command line parameters and options
   ocs::QStatParameter parameter;
   if (!parameter.parse_parameters(&alp, argv, environ)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   ocs::QStatModel model;
   if (!model.make_snapshot(&alp, parameter)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   int ret = 0;
   lList *answer_list = nullptr;

   // if -j, then only print job info and leave */
   if (parameter.output_mode_ == ocs::QStatParameter::OutputMode::JOB_INFO) {
      if (lGetNumberOfElem(parameter.jid_list_) > 0) {
         ret = qstat_show_job(parameter.jid_list_, parameter, model);
      } else {
         ret = qstat_show_job_info(parameter);
      }
   } else if (parameter.output_mode_ == ocs::QStatParameter::OutputMode::QSELECT) {
      std::unique_ptr<ocs::QStatSelectViewBase> view;
      if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
         view = std::make_unique<ocs::QStatSelectViewXML>(parameter);
      } else {
         view = std::make_unique<ocs::QStatSelectViewPlain>(parameter);
      }

      ocs::QStatSelectController controller;
      controller.process_request(parameter, model, *view);
   } else if (parameter.output_mode_== ocs::QStatParameter::OutputMode::QSTAT_GROUP) {
      std::unique_ptr<ocs::QStatGroupViewBase> view;
      if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
         view = std::make_unique<ocs::QStatGroupViewXML>();
      } else {
         view = std::make_unique<ocs::QStatGroupViewPlain>();
      }

      ocs::QStatGroupController controller;
      controller.process_request(parameter, model, *view);
   } else if (parameter.output_mode_== ocs::QStatParameter::OutputMode::QSTAT_DEFAULT) {

      // Regular output

      std::unique_ptr<ocs::QStatDefaultViewBase> view;
      if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
         view = std::make_unique<ocs::QStatDefaultViewXML>();
      } else {
         view = std::make_unique<ocs::QStatDefaultViewPlain>();
      }

      ret = qstat_no_group(&answer_list, parameter, model, *view);
      ocs::QStatDefaultController controller;
      controller.process_request(parameter, model, *view);
   } else {
      // not possible
   }

   answer_list_output(&answer_list);

   if (ret != 0) {
      sge_exit(1);
      return 1;
   }
   sge_exit(0);
   return 0;
}

/*
** qstat_show_job
** displays information about a given job
** to be extended
**
** returns 0 on success, non-zero on failure
*/
static int
qstat_show_job(lList *jid_list, ocs::QStatParameter &parameter, ocs::QStatModel &model) {
   DENTER(TOP_LAYER);
   const lListElem *j_elem = 0;
   lList* jlp = nullptr;
   lList* ilp = nullptr;
   const lListElem* aep = nullptr;
   lCondition *where = nullptr, *newcp = nullptr;
   lEnumeration* what = nullptr;
   lList* alp = nullptr;
   bool schedd_info = true;
   bool jobs_exist = true;
   const lListElem* mes;
   const lListElem *tmpElem;

   /* get job scheduling information */
   what = lWhat("%T(ALL)", SME_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_SME_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &ilp, nullptr, what);
   lFreeWhat(&what);

   if (parameter.output_format_ != ocs::QStatParameter::OutputFormat::XML) {
      for_each_ep(aep, alp) {
         if (lGetUlong(aep, AN_status) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            schedd_info = false;
         }
      }
   }
   lFreeList(&alp);

   /* build 'where' for all jobs */
   where = nullptr;
   for_each_ep(j_elem, jid_list) {
      const char *job_name = lGetString(j_elem, ST_name);

      if (isdigit(job_name[0])) {
         u_long32 jid = atol(lGetString(j_elem, ST_name));
         newcp = lWhere("%T(%I==%u)", JB_Type, JB_job_number, jid);
      } else {
         newcp = lWhere("%T(%I p= %s)", JB_Type, JB_job_name, job_name);
      }
      if (newcp) {
         if (!where) {
            where = newcp;
         } else {
            where = lOrWhere(where, newcp);
         }
      }
   }
   what = lWhat("%T(%I%I%I%I"
                    "%I%I%I%I%I"
                    "%I%I%I%I"
                    "%I%I%I%I"
                    "%I%I%I"

                    "%I%I%I"
                    "%I->%T(%I%I%I"
                    "%I%I%I%I"
                    "%I%I%I"
                    "%I)%I"

                    "%I%I%I->%T"
                    "(%I)%I->%T(%I)%I"
                    "%I%I%I%I%I"
                    "%I%I%I%I"
                    "%I%I%I%I%I"

                    "%I%I%I"
                    "%I%I%I%I%I"
                    "%I%I%I%I"
                    "%I%I%I)",

            JB_Type, JB_job_number, JB_ar, JB_exec_file, JB_submission_time,
            JB_submission_command_line, JB_owner, JB_uid, JB_group, JB_gid,
            JB_account, JB_merge_stderr, JB_mail_list, JB_project,
            JB_department, JB_notify, JB_job_name, JB_stdout_path_list,
            JB_jobshare, JB_request_set_list, JB_shell_list,

            JB_env_list, JB_job_args, JB_script_file,
            JB_ja_tasks, JAT_Type, JAT_state, JAT_status, JAT_hold,
            JAT_task_number, JAT_scaled_usage_list, JAT_job_restarted, JAT_task_list,
            JAT_message_list, JAT_start_time, JAT_granted_resources_list,
            JAT_granted_destin_identifier_list, JB_context,

            JB_cwd, JB_stderr_path_list, JB_jid_predecessor_list, JRE_Type,
            JRE_job_number, JB_jid_successor_list, JRE_Type, JRE_job_number, JB_deadline,
            JB_execution_time, JB_checkpoint_name, JB_checkpoint_attr, JB_checkpoint_interval, JB_directive_prefix,
            JB_reserve, JB_mail_options, JB_stdin_path_list, JB_priority,
            JB_restart, JB_verify, JB_script_size, JB_pe, JB_pe_range,

            JB_jid_request_list, JB_ja_ad_request_list, JB_verify_suitable_queues,
            JB_soft_wallclock_gmt, JB_hard_wallclock_gmt, JB_override_tickets, JB_version, JB_ja_structure,
            JB_type, JB_binding, JB_ja_task_concurrency, JB_pty,
            JB_grp_list, JB_sync_options, JB_category_id);

   /* get job list */
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::TargetValue::SGE_JB_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &jlp, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);

   if (parameter.output_format_ == ocs::QStatParameter::OutputFormat::XML) {
      /* filter the message list to contain only jobs that have been requested.
         First remove all entries in the job_number_list that are not in the
         jbList. Then remove all entries (job_number_list, message_number and 
         message) from the message_list that have no jobs in them. 
      */
      for_each_ep(tmpElem, ilp) {
         lList *msgList = nullptr;
         lListElem *msgElem = nullptr;
         lListElem *tmp_msgElem = nullptr;
         msgList = lGetListRW(tmpElem, SME_message_list);
         msgElem = lFirstRW(msgList);
         while (msgElem) {            
            lList *jbList = nullptr;
            lListElem *jbElem = nullptr;
            lListElem *tmp_jbElem = nullptr;
            
            tmp_msgElem = lNextRW(msgElem);
            jbList = lGetListRW(msgElem, MES_job_number_list);
            jbElem = lFirstRW(jbList);
            
            while (jbElem) {
               tmp_jbElem = lNextRW(jbElem);
               if (lGetElemUlong(jlp, JB_job_number, lGetUlong(jbElem, ULNG_value)) == nullptr) {
                  lRemoveElem(jbList, &jbElem);
               }
               jbElem = tmp_jbElem;
            }
            if (lGetNumberOfElem(lGetList(msgElem, MES_job_number_list)) == 0) {
               lRemoveElem(msgList, &msgElem);
            }
            msgElem = tmp_msgElem;
         }         
      }
      
      xml_qstat_show_job(&jlp, &ilp,  &alp, &jid_list, parameter);
   
      lFreeList(&jlp);
      lFreeList(&alp);
      lFreeList(&jid_list);
      DRETURN(0);
   }

   for_each_ep(aep, alp) {
      if (lGetUlong(aep, AN_status) != STATUS_OK) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
         jobs_exist = false;
      }
   }
   lFreeList(&alp);
   if (!jobs_exist) {
      DRETURN(1);
   }

   /* does jop contain all information we requested? */
   if (lGetNumberOfElem(jlp) == 0) {
      lListElem *elem1, *elem2;

      // remove all pattern
      bool removed_pattern = false;
      elem2 = lFirstRW(jid_list);
      while ((elem1 = elem2) != nullptr) {
         elem2 = lNextRW(elem1);

         if (ocs::is_pattern(lGetString(elem1, ST_name))) {
            lDechainElem(jid_list, elem1);
            removed_pattern = true;
         }
      }

      // if there is still something missing then report an error
      int first_time = 1;
      if (lGetNumberOfElem(jid_list) > 0) {
         fprintf(stderr, "%s\n", MSG_QSTAT_FOLLOWINGDONOTEXIST);
         for_each_rw(elem1, jid_list) {
            if (!first_time) {
               fprintf(stderr, ", ");
            }
            first_time = 0;
            fprintf(stderr, "%s", lGetString(elem1, ST_name));
         }
         fprintf(stderr, "\n");
         sge_exit(1);
      } else {
         if (removed_pattern) {
            fprintf(stderr, "%s\n", MSG_QSTAT_FOUNDNOMATCHING);
         }
         sge_exit(0);
      }
   }

   /* print scheduler job information and global scheduler info */
   for_each_ep(j_elem, jlp) {
      u_long32 jid = lGetUlong(j_elem, JB_job_number);
      const lListElem *sme;
      const char *owner = lGetString(j_elem, JB_owner);
      bool show_job = job_is_visible(owner,  model.is_manager_);
      if (!show_job) {
         DTRACE;
         continue;
      }

      printf("==============================================================\n");
      /* print job information */
      cull_show_job(j_elem, 0, (parameter.full_listing_ & QSTAT_DISPLAY_BINDING) != 0 ? true : false);
      
      /* print scheduling information */
      if (schedd_info && (sme = lFirst(ilp))) {
         int first_run = 1;

         if (sme) {
            /* global schduling info */
            for_each_ep(mes, lGetList(sme, SME_global_message_list)) {
               if (first_run) {
                  printf("%s:                 ",MSG_SCHEDD_SCHEDULINGINFO);
                  first_run = 0;
               } else {
                  printf("%s", "                                 ");
               }
               printf("%s\n", lGetString(mes, MES_message));
            }

            /* job scheduling info */
            for_each_ep(mes, lGetList(sme, SME_message_list)) {
               const lListElem *mes_jid;

               for_each_ep(mes_jid, lGetList(mes, MES_job_number_list)) {
                  if (lGetUlong(mes_jid, ULNG_value) == jid) {
                     if (first_run) {
                        printf("%s:                 ",MSG_SCHEDD_SCHEDULINGINFO);
                        first_run = 0;
                     } else {
                        printf("%s", "                                 ");
                     }
                     printf("%s\n", lGetString(mes, MES_message));
                  }
               }
            }
         }
      }
   }

   lFreeList(&ilp);
   lFreeList(&jlp);
   DRETURN(0);
}

static int qstat_show_job_info(ocs::QStatParameter &parameter)
{
   lList *ilp = nullptr;
   lList *mlp = nullptr;
   const lListElem* aep = nullptr;
   lEnumeration* what = nullptr;
   lList* alp = nullptr;
   bool schedd_info = true;
   const lListElem* mes;
   int initialized = 0;
   u_long32 last_jid = 0;
   u_long32 last_mid = 0;
   char text[256], ltext[256];
   int ids_per_line = 0;
   int first_run = 1;
   int first_row = 1;
   lListElem *sme;
   const lListElem *jid_ulng = nullptr;

   DENTER(TOP_LAYER);

   /* get job scheduling information */
   what = lWhat("%T(ALL)", SME_Type);
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::SGE_SME_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, &ilp, nullptr, what);
   lFreeWhat(&what);
   if (parameter.output_format_ == ocs::QStatParameter::OutputFormat::XML) {
      xml_qstat_show_job_info(&ilp, &alp, parameter);
   }
   else {
      for_each_ep(aep, alp) {
         if (lGetUlong(aep, AN_status) != STATUS_OK) {
            fprintf(stderr, "%s\n", lGetString(aep, AN_text));
            schedd_info = false;
         }
      }
      lFreeList(&alp);
      if (!schedd_info) {
         DRETURN(1);
      }

      sme = lFirstRW(ilp);
      if (sme) {
         /* print global schduling info */
         first_run = 1;
         for_each_ep(mes, lGetList(sme, SME_global_message_list)) {
            if (first_run) {
               printf("%s:                 ",MSG_SCHEDD_SCHEDULINGINFO);
               first_run = 0;
            }
            else
               printf("%s", "                            ");
            printf("%s\n", lGetString(mes, MES_message));
         }
         if (!first_run)
            printf("\n");

         first_run = 1;

         mlp = lGetListRW(sme, SME_message_list);
         lPSortList(mlp, "I+", MES_message_number);

         /* 
          * Remove all jids which have more than one entry for a MES_message_number
          * After this step the MES_messages are not correct anymore
          * We do not need this messages for the summary output
          */
         {
            lListElem *flt_msg, *flt_nxt_msg;
            lList *new_list;
            const lListElem *ref_msg, *ref_jid;

            new_list = lCreateList("filtered message list", MES_Type);

            flt_nxt_msg = lFirstRW(mlp);
            while ((flt_msg = flt_nxt_msg)) {
               lListElem *flt_jid, * flt_nxt_jid;
               int found_msg, found_jid;

               flt_nxt_msg = lNextRW(flt_msg);
               found_msg = 0;
               for_each_ep(ref_msg, new_list) {
                  if (lGetUlong(ref_msg, MES_message_number) == 
                      lGetUlong(flt_msg, MES_message_number)) {
                 
                  flt_nxt_jid = lFirstRW(lGetList(flt_msg, MES_job_number_list));
                  while ((flt_jid = flt_nxt_jid)) {
                     flt_nxt_jid = lNextRW(flt_jid);
                    
                     found_jid = 0; 
                     for_each_ep(ref_jid, lGetList(ref_msg, MES_job_number_list)) {
                        if (lGetUlong(ref_jid, ULNG_value) == 
                            lGetUlong(flt_jid, ULNG_value)) {
                           lRemoveElem(lGetListRW(flt_msg, MES_job_number_list), &flt_jid);
                           found_jid = 1;
                           break;
                        }
                     }
                     if (!found_jid) { 
                        lDechainElem(lGetListRW(flt_msg, MES_job_number_list), flt_jid);
                        lAppendElem(lGetListRW(ref_msg, MES_job_number_list), flt_jid);
                     } 
                  }
                  found_msg = 1;
               }
            }
            if (!found_msg) {
               lDechainElem(mlp, flt_msg);
               lAppendElem(new_list, flt_msg);
            }
         }
         lSetList(sme, SME_message_list, new_list);
         mlp = new_list;
      }

      text[0]=0;
      for_each_ep(mes, mlp) {
         lPSortList(lGetListRW(mes, MES_job_number_list), "I+", ULNG_value);

         for_each_ep(jid_ulng, lGetList(mes, MES_job_number_list)) {
            u_long32 mid;
            u_long32 jid = 0;
            int skip = 0;
            int header = 0;

            mid = lGetUlong(mes, MES_message_number);
            jid = lGetUlong(jid_ulng, ULNG_value);

            if (initialized) {
               if (last_mid == mid && last_jid == jid)
                  skip = 1;
               else if (last_mid != mid)
                     header = 1;
               }
               else {
                  initialized = 1;
                  header = 1;
            }

               if (strlen(text) >= MAX_LINE_LEN || ids_per_line >= MAX_IDS_PER_LINE || header) {
                  printf("%s", text);
                  text[0] = 0;
                  ids_per_line = 0;
                  first_row = 0;
               }

               if (header) {
                  if (!first_run)
                     printf("\n\n");
                  else
                     first_run = 0;
                  printf("%s\n", sge_schedd_text(mid+SCHEDD_INFO_OFFSET));
                  first_row = 1;
               }

               if (!skip) {
                  if (ids_per_line == 0)
                     if (first_row)
                        strcat(text, "\t");
                     else
                        strcat(text, ",\n\t");
                  else
                     strcat(text, ",\t");
                  snprintf(ltext, sizeof(ltext), sge_u32, jid);
                  strcat(text, ltext);
                  ids_per_line++;
               }

               last_jid = jid;
               last_mid = mid;
            }
         }
         if (text[0] != 0)
            printf("%s\n", text);
      }
   }

   lFreeList(&ilp);
   
   DRETURN(0);
}

