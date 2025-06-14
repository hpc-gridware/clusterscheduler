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
#include <cstdio>
#include <unistd.h>
#include <cstring>

#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_monitor.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "comm/commlib.h"

#include "basis_types.h"
#include "dispatcher.h"
#include "msg_execd.h"
#include "execd.h"
#include "execd_job_exec.h"
#include "execd_ticket.h"
#include "job_report_execd.h"
#include "execd_signal_queue.h"
#include "execd_kill_execd.h"
#include "execd_get_new_conf.h"
#include "execd_ck_to_do.h"
#include "load_avg.h"

#if defined(SOLARIS)
#   include "sge_smf.h"
#endif

#define SGE_EXECD_ALIVE_CHECK_MIN_INTERVAL 2*60
#define SGE_EXECD_ALIVE_CHECK_DELAY 30
#define DELAYED_FINISHED_JOB_REPORTING_INTERVAL 600

int sge_execd_process_messages() {
   DENTER(TOP_LAYER);
   monitoring_t monitor;
   bool terminate = false;
   bool do_reconnect = false;
   int ret = CL_RETVAL_UNKNOWN;
   u_long64 last_heard = 0;
   u_long64 last_alive_check = 0;
   u_long64 load_report_time = 0;
   u_long64 alive_check_interval = 0;


   sge_monitor_init(&monitor, "sge_execd_process_messages", NONE_EXT, EXECD_WARNING, EXECD_ERROR, nullptr);

   /* If we just started the execd we skip alive check */
   last_alive_check = sge_get_gmt64();
   last_heard = last_alive_check;

   /* calculate alive check interval based on load report time POS 1/2
    * If modified, please also change POS 2/2
    */
   load_report_time = sge_gmt32_to_gmt64(mconf_get_load_report_time());
   alive_check_interval    = sge_gmt32_to_gmt64(SGE_EXECD_ALIVE_CHECK_DELAY);
   if (load_report_time > sge_gmt32_to_gmt64(SGE_EXECD_ALIVE_CHECK_MIN_INTERVAL)) {
      alive_check_interval += load_report_time;
   } else {
      alive_check_interval += sge_gmt32_to_gmt64(SGE_EXECD_ALIVE_CHECK_MIN_INTERVAL);
   }

   while (!terminate) {
      u_long64 now = sge_get_gmt64();
      ocs::gdi::ClientServerBase::struct_msg_t msg;
      char* buffer     = nullptr;
      u_long32 buflen  = 0;
      sge_monitor_output(&monitor);

      memset((void*)&msg, 0, sizeof(ocs::gdi::ClientServerBase::struct_msg_t));

      ret = ocs::gdi::ClientServerBase::gdi_receive_message(msg.snd_name, &msg.snd_id, msg.snd_host,
                                                        &msg.tag, &buffer, &buflen, 0);
      if (ret == CL_RETVAL_OK) {
         int pack_ret = init_packbuffer_from_buffer(&msg.buf, buffer, buflen);
         if (pack_ret == PACK_SUCCESS) {
            bool from_qmaster = (strcmp(msg.snd_name, prognames[QMASTER]) == 0);
            bool authentication_ok = true;
            // in case of Munge authentication check and optionally re-resolve the user
            if (bootstrap_get_use_munge()) {
               if (from_qmaster) {
                  // Message from qmaster? Check if it is coming from the admin user.
                  if (!ocs::gdi::ClientServerBase::sge_gdi_reresolve_check_user(&msg.buf, true, false, false)) {
                     authentication_ok = false;
                  }
               } else {
                  // Messages from non qmaster are pe-task start orders,
                  // re-resolve and check the user
                  // no need to re-resolve the supplementary groups - they are not used in starting tasks
                  if (bootstrap_get_use_munge()) {
                     if (!ocs::gdi::ClientServerBase::sge_gdi_reresolve_check_user(&msg.buf, false, true, false)) {
                        authentication_ok = false;
                     }
                  }
               }
            }

            if (authentication_ok) {
               bool is_apb_used = false;
               sge_pack_buffer apb;
               ocs::gdi::ClientServerBase::ClientServerBaseTag atag = ocs::gdi::ClientServerBase::TAG_NONE;

               switch (msg.tag) {
                  case ocs::gdi::ClientServerBase::TAG_JOB_EXECUTION:
                     // Here we just store the job in the master job list or add a pe task to the job->ja_task->task_list.
                     // It will be executed later in do_ck_to_do() -> sge_start_jobs().
                     if (init_packbuffer(&apb, 1024) == PACK_SUCCESS) {
                        do_job_exec(&msg, &apb, from_qmaster);
                        is_apb_used = true;
                        atag = msg.tag;
                     }
                  break;
                  case ocs::gdi::ClientServerBase::TAG_SLAVE_ALLOW:
                     // Here we store the job in the master job list.
                     // This allows us to start tasks of tightly integrated jobs later (being submitted via qrsh -inherit).
                     do_job_slave(&msg);
                  break;
                  case ocs::gdi::ClientServerBase::TAG_CHANGE_TICKET:
                     do_ticket(&msg);
                  break;
                  case ocs::gdi::ClientServerBase::TAG_ACK_REQUEST:
                     do_ack(&msg);
                  break;
                  case ocs::gdi::ClientServerBase::TAG_SIGQUEUE:
                     case ocs::gdi::ClientServerBase::TAG_SIGJOB:
                     if (init_packbuffer(&apb, 1024) == PACK_SUCCESS) {
                        do_signal_queue(&msg, &apb);
                        is_apb_used = true;
                        atag = ocs::gdi::ClientServerBase::TAG_ACK_REQUEST;
                     }
                     break;
                  case ocs::gdi::ClientServerBase::TAG_KILL_EXECD:
                     do_kill_execd(&msg);
#if defined(SOLARIS)
                  if (sge_smf_used() == 1) {
                     /* We must stop, we don't care about current or next planned service state */
                     sge_smf_temporary_disable_instance();
                  }
#endif
                     break;
                  case ocs::gdi::ClientServerBase::TAG_GET_NEW_CONF:
                     do_get_new_conf(&msg);

                     /* calculate alive check interval based on load report time POS 2/2
                      * If modified, please also change POS 1/2
                      */
                     load_report_time = sge_gmt32_to_gmt64(mconf_get_load_report_time());
                     alive_check_interval    = sge_gmt32_to_gmt64(SGE_EXECD_ALIVE_CHECK_DELAY);
                     if (load_report_time > sge_gmt32_to_gmt64(SGE_EXECD_ALIVE_CHECK_MIN_INTERVAL)) {
                        alive_check_interval += load_report_time;
                     } else {
                        alive_check_interval += sge_gmt32_to_gmt64(SGE_EXECD_ALIVE_CHECK_MIN_INTERVAL);
                     }
                     break;
                  case ocs::gdi::ClientServerBase::TAG_FULL_LOAD_REPORT:
                     execd_trash_load_report();
                     sge_set_flush_lr_flag(true);
                     break;
                  default:
                     DPRINTF("***** UNKNOWN TAG TYPE %d\n", msg.tag);
               }
               last_heard = now;
               clear_packbuffer(&(msg.buf));
               if (is_apb_used) {
                  if (pb_filled(&apb)) {
                     ocs::gdi::ClientServerBase::gdi_send_message_pb(0, msg.snd_name, msg.snd_id, msg.snd_host, atag, &apb, nullptr);
                  }
                  clear_packbuffer(&apb);
               }
            }
         }
      } else {
         switch (ret) {
            case CL_RETVAL_CONNECTION_NOT_FOUND:  /* we lost connection to qmaster */
            case CL_RETVAL_CONNECT_ERROR:         /* or we can't connect */
               do_reconnect = true;
               break;
            case CL_RETVAL_NO_MESSAGE:
            case CL_RETVAL_SYNC_RECEIVE_TIMEOUT:
               break;
            default:
               do_reconnect = true;
               break;
         }
         cl_commlib_trigger(cl_com_get_handle(component_get_component_name(), 0), 1);
      }

      if (ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_WAS_COMMUNICATION_ERROR, false)) {
         do_reconnect = true;
      }

      if (do_reconnect) {
         /*
          * we are not connected, reconnect and register at qmaster ...
          */
         if (cl_com_get_handle(prognames[EXECD], 1) == nullptr) {
            terminate = true; /* if we don't have a handle, we must leave
                               * because execd_register will create a new one.
                               * This error would be realy strange, because
                               * if this happens the local socket was destroyed.
                               */
            ret = CL_RETVAL_HANDLE_NOT_FOUND;
         }

         /*
          * trigger re-read of act_qmaster_file
          */
         if (!terminate) {
            static u_long64 last_qmaster_file_read = 0;

            /* fix system clock moved back situation */
            if (last_qmaster_file_read > now) {
               last_qmaster_file_read = 0;
            }

            if (now - last_qmaster_file_read >= sge_gmt32_to_gmt64(EXECD_MAX_RECONNECT_TIMEOUT)) {
               /* re-read act qmaster file (max. every EXECD_MAX_RECONNECT_TIMEOUT seconds) */
               DPRINTF("re-read actual qmaster file\n");
               last_qmaster_file_read = now;

               /* Try to re-register at qmaster */
               if (sge_execd_register_at_qmaster(true) == 0) {
                  do_reconnect = false;    /* we are reconnected */
                  last_heard = sge_get_gmt64();
                  ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_WAS_COMMUNICATION_ERROR, true);
                  ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_ACCESS_DENIED, true);

                  // Check if we have to delay job reporting. This is the case if there are running qsub -sync jobs
                  bool delay_job_reporting = false;
                  const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
                  const lListElem *job;
                  for_each_ep(job, master_job_list) {
                     u_long32 sync_options = lGetUlong(job, JB_sync_options);
                     if (sync_options != SYNC_NO) {
                        delay_job_reporting = true;
                        break;
                     }
                  }

                  // Delay the job reporting
                  // This will be reset after the completion of the DELAYED_FINISHED_JOB_REPORTING_INTERVAL elapsed
                  if (delay_job_reporting) {
                     sge_set_qmrestart_time(now);
                     sge_set_delay_job_reports_flag(delay_job_reporting);
                     INFO(SFNMAX, MSG_EXECD_ENABLEDELEAYDJOBREPORTING);
                  }

                  /* after a reconnect, we want to send a full load report - immediately */
                  execd_trash_load_report();
                  sge_set_flush_lr_flag(true);
               } /* else we are still not connected!!! */
            }
         }
      } else {
         /*
          * we are connected - test connection
          */
         if (!terminate) {

            /* fix system clock moved back situation and do test in any case */
            if (last_alive_check > now) {
               last_alive_check = 0;
            }
            if (last_heard > now) {
               last_heard = 0;
            }

            if (sge_get_delay_job_reports_flag() &&
                now - sge_get_qmrestart_time() >= sge_gmt32_to_gmt64(DELAYED_FINISHED_JOB_REPORTING_INTERVAL)) {
                  sge_set_delay_job_reports_flag(false);
                  sge_set_qmrestart_time(0);
                  INFO(SFNMAX, MSG_EXECD_DISABLEDELEAYDJOBREPORTING);
            }

            if (now - last_alive_check >= alive_check_interval) {
               /* set new last_alive_check time */
               last_alive_check = now;

#if 0
               DPRINTF("Do we have to to alive check of qmaster?\n");


               DPRINTF("*************************************** alive check *****************************************************\n");
               DPRINTF("last_heard=" sge_u64 "\n", last_heard);
               DPRINTF("now=%ld\n", now);
               DPRINTF("now - last_heard = " sge_u64 "\n", now - last_heard);
               DPRINTF("alive_check_interval= " sge_u64 "\n", alive_check_interval);
#endif

               /*
                * last message was send before alive_check_interval seconds
                */
               if (now - last_heard > alive_check_interval) {
                  int ret_val = CL_RETVAL_OK;
                  const char* master_host = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
                  cl_com_handle_t* handle = cl_com_get_handle(prognames[EXECD],1);
                  cl_com_SIRM_t* ep_status = nullptr;

                  /*
                   * qmaster file has not changed, check the endpoint status
                   */
                  ret_val = cl_commlib_get_endpoint_status(handle,
                                                           (char*)master_host, (char*)prognames[QMASTER], 1,
                                                           &ep_status);
                  cl_com_free_sirm_message(&ep_status);
                  if (ret_val != CL_RETVAL_OK) {
                     /* There was an error, close connection and trigger reconnect */
                     cl_commlib_close_connection(handle, (char*)master_host, (char*)prognames[QMASTER], 1, false);
                     do_reconnect = true;
                  }
               }
            }
         }
      }

      if (ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_ACCESS_DENIED, false)) {
         /* we have to reconnect, when the problem is fixed */
         do_reconnect = true;
         /* we do not expect that the problem is fast to fix */
         sleep(EXECD_MAX_RECONNECT_TIMEOUT);
      } else if (ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_ENDPOINT_NOT_UNIQUE, false)) {
         terminate = true; /* leave sge_execd_process_messages */
         ret = ocs::gdi::SGE_COM_ENDPOINT_NOT_UNIQUE;
      }


      /* do cyclic stuff */
      if (!terminate) {
         int to_do_return_value = do_ck_to_do(do_reconnect);
         if (to_do_return_value == 1) {
            terminate = true;
            ret = 0;
         }
         if (to_do_return_value == 2) {
            do_reconnect = true;
         }
      }
   }
   sge_monitor_free(&monitor);

   DRETURN(ret);
}
