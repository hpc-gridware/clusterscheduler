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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <pthread.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_string.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_ShareTree.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/cull/sge_message_SME_L.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/ocs_DataStore.h"

#include "sched/debit.h"

#include "sge.h"
#include "sge_userprj_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "sge_give_jobs.h"
#include "evm/sge_event_master.h"
#include "evm/sge_queue_event_master.h"
#include "sge_persistence_qmaster.h"
#include "sge_follow.h"
#include "msg_common.h"
#include "msg_qmaster.h"

#include <ocs_gdi_ClientServerBase.h>

typedef enum {
   NOT_DEFINED = 0,
   DO_SPOOL,
   DONOT_SPOOL
} spool_type;

typedef struct {
   pthread_mutex_t last_update_mutex; /* guards the last_update access */
   u_long64 next_update;               /* used to store the last time, when the usage was stored */
   spool_type is_spooling;             /* identifies, if spooling should happen */
   u_long64 now;                       /* stores the time of the last spool computation */
   order_pos_t *cull_order_pos;        /* stores cull positions in the job, ja-task, and order structure */
} sge_follow_t;


static sge_follow_t Follow_Control = {
        PTHREAD_MUTEX_INITIALIZER,
        0,
        NOT_DEFINED,
        0,
        nullptr
};

static int ticket_orders_field[] = {OR_job_number,
                                    OR_ja_task_number,
                                    OR_ticket,
                                    NoName};

/** @brief sets the next spooling time for stree/prj/user
 *
 *  This function sets the next spooling time. It is called by the scheduler
 *  when it has finished its work and wants to spool the usage information.
 *  The function checks if the last update time is in the future and sets
 *  the next spooling time accordingly.
 */
void
set_next_stree_spooling_time() {
   DENTER(TOP_LAYER);
   sge_mutex_lock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);

   if (Follow_Control.is_spooling != NOT_DEFINED) {
      const u_long64 spool_interval = sge_gmt32_to_gmt64(mconf_get_spool_time());

      if (Follow_Control.now + spool_interval < Follow_Control.next_update) {
         Follow_Control.next_update = Follow_Control.now;
      } else if (Follow_Control.is_spooling == DO_SPOOL) {
         Follow_Control.next_update = Follow_Control.now + spool_interval;

         DSTRING_STATIC(dstr_now, 100);
         DSTRING_STATIC(dstr_next, 100);
         DPRINTF("stree/prj/user spooling now: %s (" sge_u64 ") next: %s (" sge_u64 ") interval: " sge_u64 "\n",
+                 sge_ctime64(Follow_Control.now, &dstr_now), Follow_Control.now,
+                 sge_ctime64(Follow_Control.next_update, &dstr_next), Follow_Control.next_update,
+                 spool_interval);
      }

      Follow_Control.now = 0;
      Follow_Control.is_spooling = NOT_DEFINED;
   }

   sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
   DRETURN_VOID;
}

/** @brief returns true if the scheduler should spool stree/prj/user objects
 *
 *  This function checks if the scheduler should spool.
 */
bool
do_stree_spooling() {
   DENTER(TOP_LAYER);
   bool is_spool = false;

   sge_mutex_lock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
   if (Follow_Control.is_spooling == NOT_DEFINED) {
      Follow_Control.now = sge_get_gmt64();

      DSTRING_STATIC(dstr_now, 100);
      DSTRING_STATIC(dstr_next, 100);
      DPRINTF("stree/prj/user spooling now: %s (" sge_u64 ") next: %s (" sge_u64 ")\n",
+             sge_ctime64(Follow_Control.now, &dstr_now), Follow_Control.now,
+             sge_ctime64(Follow_Control.next_update, &dstr_next), Follow_Control.next_update);

      if (Follow_Control.now >= Follow_Control.next_update) {
         DPRINTF("stree/prj/user spooling will be done.");
         Follow_Control.is_spooling = DO_SPOOL;
         is_spool = true;
      } else {
         DPRINTF("stree/prj/user spooling will not be done. Time not reached");
         Follow_Control.is_spooling = DONOT_SPOOL;
      }
   }
   sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
   DRETURN(is_spool);
}

/**********************************************************************
 Gets an order and executes it.

 Return 0 if everything is fine or

 -1 if the scheduler has sent an inconsistent order list but we think
    the next event delivery will correct this

 -2 if the scheduler has sent an inconsistent order list and we don't think
    the next event delivery will correct this

 -3 if delivery to an execd failed

lList **topp,   ticket orders ptr ptr

 **********************************************************************/
int
sge_follow_order(lListElem *ep, char *ruser, char *rhost, lList **topp, monitoring_t *monitor, u_long64 gdi_session) {
   DENTER(TOP_LAYER);

   u_long32 job_number, task_number;
   const char *or_pe, *q_name = nullptr;
   u_long32 or_type;
   lListElem *jep, *qep, *hep, *jatp = nullptr;
   const lListElem *oep;
   u_long32 state;
   u_long32 pe_slots = 0, q_slots = 0, q_version;
   lListElem *pe = nullptr;
   bool is_spool = do_stree_spooling();
   u_long64 now = sge_get_gmt64();

   or_type = lGetUlong(ep, OR_type);
   or_pe = lGetString(ep, OR_pe);

   DPRINTF("-----------------------------------------------------------------------\n");

   switch (or_type) {

      /* -----------------------------------------------------------------------
       * START JOB
       * ----------------------------------------------------------------------- */
      case ORT_start_job: {
         lList *gdil = nullptr;
         lListElem *master_qep = nullptr;
         lListElem *master_host = nullptr;
         const lList *exec_host_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
         const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
         const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
         const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
         const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
         const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
         const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);

         DPRINTF("ORDER ORT_start_job\n");

         job_number = lGetUlong(ep, OR_job_number);
         if (job_number == 0) {
            ERROR(SFNMAX, MSG_JOB_NOJOBID);
            DRETURN(-2);
         }

         task_number = lGetUlong(ep, OR_ja_task_number);
         if (task_number == 0) {
            ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_start_job");
            DRETURN(-2);
         }

         jep = lGetElemUlongRW(master_job_list, JB_job_number, job_number);
         if (jep == nullptr) {
            WARNING(MSG_JOB_FINDJOB_U, job_number);
            DRETURN(-1);
         }

         DPRINTF("ORDER to start Job %ld Task %ld\n", (long) job_number, (long) task_number);

         /* if job got modified in the meantime (qalter), do not start it */
         if (lGetUlong(jep, JB_version) != lGetUlong(ep, OR_job_version)) {
            WARNING(MSG_ORD_OLDVERSION_UUU, job_number, task_number, lGetUlong(ep, OR_job_version));
            DRETURN(-1);
         }

         /* search and enroll task */
         bool enrolled_task = false;
         jatp = job_search_task(jep, nullptr, task_number);
         if (jatp == nullptr) {
            if (range_list_is_id_within(lGetList(jep, JB_ja_n_h_ids), task_number)) {
               jatp = job_create_task(jep, nullptr, task_number);
               if (jatp == nullptr) {
                  WARNING(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
                  DRETURN(-1);
               }
               enrolled_task = true;
            } else {
               INFO(MSG_JOB_IGNORE_DELETED_TASK_UU, job_number, task_number);
               DRETURN(0);
            }
         }

         /* job is not pending - we got the order twice? */
         if (lGetUlong(jatp, JAT_status) != JIDLE) {
            if (enrolled_task) {
               job_unenroll(jep, nullptr, &jatp);
            }
            ERROR(MSG_ORD_TWICE_UU, job_number, task_number);
            DRETURN(-1);
         }

         /* pe job - find the pe object */
         if (or_pe) {
            pe = pe_list_locate(master_pe_list, or_pe);
            if (pe == nullptr) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               ERROR(MSG_OBJ_UNABLE2FINDPE_S, or_pe);
               DRETURN(-2);
            }
            lSetString(jatp, JAT_granted_pe, or_pe);  /* free me on error! */
         }

         /* job is running in an advance reservation - find it */
         u_long32 ar_id = lGetUlong(jep, JB_ar);
         if (ar_id != 0) {
            lListElem *ar = ar_list_locate(master_ar_list, lGetUlong(jep, JB_ar));
            if (ar == nullptr) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, lGetUlong(jep, JB_ar), lGetUlong(jep, JB_job_number));
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-2);
            }
            lSetUlong64(jatp, JAT_wallclock_limit,
                      lGetUlong64(ar, AR_end_time) - now - sge_gmt32_to_gmt64(sconf_get_duration_offset()));
         }

         /* fill number of tickets into job */
         lSetDouble(jatp, JAT_tix, lGetDouble(ep, OR_ticket));
         lSetDouble(jatp, JAT_ntix, lGetDouble(ep, OR_ntix));
         lSetDouble(jatp, JAT_prio, lGetDouble(ep, OR_prio));

         /**
          * Move the order granted resource list into a JAT granted
          * resource list, which is sent to the execution daemon. This is
          * done for the RSMAP complex type and hard and soft requests
          * which are passed to the execution daemon.
          **/
         lSwapList(jatp, JAT_granted_resources_list, ep, OR_granted_resources_list);

         if ((oep = lFirst(lGetList(ep, OR_queuelist)))) {
            lSetDouble(jatp, JAT_oticket, lGetDouble(oep, OQ_oticket));
            lSetDouble(jatp, JAT_fticket, lGetDouble(oep, OQ_fticket));
            lSetDouble(jatp, JAT_sticket, lGetDouble(oep, OQ_sticket));
         }

         for_each_ep(oep, lGetList(ep, OR_queuelist)) {
            lListElem *gdil_ep;

            q_name = lGetString(oep, OQ_dest_queue);
            q_version = lGetUlong(oep, OQ_dest_version);
            q_slots = lGetUlong(oep, OQ_slots);

            /* ----------------------
             *  find and check queue
             */
            if (!q_name) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               ERROR(SFNMAX, MSG_OBJ_NOQNAME);
               DRETURN(-2);
            }

            DPRINTF("ORDER: start %d slots of job \"%d\" on"
                    " queue \"%s\" v" sge_u32 " with " sge_u32 " initial tickets\n",
                    q_slots, job_number, q_name, q_version, static_cast<u_long32>(lGetDouble(ep, OR_ticket)));

            qep = cqueue_list_locate_qinstance(master_cqueue_list, q_name);
            if (qep == nullptr) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, q_name, job_number);
               DRETURN(-2);
            }

            /* check queue version */
            if (q_version != lGetUlong(qep, QU_version)) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               /* try to repair schedd data */
               qinstance_add_event(qep, sgeE_QINSTANCE_MOD, gdi_session);
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               WARNING(MSG_ORD_QVERSION_SUU, q_name, q_version,  lGetUlong(qep, QU_version));
               DRETURN(-1);
            }

            /* the first queue is the master queue */
            if (master_qep == nullptr) {
               master_qep = qep;
            }

            DPRINTF("Queue version: %d\n", q_version);

            /* ensure that the jobs owner has access to this queue */
            if (!sge_has_access_(lGetString(jep, JB_owner), lGetString(jep, JB_group), lGetList(jep, JB_grp_list),
                                 lGetList(qep, QU_acl), lGetList(qep, QU_xacl),
                                 master_userset_list)) {
               ERROR(MSG_JOB_JOBACCESSQ_US, job_number, q_name);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-1);
            }

            /* ensure that this queue has enough free slots */
            if (lGetUlong(qep, QU_job_slots) - qinstance_slots_used(qep) < q_slots) {
               ERROR(MSG_JOB_FREESLOTS_USUU, q_slots, q_name, job_number, task_number);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-1);
            }

            /* check queue states - queue may not be
             *   - in error state
             *   - calendar suspended
             *   - calendar disabled
             */
            if (qinstance_state_is_error(qep)) {
               WARNING(MSG_JOB_QMARKEDERROR_S, q_name);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-1);
            }
            if (qinstance_state_is_cal_suspended(qep)) {
               WARNING(MSG_JOB_QSUSPCAL_S, q_name);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-1);
            }
            if (qinstance_state_is_cal_disabled(qep)) {
               WARNING(MSG_JOB_QDISABLECAL_S, q_name);
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-1);
            }

            /* ----------------------
             *  find and check host
             */
            hep = host_list_locate(exec_host_list, lGetHost(qep, QU_qhostname));
            if (hep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDHOST_S, lGetHost(qep, QU_qhostname));
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(-2);
            } else {
               const lListElem *ruep;

               /* host not yet clean after reschedule unknown? */
               for_each_ep(ruep, lGetList(hep, EH_reschedule_unknown_list)) {
                  if (job_number == lGetUlong(ruep, RU_job_number)
                      && task_number == lGetUlong(ruep, RU_task_number)) {
                     ERROR(MSG_JOB_UNABLE2STARTJOB_US, lGetUlong(ruep, RU_job_number), lGetHost(qep, QU_qhostname));
                     if (enrolled_task) {
                        job_unenroll(jep, nullptr, &jatp);
                     }
                     lFreeList(&gdil);
                     lSetString(jatp, JAT_granted_pe, nullptr);
                     DRETURN(-1);
                  }
               }
            }

            /* ------------------------------------------------
             *  build up granted_destin_identifier_list (gdil)
             */
            gdil_ep = lAddElemStr(&gdil, JG_qname, q_name, JG_Type); /* free me on error! */
            lSetHost(gdil_ep, JG_qhostname, lGetHost(qep, QU_qhostname));
            lSetUlong(gdil_ep, JG_slots, q_slots);

            /* ------------------------------------------------
             *  tag each gdil entry of slave exec host
             *  in case of sge controlled slaves
             *  this triggers our retry for delivery of slave jobs
             *  and gets untagged when ack has arrived
             */
            if (pe && lGetBool(pe, PE_control_slaves)) {
               lSetDouble(gdil_ep, JG_ticket, lGetDouble(oep, OQ_ticket));
               lSetDouble(gdil_ep, JG_oticket, lGetDouble(oep, OQ_oticket));
               lSetDouble(gdil_ep, JG_fticket, lGetDouble(oep, OQ_fticket));
               lSetDouble(gdil_ep, JG_sticket, lGetDouble(oep, OQ_sticket));

               /* the master host is the host of the master queue */
               if (master_host != nullptr && master_host != hep) {
                  lListElem *first_at_host;

                  /* ensure each host gets tagged only one time
                   * we tag the first entry for a host in the existing gdil
                   */
                  first_at_host = lGetElemHostRW(gdil, JG_qhostname, lGetHost(hep, EH_name));
                  if (!first_at_host) {
                     ERROR(MSG_JOB_HOSTNAMERESOLVE_US, lGetUlong(jep, JB_job_number), lGetHost( hep, EH_name));
                  } else {
                     lSetUlong(first_at_host, JG_tag_slave_job, 1);
                  }
               } else {
                  master_host = hep;
                  DPRINTF("master host %s\n", lGetHost(master_host, EH_name));
               }
            } else {
               /* a sequential job only has one host */
               master_host = hep;
            }

            /* in case of a pe job update free_slots on the pe */
            if (pe) {
               pe_slots += q_slots;
            }
         }

         /*
          * check if consumables are still sufficient on the host and on global host
          * - for each individual host with the number of slots on that host
          *   - the first host is master host
          * - for the global host with the total number of slots
          *   - do the check as master host, to make sure global per job consumables are checked
          */
         {
            bool is_master = true;
            bool do_per_host_booking = true;
            bool consumables_ok = true;
            int host_slots = 0;
            int total_slots = 0;
            const char *host_name = nullptr;
            lListElem *gdil_ep;
            lListElem *next_gdil_ep = lFirstRW(gdil);

            /* loop over gdil */
            host_name = lGetHost(next_gdil_ep, JG_qhostname);
            while ((gdil_ep = next_gdil_ep) != nullptr) {
               u_long32 slots = lGetUlong(gdil_ep, JG_slots);

               // check if booking would work on the queue
               // this should actually not be necessary, as any change on the queue definition (consumables)
               // would have increased the queue version which we check above
               // it would reject jobs which got falsely scheduled, though
#if 0
               const char *queue_name = lGetString(gdil_ep, JG_qname);
               lListElem *queue = nullptr;
               queue = cqueue_list_locate_qinstance(master_cqueue_list, queue_name);
               if (queue) {
                  qinstance_debit_consumable(queue, jep, pe, master_centry_list, slots, is_master, false, &consumables_ok);
                  if (!consumables_ok) {
                     break;
                  }
               }
#endif

               /* sum up slots */
               host_slots += slots;
               total_slots += slots;

               /* gdil end or switch to next host: check booking on the current host */
               next_gdil_ep = lNextRW(gdil_ep);
               if (next_gdil_ep == nullptr || strcmp(host_name, lGetHost(next_gdil_ep, JG_qhostname)) != 0) {
                  hep = host_list_locate(exec_host_list, host_name);
                  debit_host_consumable(jep, jatp, pe, hep, master_centry_list, host_slots, is_master,
                                        do_per_host_booking,
                                        &consumables_ok);
                  if (!consumables_ok) {
                     break;
                  }

                  /* if there is a next host
                   *    - it is a slave host
                   *    - we have already booked the per host consumables
                   */
                  is_master = false;
                  do_per_host_booking = false;

                  /* there is a next host: get hostname and reset host slot counter */
                  if (next_gdil_ep != nullptr) {
                     host_name = lGetHost(next_gdil_ep, JG_qhostname);
                     host_slots = 0;
                  }
               }
            }

            /* Per host checks were OK? Then check global host. */
            if (consumables_ok) {
               lListElem *global_hep = host_list_locate(exec_host_list, SGE_GLOBAL_NAME);
               debit_host_consumable(jep, jatp, pe, global_hep, master_centry_list, total_slots, true, true,
                                     &consumables_ok);
            }

            /* Consumable check failed - we cannot start this job! */
            if (!consumables_ok) {
               ERROR(MSG_JOB_RESOURCESNOLONGERAVAILABLE_UU, job_number, task_number);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               lSetString(jatp, JAT_granted_pe, nullptr);
               DRETURN(0);
            }
         }

         /* fill in master_queue */
         lSetString(jatp, JAT_master_queue, lGetString(master_qep, QU_full_name));
         lSetList(jatp, JAT_granted_destin_identifier_list, gdil);
         // store the pe - we need it for unbooking when the job terminates
         if (pe != nullptr) {
            lSetObject(jatp, JAT_pe_object, lCopyElem(pe));
         }

         // @todo: can this be summaized with the mod event that will set the job in t-state?
         sge_add_event(now, sgeE_JATASK_ADD, job_number, task_number,
                      nullptr, nullptr, lGetString(jep, JB_session), jatp, gdi_session);

         if (sge_give_job(jep, jatp, master_qep, master_host, monitor, gdi_session)) {
            /* setting of queues in state unheard is done by sge_give_job() */
            sge_commit_job(jep, jatp, nullptr, COMMIT_ST_DELIVERY_FAILED, COMMIT_DEFAULT, monitor, gdi_session);
            /* This was sge_commit_job(jep, COMMIT_ST_RESCHEDULED). It raised problems if a job
               could not be delivered. The jobslotsfree had been increased even if
               they were not decreased before. */

            ERROR(MSG_JOB_JOBDELIVER_UU, lGetUlong(jep, JB_job_number), lGetUlong(jatp, JAT_task_number));
            DRETURN(-3);
         }

         /* job is now sent and goes into transfering state */
         /* mode == COMMIT_ST_SENT -> really accept when execd acks */
         sge_commit_job(jep, jatp, nullptr, COMMIT_ST_SENT, COMMIT_DEFAULT, monitor, gdi_session);

         /* now send events and spool the job */
         {
            lList *answer_list = nullptr;
            const char *session = lGetString(jep, JB_session);

            /* spool job and ja_task in one transaction, send job mod event */
            sge_event_spool(&answer_list, 0, sgeE_JOB_MOD,
                            job_number, task_number, nullptr, nullptr, session,
                            jep, jatp, nullptr, true, true, gdi_session);
            answer_list_output(&answer_list);
         }

         /* set timeout for job resend */
         if (mconf_get_simulate_execds()) {
            trigger_job_resend(now, master_host, job_number, task_number, 1);
         } else {
            trigger_job_resend(now, master_host, job_number, task_number, 5);
         }

         if (pe != nullptr) {
            pe_debit_slots(pe, pe_slots, job_number);
            /* this info is not spooled */
            sge_add_event(now, sgeE_PE_MOD, 0, 0, lGetString(jatp, JAT_granted_pe), nullptr, nullptr, pe, gdi_session);
         }

         DPRINTF("successfully handed off job \"" sge_u32 "\" to queue \"%s\"\n",
                 lGetUlong(jep, JB_job_number), lGetString(jatp, JAT_master_queue));

         /* now after successfully (we hope) sent the job to execd
          * suspend all subordinated queues that need suspension
          */
         cqueue_list_x_on_subordinate_gdil(master_cqueue_list, true, gdil, monitor, gdi_session);
      }
         break;

         /* -----------------------------------------------------------------------
            * SET PRIORITY VALUES TO nullptr
            *
            * modifications performed on the job are not spooled
            * ----------------------------------------------------------------------- */
      case ORT_clear_pri_info:

      DPRINTF("ORDER ORT_ptickets\n");
         {
            ja_task_pos_t *ja_pos = nullptr;
            job_pos_t *job_pos = nullptr;
            lListElem *next_ja_task = nullptr;
            const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

            job_number = lGetUlong(ep, OR_job_number);
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            task_number = lGetUlong(ep, OR_ja_task_number);

            jep = lGetElemUlongRW(master_job_list, JB_job_number, job_number);
            if (jep == nullptr) {
               WARNING(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }

            next_ja_task = lFirstRW(lGetList(jep, JB_ja_tasks));

            /* we have to iterate over the ja-tasks and the template */
            jatp = job_get_ja_task_template_pending(jep, 0);
            if (jatp == nullptr) {
               ERROR(MSG_JOB_FINDJOBTASK_UU, static_cast<u_long32>(0), job_number);
               DRETURN(-2);
            }

            sge_mutex_lock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);

            if (Follow_Control.cull_order_pos != nullptr) { /* do we have the positions cached? */
               ja_pos = &(Follow_Control.cull_order_pos->ja_task);
               job_pos = &(Follow_Control.cull_order_pos->job);

               while (jatp != nullptr) {
                  lSetPosDouble(jatp, ja_pos->JAT_tix_pos, 0);
                  lSetPosDouble(jatp, ja_pos->JAT_oticket_pos, 0);
                  lSetPosDouble(jatp, ja_pos->JAT_fticket_pos, 0);
                  lSetPosDouble(jatp, ja_pos->JAT_sticket_pos, 0);
                  lSetPosDouble(jatp, ja_pos->JAT_share_pos, 0);
                  lSetPosDouble(jatp, ja_pos->JAT_prio_pos, 0);
                  lSetPosDouble(jatp, ja_pos->JAT_ntix_pos, 0);
                  if (task_number != 0) { /* if task_number == 0, we only change the */
                     sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
                     jatp = next_ja_task; /* pending tickets, otherwise all */
                     next_ja_task = lNextRW(next_ja_task);
                  } else {
                     jatp = nullptr;
                  }
               }

               lSetPosDouble(jep, job_pos->JB_nurg_pos, 0);
               lSetPosDouble(jep, job_pos->JB_urg_pos, 0);
               lSetPosDouble(jep, job_pos->JB_rrcontr_pos, 0);
               lSetPosDouble(jep, job_pos->JB_dlcontr_pos, 0);
               lSetPosDouble(jep, job_pos->JB_wtcontr_pos, 0);
            } else {   /* we do not have the positions cached.... */
               while (jatp != nullptr) {
                  lSetDouble(jatp, JAT_tix, 0);
                  lSetDouble(jatp, JAT_oticket, 0);
                  lSetDouble(jatp, JAT_fticket, 0);
                  lSetDouble(jatp, JAT_sticket, 0);
                  lSetDouble(jatp, JAT_share, 0);
                  lSetDouble(jatp, JAT_prio, 0);
                  lSetDouble(jatp, JAT_ntix, 0);
                  if (task_number != 0) {   /* if task_number == 0, we only change the */
                     sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
                     jatp = next_ja_task;   /* pending tickets, otherwise all */
                     next_ja_task = lNextRW(next_ja_task);
                  } else {
                     jatp = nullptr;
                  }
               }

               lSetDouble(jep, JB_nurg, 0);
               lSetDouble(jep, JB_urg, 0);
               lSetDouble(jep, JB_rrcontr, 0);
               lSetDouble(jep, JB_dlcontr, 0);
               lSetDouble(jep, JB_wtcontr, 0);
            }

            sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
            // @todo CS-913 we should have the tickets in sub-objects and have a ticket event having only the sub-object as data
            sge_add_event(now, sgeE_JOB_MOD, job_number, 0, nullptr, nullptr, nullptr, jep, gdi_session);
         }
         break;

         /* -----------------------------------------------------------------------
          * CHANGE TICKETS OF PENDING JOBS
          *
          * Modify the tickets of pending jobs for the sole purpose of being
          * able to display and sort the pending jobs list based on the
          * expected execution order.
          *
          * modifications performed on the job are not spooled
          * ----------------------------------------------------------------------- */
      case ORT_ptickets:
      DPRINTF("ORDER ORT_ptickets\n");
         {
            ja_task_pos_t *ja_pos;
            ja_task_pos_t *order_ja_pos;
            job_pos_t *job_pos;
            job_pos_t *order_job_pos;
            const lListElem *joker;
            const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

            job_number = lGetUlong(ep, OR_job_number);
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            DPRINTF("ORDER : job(" sge_u32 ")->ticket = %f\n", job_number, lGetDouble(ep, OR_ticket));

            jep = lGetElemUlongRW(master_job_list, JB_job_number, job_number);
            if (jep == nullptr) {
               WARNING(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }

            task_number = lGetUlong(ep, OR_ja_task_number);
            if (task_number == 0) {
               ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_ptickets");
               DRETURN(-2);
            }

            bool send_task_event = true;
            jatp = job_search_task(jep, nullptr, task_number);
            if (jatp == nullptr) {
               send_task_event = false;
               jatp = job_get_ja_task_template_pending(jep, task_number);

               if (jatp == nullptr) {
                  ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
                  sge_add_event(now, sgeE_JATASK_DEL, job_number, task_number,
                                nullptr, nullptr, lGetString(jep, JB_session), nullptr, gdi_session);
                  DRETURN(-2);
               }
            }

            sge_mutex_lock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);

            if (Follow_Control.cull_order_pos == nullptr) {
               const lListElem *joker_task;

               joker = lFirst(lGetList(ep, OR_joker));
               joker_task = lFirst(lGetList(joker, JB_ja_tasks));

               sge_create_cull_order_pos(&(Follow_Control.cull_order_pos), jep, jatp, joker, joker_task);
            }

            ja_pos = &(Follow_Control.cull_order_pos->ja_task);
            order_ja_pos = &(Follow_Control.cull_order_pos->order_ja_task);
            job_pos = &(Follow_Control.cull_order_pos->job);
            order_job_pos = &(Follow_Control.cull_order_pos->order_job);

            if (lGetPosUlong(jatp, ja_pos->JAT_status_pos) == JFINISHED) {
               WARNING(MSG_JOB_CHANGEPTICKETS_UU, lGetUlong(jep, JB_job_number), lGetUlong(jatp, JAT_task_number));
               sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
               DRETURN(0);
            }

            /* modify jobs ticket amount */
            lSetPosDouble(jatp, ja_pos->JAT_tix_pos, lGetDouble(ep, OR_ticket));

            /* check several fields to be updated */
            if ((joker = lFirst(lGetList(ep, OR_joker)))) {
               const lListElem *joker_task;

               joker_task = lFirst(lGetList(joker, JB_ja_tasks));

               lSetPosDouble(jatp, ja_pos->JAT_oticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_oticket_pos));
               lSetPosDouble(jatp, ja_pos->JAT_fticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_fticket_pos));
               lSetPosDouble(jatp, ja_pos->JAT_sticket_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_sticket_pos));
               lSetPosDouble(jatp, ja_pos->JAT_share_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_share_pos));
               lSetPosDouble(jatp, ja_pos->JAT_prio_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_prio_pos));
               lSetPosDouble(jatp, ja_pos->JAT_ntix_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_ntix_pos));

               // @todo do we really update the *job* values with every ja_task update?
               lSetPosDouble(jep, job_pos->JB_nurg_pos, lGetPosDouble(joker, order_job_pos->JB_nurg_pos));
               lSetPosDouble(jep, job_pos->JB_urg_pos, lGetPosDouble(joker, order_job_pos->JB_urg_pos));
               lSetPosDouble(jep, job_pos->JB_rrcontr_pos, lGetPosDouble(joker, order_job_pos->JB_rrcontr_pos));
               lSetPosDouble(jep, job_pos->JB_dlcontr_pos, lGetPosDouble(joker, order_job_pos->JB_dlcontr_pos));
               lSetPosDouble(jep, job_pos->JB_wtcontr_pos, lGetPosDouble(joker, order_job_pos->JB_wtcontr_pos));
            }

            sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
            if (send_task_event) {
               sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
            }
            sge_add_event(now, sgeE_JOB_MOD, job_number, 0, nullptr, nullptr, nullptr, jep, gdi_session);

#if 0
            DPRINTF(("PRIORITY: " sge_u32 "." sge_u32" %f/%f tix/ntix %f npri %f/%f urg/nurg %f prio\n",
               lGetUlong(jep, JB_job_number),
               lGetUlong(jatp, JAT_task_number),
               lGetDouble(jatp, JAT_tix),
               lGetDouble(jatp, JAT_ntix),
               lGetDouble(jep, JB_urg),
               lGetDouble(jep, JB_nurg),
               lGetDouble(jatp, JAT_prio)));
#endif

         }
         break;


         /* -----------------------------------------------------------------------
          * CHANGE TICKETS OF RUNNING/TRANSFERRING JOBS
          *
          * Our aim is to collect all ticket orders of a gdi request
          * and to send ONE packet to each exec host. Here we just
          * check the orders for consistency. They get forwarded to
          * execd having checked all orders.
          *
          * modifications performed on the job are not spooled
          * ----------------------------------------------------------------------- */
      case ORT_tickets:
      DPRINTF("ORDER ORT_tickets\n");
         {
            const lListElem *joker;
            const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

            job_number = lGetUlong(ep, OR_job_number);
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            DPRINTF("ORDER: job(" sge_u32 ")->ticket = %f\n",
                    job_number, lGetDouble(ep, OR_ticket));

            jep = lGetElemUlongRW(master_job_list, JB_job_number, job_number);
            if (jep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }
            task_number = lGetUlong(ep, OR_ja_task_number);
            if (task_number == 0) {
               ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_tickets");
               DRETURN(-2);
            }
            jatp = job_search_task(jep, nullptr, task_number);
            if (jatp == nullptr) {
               ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
               sge_add_event(now, sgeE_JATASK_DEL, job_number, task_number, nullptr, nullptr, lGetString(jep, JB_session), nullptr, gdi_session);
               DRETURN(-2);
            }

            /* job must be running */
            if (lGetUlong(jatp, JAT_status) != JRUNNING &&
                lGetUlong(jatp, JAT_status) != JTRANSFERING) {

               /* if the job just finished, ignore the order */
               if (lGetUlong(jatp, JAT_status) != JFINISHED) {
                  WARNING(MSG_JOB_CHANGETICKETS_UUU, lGetUlong(jep, JB_job_number),  lGetUlong(jatp, JAT_task_number), lGetUlong(jatp, JAT_status));
                  DRETURN(0);
               }
            } else {
               bool distribute_tickets = false;
               /* modify jobs ticket amount and spool job */
               lSetDouble(jatp, JAT_tix, lGetDouble(ep, OR_ticket));
               DPRINTF("TICKETS: " sge_u32 "." sge_u32 " %f tickets\n",
                       lGetUlong(jep, JB_job_number), lGetUlong(jatp, JAT_task_number), lGetDouble(jatp, JAT_tix));

               /* check several fields to be updated */
               if ((joker = lFirst(lGetList(ep, OR_joker)))) {
                  const lListElem *joker_task;
                  ja_task_pos_t *ja_pos;
                  ja_task_pos_t *order_ja_pos;
                  job_pos_t *job_pos;
                  job_pos_t *order_job_pos;

                  joker_task = lFirst(lGetList(joker, JB_ja_tasks));
                  distribute_tickets = (lGetPosViaElem(joker_task, JAT_granted_destin_identifier_list, SGE_NO_ABORT) >
                                        -1) ? true : false;

                  sge_mutex_lock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);

                  if (Follow_Control.cull_order_pos == nullptr) {
                     const lListElem *joker_task;

                     joker = lFirst(lGetList(ep, OR_joker));
                     joker_task = lFirst(lGetList(joker, JB_ja_tasks));

                     sge_create_cull_order_pos(&(Follow_Control.cull_order_pos), jep, jatp, joker, joker_task);
                  }

                  ja_pos = &(Follow_Control.cull_order_pos->ja_task);
                  order_ja_pos = &(Follow_Control.cull_order_pos->order_ja_task);
                  job_pos = &(Follow_Control.cull_order_pos->job);
                  order_job_pos = &(Follow_Control.cull_order_pos->order_job);

                  lSetPosDouble(jatp, ja_pos->JAT_oticket_pos,
                                lGetPosDouble(joker_task, order_ja_pos->JAT_oticket_pos));
                  lSetPosDouble(jatp, ja_pos->JAT_fticket_pos,
                                lGetPosDouble(joker_task, order_ja_pos->JAT_fticket_pos));
                  lSetPosDouble(jatp, ja_pos->JAT_sticket_pos,
                                lGetPosDouble(joker_task, order_ja_pos->JAT_sticket_pos));
                  lSetPosDouble(jatp, ja_pos->JAT_share_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_share_pos));
                  lSetPosDouble(jatp, ja_pos->JAT_prio_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_prio_pos));
                  lSetPosDouble(jatp, ja_pos->JAT_ntix_pos, lGetPosDouble(joker_task, order_ja_pos->JAT_ntix_pos));

                  lSetPosDouble(jep, job_pos->JB_nurg_pos, lGetPosDouble(joker, order_job_pos->JB_nurg_pos));
                  lSetPosDouble(jep, job_pos->JB_urg_pos, lGetPosDouble(joker, order_job_pos->JB_urg_pos));
                  lSetPosDouble(jep, job_pos->JB_rrcontr_pos, lGetPosDouble(joker, order_job_pos->JB_rrcontr_pos));
                  lSetPosDouble(jep, job_pos->JB_dlcontr_pos, lGetPosDouble(joker, order_job_pos->JB_dlcontr_pos));
                  lSetPosDouble(jep, job_pos->JB_wtcontr_pos, lGetPosDouble(joker, order_job_pos->JB_wtcontr_pos));
                  sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
               }

               /* tickets should only be further distributed in the scheduler reprioritize_interval. Only in
                  those intervales does the ticket order structure contain a JAT_granted_destin_identifier_list.
                  We use that as an identifier to go on, or not. */
               if (distribute_tickets && topp != nullptr) {
                  lDescr *rdp = nullptr;

                  lEnumeration *what = lIntVector2What(OR_Type, ticket_orders_field);
                  lReduceDescr(&rdp, OR_Type, what);

                  /* If a ticket order has a queuelist, then this is a parallel job
                     with controlled sub-tasks. We generate a ticket order for
                     each host in the queuelist containing the total tickets for
                     all job slots being used on the host */
                  if (lGetList(ep, OR_queuelist)) {
                     lList *host_tickets_cache = lCreateList("", UA_Type); /* cashed temporary hash list */
                     /* set granted slot tickets */
                     for_each_ep(oep, lGetList(ep, OR_queuelist)) {
                        lListElem *chost_ep;
                        lListElem *gdil_ep = lGetSubStrRW(jatp, JG_qname, lGetString(oep, OQ_dest_queue),
                                                          JAT_granted_destin_identifier_list);
                        if (gdil_ep != nullptr) {
                           double tickets = lGetDouble(oep, OQ_ticket);
                           const char *hostname = lGetHost(gdil_ep, JG_qhostname);

                           lSetDouble(gdil_ep, JG_ticket, tickets);
                           lSetDouble(gdil_ep, JG_oticket, lGetDouble(oep, OQ_oticket));
                           lSetDouble(gdil_ep, JG_fticket, lGetDouble(oep, OQ_fticket));
                           lSetDouble(gdil_ep, JG_sticket, lGetDouble(oep, OQ_sticket));

                           chost_ep = lGetElemStrRW(host_tickets_cache, UA_name, hostname);
                           if (chost_ep == nullptr) {
                              chost_ep = lAddElemStr(&host_tickets_cache, UA_name, hostname, UA_Type);
                           }
                           lAddDouble(chost_ep, UA_value, tickets);
                        }
                     }

                     /* map cached tickets back to RTIC_Type list */
                     for_each_ep(oep, host_tickets_cache) {
                        const char *hostname = lGetString(oep, UA_name);
                        lListElem *rtic_ep;
                        lList *host_tickets;

                        lListElem *newep = lSelectElemDPack(ep, nullptr, rdp, what, false, nullptr);
                        lSetDouble(newep, OR_ticket, lGetDouble(oep, UA_value));

                        rtic_ep = lGetElemHostRW(*topp, RTIC_host, hostname);
                        if (rtic_ep == nullptr) {
                           rtic_ep = lAddElemHost(topp, RTIC_host, hostname, RTIC_Type);
                        }
                        host_tickets = lGetListRW(rtic_ep, RTIC_tickets);
                        if (host_tickets == nullptr) {
                           host_tickets = lCreateList("ticket orders", rdp);
                           lSetList(rtic_ep, RTIC_tickets, host_tickets);
                        }
                        lAppendElem(host_tickets, newep);
                     }
                     lFreeList(&host_tickets_cache);
                  } else {
                     const lList *gdil = lGetList(jatp, JAT_granted_destin_identifier_list);
                     if (gdil != nullptr) {
                        lListElem *newep = lSelectElemDPack(ep, nullptr, rdp, what, false, nullptr);
                        lList *host_tickets;
                        const char *hostname = lGetHost(lFirst(gdil), JG_qhostname);
                        lListElem *rtic_ep = lGetElemHostRW(*topp, RTIC_host, hostname);
                        if (rtic_ep == nullptr) {
                           rtic_ep = lAddElemHost(topp, RTIC_host, hostname, RTIC_Type);
                        }
                        host_tickets = lGetListRW(rtic_ep, RTIC_tickets);
                        if (host_tickets == nullptr) {
                           host_tickets = lCreateList("ticket orders", rdp);
                           lSetList(rtic_ep, RTIC_tickets, host_tickets);
                        }
                        lAppendElem(host_tickets, newep);
                     }

                  }
                  lFreeWhat(&what);
                  sge_free(&rdp);
               }
            }
            sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
            sge_add_event(now, sgeE_JOB_MOD, job_number, 0, nullptr, nullptr, nullptr, jep, gdi_session);
         }
         break;

         /* -----------------------------------------------------------------------
          * REMOVE JOBS THAT ARE WAITING FOR SCHEDD'S PERMISSION TO GET DELETED
          *
          * Using this order schedd can only remove jobs in the
          * "dead but not buried" state
          *
          * ----------------------------------------------------------------------- */
      case ORT_remove_job:
         /* -----------------------------------------------------------------------
          * REMOVE IMMEDIATE JOBS THAT COULD NOT GET SCHEDULED IN THIS PASS
          *
          * Using this order schedd can only remove idle immediate jobs
          * (former ORT_remove_interactive_job)
          * ----------------------------------------------------------------------- */
      case ORT_remove_immediate_job: {
         lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
         DPRINTF("ORDER: ORT_remove_immediate_job or ORT_remove_job\n");

         job_number = lGetUlong(ep, OR_job_number);
         if (job_number == 0) {
            ERROR(SFNMAX, MSG_JOB_NOJOBID);
            DRETURN(-2);
         }
         task_number = lGetUlong(ep, OR_ja_task_number);
         if (task_number == 0) {
            ERROR(MSG_JOB_NOORDERTASK_US, job_number, (or_type == ORT_remove_immediate_job) ? "ORT_remove_immediate_job" : "ORT_remove_job");
            DRETURN(-2);
         }
         DPRINTF("ORDER: remove %sjob " sge_u32 "." sge_u32 "\n", or_type == ORT_remove_immediate_job ? "immediate " : "", job_number, task_number);
         jep = lGetElemUlongRW(master_job_list, JB_job_number, job_number);
         if (jep == nullptr) {
            if (or_type == ORT_remove_job) {
               ERROR(MSG_JOB_FINDJOB_U, job_number);
               /* try to repair schedd data - session is unknown here */
               sge_add_event(now, sgeE_JOB_DEL, job_number, task_number, nullptr, nullptr, nullptr, nullptr, gdi_session);
               DRETURN(-1);
            } else {
               /* in case of an immediate parallel job the job could be missing */
               INFO(MSG_JOB_FINDJOB_U, job_number);
               DRETURN(0);
            }
         }
         jatp = job_search_task(jep, nullptr, task_number);

         /* if ja task doesn't exist yet, create it */
         if (jatp == nullptr) {
            lList *answer_list = nullptr;

            jatp = job_create_task(jep, nullptr, task_number);

            /* new jatask has to be spooled and event sent */
            if (jatp == nullptr) {
               ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
               DRETURN(-1);
            }

            if (or_type == ORT_remove_job) {
               ERROR(MSG_JOB_ORDERDELINCOMPLETEJOB_UU, job_number, task_number);
               lSetUlong(jatp, JAT_status, JFINISHED);
            }
            sge_event_spool(&answer_list, 0, sgeE_JATASK_ADD,
                            job_number, task_number, nullptr, nullptr,
                            lGetString(jep, JB_session),
                            jep, jatp, nullptr, true, true, gdi_session);
            answer_list_output(&answer_list);
         }

         if (or_type == ORT_remove_job) {
            if (lGetUlong(jatp, JAT_status) != JFINISHED) {
               ERROR(MSG_JOB_REMOVENOTFINISHED_U, lGetUlong(jep, JB_job_number));
               DRETURN(-1);
            }

            /* remove it */
            sge_commit_job(jep, jatp, nullptr, COMMIT_ST_DEBITED_EE, COMMIT_DEFAULT, monitor, gdi_session);
         } else {
            if (!JOB_TYPE_IS_IMMEDIATE(lGetUlong(jep, JB_type))) {
               if (lGetString(jep, JB_script_file)) {
                  ERROR(MSG_JOB_REMOVENONINTERACT_U, lGetUlong(jep, JB_job_number));
               } else {
                  ERROR(MSG_JOB_REMOVENONIMMEDIATE_U, lGetUlong(jep, JB_job_number));
               }
               DRETURN(-1);
            }
            if (lGetUlong(jatp, JAT_status) != JIDLE) {
               ERROR(MSG_JOB_REMOVENOTIDLEIA_U, lGetUlong(jep, JB_job_number));
               DRETURN(-1);
            }
            INFO(MSG_JOB_NOFREERESOURCEIA_UU, lGetUlong(jep, JB_job_number), lGetUlong(jatp, JAT_task_number), lGetString(jep, JB_owner));

            /* remove it */
            sge_commit_job(jep, jatp, nullptr, COMMIT_ST_NO_RESOURCES, COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
         }
         break;
      }

// @todo CS-272: should not be required as soon as deletion of job specific
#if 1
         /* -----------------------------------------------------------------------
          * REPLACE A PROJECT'S
          *
          * - PR_usage
          * - PR_usage_time_stamp
          * - PR_long_term_usage
          * - PR_debited_job_usage
          *
          * Using this order schedd can debit usage on users/projects
          * both orders are handled identically except target list
          * ----------------------------------------------------------------------- */
      case ORT_update_project_usage:
      DPRINTF("ORDER: ORT_update_project_usage\n");
         {
            const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);

            DPRINTF("ORDER: update %d projects\n", lGetNumberOfElem(lGetList(ep, OR_joker)));

            lListElem *up_order;
            for_each_rw (up_order, lGetList(ep, OR_joker)) {
               int pos = lGetPosViaElem(up_order, PR_name, SGE_NO_ABORT);
               if (pos < 0) {
                  continue;
               }
               const char *up_name = lGetString(up_order, PR_name);
               if (up_name == nullptr) {
                  continue;
               }
               lListElem *up = prj_list_locate(master_project_list, up_name);
               if (up == nullptr) {
                  /* order contains reference to unknown user/prj object */
                  continue;
               }

               DPRINTF("%s %s usage updating with %d jobs\n", MSG_OBJ_PRJ,
                       up_name, lGetNumberOfElem(lGetList(up_order, PR_debited_job_usage)));

               if ((pos = lGetPosViaElem(up_order, PR_version, SGE_NO_ABORT)) >= 0 &&
                   (lGetPosUlong(up_order, pos) != lGetUlong(up, PR_version))) {
                  /* order contains update for outdated user/project usage */
                  WARNING(MSG_ORD_USRPRJVERSION_SUU, up_name, lGetPosUlong(up_order, pos), lGetUlong(up, PR_version));
                  /* Note: Should we apply the debited job usage in this case? */
                  continue;
               }

               lAddUlong(up, PR_version, 1);

               if ((pos = lGetPosViaElem(up_order, PR_project, SGE_NO_ABORT)) >= 0) {
                  lSwapList(up_order, PR_project, up, PR_project);
               }

               if ((pos = lGetPosViaElem(up_order, PR_usage_time_stamp, SGE_NO_ABORT)) >= 0)
                  lSetUlong64(up, PR_usage_time_stamp, lGetPosUlong64(up_order, pos));

               if ((pos = lGetPosViaElem(up_order, PR_usage, SGE_NO_ABORT)) >= 0) {
                  lSwapList(up_order, PR_usage, up, PR_usage);
               }

               if ((pos = lGetPosViaElem(up_order, PR_long_term_usage, SGE_NO_ABORT)) >= 0) {
                  lSwapList(up_order, PR_long_term_usage, up, PR_long_term_usage);
               }

               /* update old usage in up for each job appearing in
                  PR_debited_job_usage of 'up_order' */
               lListElem *ju;
               lListElem *next = lFirstRW(lGetList(up_order, PR_debited_job_usage));
               while ((ju = next)) {
                  next = lNextRW(ju);

                  job_number = lGetUlong(ju, UPU_job_number);

                  /* seek for existing debited usage of this job */
                  lListElem *up_ju;
                  if ((up_ju = lGetSubUlongRW(up, UPU_job_number, job_number, PR_debited_job_usage))) {

                     /* if passed old usage list is nullptr, delete existing usage */
                     if (lGetList(ju, UPU_old_usage_list) == nullptr) {
                        lRemoveElem(lGetListRW(up_order, PR_debited_job_usage), &ju);
                        lRemoveElem(lGetListRW(up, PR_debited_job_usage), &up_ju);
                     } else {
                        /* still exists - replace old usage with new one */
                        DPRINTF("updating debited usage for job " sge_u32 "\n", job_number);
                        lSwapList(ju, UPU_old_usage_list, up_ju, UPU_old_usage_list);
                     }

                  } else {
                     /* unchain ju element and chain it into our user/prj object */
                     DPRINTF("adding debited usage for job " sge_u32 "\n", job_number);
                     lDechainElem(lGetListRW(up_order, PR_debited_job_usage), ju);

                     if (lGetList(ju, UPU_old_usage_list) != nullptr) {
                        /* unchain ju element and chain it into our user/prj object */
                        lList *tlp;
                        if (!(tlp = lGetListRW(up, PR_debited_job_usage))) {
                           tlp = lCreateList(up_name, UPU_Type);
                           lSetList(up, PR_debited_job_usage, tlp);
                        }
                        lInsertElem(tlp, nullptr, ju);
                     } else {
                        /* do not chain in empty empty usage records */
                        lFreeElem(&ju);
                     }
                  }
               }

               /* spool and send event */
               lList *answer_list = nullptr;
               sge_event_spool(&answer_list, 0, sgeE_PROJECT_MOD, 0, 0, up_name,
                               nullptr, nullptr, up, nullptr, nullptr,
                               true, is_spool, gdi_session);
               answer_list_output(&answer_list);
            }
         }
         break;

         /* -----------------------------------------------------------------------
          * REPLACE A USER
          *
          * - UU_usage
          * - UU_usage_time_stamp
          * - UU_long_term_usage
          * - UU_debited_job_usage
          *
          * Using this order schedd can debit usage on users/projects
          * both orders are handled identically except target list
          * ----------------------------------------------------------------------- */
      case ORT_update_user_usage:
      DPRINTF("ORDER: ORT_update_user_usage\n");
         {
            lListElem *up_order, *up, *ju, *up_ju, *next;
            int pos;
            const char *up_name;
            lList *tlp;
            const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);

            DPRINTF("ORDER: update %d users\n", lGetNumberOfElem(lGetList(ep, OR_joker)));

            for_each_rw (up_order, lGetList(ep, OR_joker)) {
               if ((pos = lGetPosViaElem(up_order, UU_name, SGE_NO_ABORT)) < 0 ||
                   !(up_name = lGetString(up_order, UU_name))) {
                  continue;
               }

               DPRINTF("%s %s usage updating with %d jobs\n", MSG_OBJ_USER,
                       up_name, lGetNumberOfElem(lGetList(up_order, UU_debited_job_usage)));

               if (!(up = user_list_locate(master_user_list, up_name))) {
                  /* order contains reference to unknown user/prj object */
                  continue;
               }

               if ((pos = lGetPosViaElem(up_order, UU_version, SGE_NO_ABORT)) >= 0 &&
                   (lGetPosUlong(up_order, pos) != lGetUlong(up, UU_version))) {
                  /* order contains update for outdated user/project usage */
                  WARNING(MSG_ORD_USRPRJVERSION_SUU, up_name, lGetPosUlong(up_order, pos), lGetUlong(up, UU_version));
                  /* Note: Should we apply the debited job usage in this case? */
                  continue;
               }

               lAddUlong(up, UU_version, 1);

               if ((pos = lGetPosViaElem(up_order, UU_project, SGE_NO_ABORT)) >= 0) {
                  lSwapList(up_order, UU_project, up, UU_project);
               }

               if ((pos = lGetPosViaElem(up_order, UU_usage_time_stamp, SGE_NO_ABORT)) >= 0)
                  lSetUlong64(up, UU_usage_time_stamp, lGetPosUlong64(up_order, pos));

               if ((pos = lGetPosViaElem(up_order, UU_usage, SGE_NO_ABORT)) >= 0) {
                  lSwapList(up_order, UU_usage, up, UU_usage);
               }

               if ((pos = lGetPosViaElem(up_order, UU_long_term_usage, SGE_NO_ABORT)) >= 0) {
                  lSwapList(up_order, UU_long_term_usage, up, UU_long_term_usage);
               }

               /* update old usage in up for each job appearing in
                  UU_debited_job_usage of 'up_order' */
               next = lFirstRW(lGetList(up_order, UU_debited_job_usage));
               while ((ju = next)) {
                  next = lNextRW(ju);

                  job_number = lGetUlong(ju, UPU_job_number);

                  /* seek for existing debited usage of this job */
                  if ((up_ju = lGetSubUlongRW(up, UPU_job_number, job_number, UU_debited_job_usage))) {

                     /* if passed old usage list is nullptr, delete existing usage */
                     if (lGetList(ju, UPU_old_usage_list) == nullptr) {

                        lRemoveElem(lGetListRW(up_order, UU_debited_job_usage), &ju);
                        lRemoveElem(lGetListRW(up, UU_debited_job_usage), &up_ju);

                     } else {

                        /* still exists - replace old usage with new one */
                        DPRINTF("updating debited usage for job " sge_u32 "\n", job_number);
                        lSwapList(ju, UPU_old_usage_list, up_ju, UPU_old_usage_list);
                     }

                  } else {
                     /* unchain ju element and chain it into our user/prj object */
                     DPRINTF("adding debited usage for job " sge_u32 "\n", job_number);
                     lDechainElem(lGetListRW(up_order, UU_debited_job_usage), ju);

                     if (lGetList(ju, UPU_old_usage_list) != nullptr) {
                        /* unchain ju element and chain it into our user/prj object */
                        if (!(tlp = lGetListRW(up, UU_debited_job_usage))) {
                           tlp = lCreateList(up_name, UPU_Type);
                           lSetList(up, UU_debited_job_usage, tlp);
                        }
                        lInsertElem(tlp, nullptr, ju);
                     } else {
                        /* do not chain in empty empty usage records */
                        lFreeElem(&ju);
                     }
                  }
               }

               /* spool and send event */

               {
                  lList *answer_list = nullptr;
                  sge_event_spool(&answer_list, 0, sgeE_USER_MOD, 0, 0, up_name,
                                  nullptr, nullptr, up, nullptr, nullptr,
                                  true, is_spool, gdi_session);
                  answer_list_output(&answer_list);
               }
            }
         }
         break;
#endif

         /* -----------------------------------------------------------------------
          * FILL IN SEVERAL SCHEDULING VALUES INTO QMASTER'S SHARE TREE
          * TO BE DISPLAYED BY QMON AND OTHER CLIENTS
          * ----------------------------------------------------------------------- */
      case ORT_share_tree: {
         lList *master_stree_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE);

         DPRINTF("ORDER: ORT_share_tree\n");
         ocs::ShareTree::zero_fields(lFirstRW(master_stree_list));
         if (update_sharetree(master_stree_list, lGetListRW(ep, OR_joker)) != 0) {
            DPRINTF("ORDER: ORT_share_tree failed\n");
            DRETURN(-1);
         }

         // no need to spool but other data stores need to be updated
         sge_add_event(now, sgeE_NEW_SHARETREE, 0, 0, nullptr, nullptr,
                       nullptr, lFirstRW(master_stree_list), gdi_session);
      }
         break;

         /* -----------------------------------------------------------------------
          * UPDATE FIELDS IN SCHEDULING CONFIGURATION
          * ----------------------------------------------------------------------- */
      case ORT_sched_conf: {
         DPRINTF("ORDER: ORT_sched_conf\n");

         if (sconf_is()) {
            if (const lListElem *joker = lFirst(lGetList(ep, OR_joker)); joker != nullptr) {
               if (int pos = lGetPosViaElem(joker, SC_weight_tickets_override, SGE_NO_ABORT); pos > -1) {
                  u_long32 old_wto = sconf_get_weight_tickets_override();
                  u_long32 new_wto = lGetPosUlong(joker, pos);

                  if (old_wto != new_wto) {
                     sconf_set_weight_tickets_override(new_wto);

                     // no need to spool but other data stores need to be updated
                     lListElem *sconfig = lFirstRW(*ocs::DataStore::get_master_list(SGE_TYPE_SCHEDD_CONF));
                     sge_add_event(now, sgeE_SCHED_CONF, 0, 0,
                                   "schedd_conf", nullptr, nullptr, sconfig, gdi_session);
                  }
               }
            }
         }
         break;
      }
      case ORT_suspend_on_threshold: {
         lListElem *queueep;
         u_long32 jobid;
         lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
         lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

         DPRINTF("ORDER: ORT_suspend_on_threshold\n");

         jobid = lGetUlong(ep, OR_job_number);
         task_number = lGetUlong(ep, OR_ja_task_number);

         if (!(jep = lGetElemUlongRW(master_job_list, JB_job_number, jobid))
             || !(jatp = job_search_task(jep, nullptr, task_number))
             || !lGetList(jatp, JAT_granted_destin_identifier_list)) {
            /* don't panic - it is probably an exiting job */
            WARNING(MSG_JOB_SUSPOTNOTRUN_UU, jobid, task_number);
         } else {
            const char *qnm = lGetString(lFirst(lGetList(jatp, JAT_granted_destin_identifier_list)), JG_qname);
            queueep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);
            if (queueep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, qnm, jobid);
               DRETURN(-1);
            }

            INFO(MSG_JOB_SUSPTQ_UUS, jobid, task_number, qnm);

            if (!ISSET(lGetUlong(jatp, JAT_state), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatp, monitor);
               state = lGetUlong(jatp, JAT_state);
               CLEARBIT(JRUNNING, state);
               lSetUlong(jatp, JAT_state, state);
            }
            state = lGetUlong(jatp, JAT_state);
            SETBIT(JSUSPENDED_ON_THRESHOLD, state);
            lSetUlong(jatp, JAT_state, state);

            {
               lList *answer_list = nullptr;
               const char *session = lGetString(jep, JB_session);
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, jobid, task_number, nullptr, nullptr, session,
                               jep, jatp, nullptr, true, true, gdi_session);
               answer_list_output(&answer_list);
            }

            /* update queues time stamp in schedd */
            lSetUlong64(queueep, QU_last_suspend_threshold_ckeck, now);
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD, gdi_session);
         }
      }
         break;

      case ORT_unsuspend_on_threshold: {
         lListElem *queueep;
         u_long32 jobid;
         lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
         lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

         DPRINTF("ORDER: ORT_unsuspend_on_threshold\n");

         jobid = lGetUlong(ep, OR_job_number);
         task_number = lGetUlong(ep, OR_ja_task_number);

         if (!(jep = lGetElemUlongRW(master_job_list, JB_job_number, jobid))
             || !(jatp = job_search_task(jep, nullptr, task_number))
             || !lGetList(jatp, JAT_granted_destin_identifier_list)) {
            /* don't panic - it is probably an exiting job */
            WARNING(MSG_JOB_UNSUSPOTNOTRUN_UU, jobid, task_number);
         } else {
            const char *qnm = lGetString(lFirst(lGetList(jatp, JAT_granted_destin_identifier_list)), JG_qname);
            queueep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);
            if (queueep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, qnm, jobid);
               DRETURN(-1);
            }

            INFO(MSG_JOB_UNSUSPOT_UUS, jobid, task_number, qnm);

            if (!ISSET(lGetUlong(jatp, JAT_state), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGCONT, queueep, jep, jatp, monitor);
               state = lGetUlong(jatp, JAT_state);
               SETBIT(JRUNNING, state);
               lSetUlong(jatp, JAT_state, state);
            }
            state = lGetUlong(jatp, JAT_state);
            CLEARBIT(JSUSPENDED_ON_THRESHOLD, state);
            lSetUlong(jatp, JAT_state, state);
            {
               lList *answer_list = nullptr;
               const char *session = lGetString(jep, JB_session);
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, jobid, task_number, nullptr, nullptr, session,
                               jep, jatp, nullptr, true, true, gdi_session);
               answer_list_output(&answer_list);
            }
            /* update queues time stamp in schedd */
            lSetUlong64(queueep, QU_last_suspend_threshold_ckeck, now);
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD, gdi_session);
         }
      }
         break;

      case ORT_job_schedd_info: {
         lList *sub_order_list = lGetListRW(ep, OR_joker);
         lList **master_job_schedd_info_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB_SCHEDD_INFO);

         DPRINTF("ORDER: ORT_job_schedd_info\n");

         if (sub_order_list != nullptr) {
            lListElem *sme = lFirstRW(sub_order_list);

            if (sme != nullptr) {
               lListElem *first;

               DPRINTF("ORDER: got %d schedd infos\n", lGetNumberOfElem(lGetList(sme, SME_message_list)));

               while ((first = lFirstRW(*master_job_schedd_info_list))) {
                  lRemoveElem(*master_job_schedd_info_list, &first);
               }
               if (*master_job_schedd_info_list == nullptr) {
                  *master_job_schedd_info_list = lCreateList("schedd info", SME_Type);
               }
               lDechainElem(sub_order_list, sme);
               lAppendElem(*master_job_schedd_info_list, sme);

               /* this information is not spooled (but might be usefull in a db) */
               sge_add_event(now, sgeE_JOB_SCHEDD_INFO_MOD, 0, 0, nullptr, nullptr, nullptr, sme, gdi_session);
            }
         }
      }
         break;

      default:
         break;
   }

   DRETURN(STATUS_OK);
}

/*
 * MT-NOTE: distribute_ticket_orders() is NOT MT safe
 */
int distribute_ticket_orders(lList *ticket_orders, monitoring_t *monitor) {
   u_long64 now = sge_get_gmt64();
   unsigned long last_heard_from = 0;
   int cl_err = CL_RETVAL_OK;
   const lListElem *ep;
   lList *master_ehost_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);

   DENTER(TOP_LAYER);

   for_each_ep(ep, ticket_orders) {
      const lList *to_send = lGetList(ep, RTIC_tickets);
      const char *host_name = lGetHost(ep, RTIC_host);
      const lListElem *hep = host_list_locate(master_ehost_list, host_name);

      int n = lGetNumberOfElem(to_send);

      if (hep) {
         cl_commlib_get_last_message_time((cl_com_get_handle(prognames[QMASTER], 0)),
                                          (char *) host_name, (char *) prognames[EXECD], 1, &last_heard_from);
      }
      if (hep &&sge_gmt32_to_gmt64(last_heard_from + 10 * mconf_get_load_report_time()) > now) {
         sge_pack_buffer pb;

         if (init_packbuffer(&pb, sizeof(u_long32) * 3 * n) == PACK_SUCCESS) {
            u_long32 dummyid = 0;
            const lListElem *ep2;
            for_each_ep(ep2, to_send) {
               packint(&pb, lGetUlong(ep2, OR_job_number));
               packint(&pb, lGetUlong(ep2, OR_ja_task_number));
               packdouble(&pb, lGetDouble(ep2, OR_ticket));
            }
            cl_err = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, prognames[EXECD], 1, host_name,
                                                                 ocs::gdi::ClientServerBase::TAG_CHANGE_TICKET, &pb, &dummyid);
            MONITOR_MESSAGES_OUT(monitor);
            clear_packbuffer(&pb);
            DPRINTF("%s %d ticket changings to execd@%s\n",
                    (cl_err == CL_RETVAL_OK) ? "failed sending" : "sent", n, host_name);
         }
      } else {
         DPRINTF("     skipped sending of %d ticket changings to execd@%s because %s\n", n, host_name,
                 !hep ? "no such host registered" : "suppose host is down");
      }
   }

   DRETURN(cl_err);
}
