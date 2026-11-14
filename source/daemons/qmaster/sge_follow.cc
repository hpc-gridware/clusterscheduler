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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Following the scheduler's orders and booking the usage they imply
 */
#include <cstring>
#include <pthread.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"
#include "cull/ocs_cull_descr.h"

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
#include "sgeobj/sge_userset.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_OQ.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_RU.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_RTIC.h"
#include "sgeobj/tom/ocs_tom_UA.h"

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

/** @brief Whether the share tree should be spooled on this pass
 *
 * Decided once per interval and then reused, so that a burst of orders in the
 * same interval does not each trigger a spool write.
 */
typedef enum {
   NOT_DEFINED = 0,   ///< Not yet decided for the current interval
   DO_SPOOL,          ///< Spool the share tree on this pass
   DONOT_SPOOL        ///< Leave it; the interval has not elapsed
} spool_type;

/** @brief What the order-following code remembers between orders
 *
 * The scheduler sends its decisions as a list of orders, and following them
 * updates the share tree usage. Spooling that on every order would be far too
 * expensive, so the decision is cached here for the length of an interval.
 */
typedef struct {
   pthread_mutex_t last_update_mutex; ///< guards the last_update access
   uint64_t next_update;               ///< used to store the last time, when the usage was stored
   spool_type is_spooling;             ///< identifies, if spooling should happen
   uint64_t now;                       ///< stores the time of the last spool computation
} sge_follow_t;


static sge_follow_t Follow_Control = {
        PTHREAD_MUTEX_INITIALIZER,
        0,
        NOT_DEFINED,
        0
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
      const uint64_t spool_interval = sge_gmt32_to_gmt64(mconf_get_spool_time());

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
 *
 * @return true if this call is the one that has to spool; false if the
 *         spool interval has not elapsed, or another thread already took
 *         the decision for this round
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
              sge_ctime64(Follow_Control.now, &dstr_now), Follow_Control.now,
              sge_ctime64(Follow_Control.next_update, &dstr_next), Follow_Control.next_update);

      if (Follow_Control.now >= Follow_Control.next_update) {
         DPRINTF("stree/prj/user spooling will be done.");
         Follow_Control.is_spooling = DO_SPOOL;
         is_spool = true;
      } else {
         DPRINTF("stree/prj/user spooling will not be done. Time not reached.\n");
         Follow_Control.is_spooling = DONOT_SPOOL;
      }
   }
   sge_mutex_unlock("follow_last_update_mutex", __func__, __LINE__, &Follow_Control.last_update_mutex);
   DRETURN(is_spool);
}

/**
 * @brief Execute one order of the scheduler.
 *
 * @param ep          the order (OR_Type)
 * @param ruser       user who sent the order list
 * @param rhost       host the order list came from
 * @param topp        ticket orders that are collected here and sent to the
 *                    execds afterwards
 * @param monitor     monitoring of the calling thread
 * @param gdi_session GDI session the events of this order belong to
 * @return 0 if the order was executed or could be ignored,
 *         -1 if the scheduler sent an inconsistent order list that the next
 *         event delivery will probably correct,
 *         -2 if the scheduler sent an inconsistent order list that the next
 *         event delivery will not correct (the remaining orders are skipped
 *         and the scheduler is reinitialized),
 *         -3 if the delivery to an execd failed
 */
int
sge_follow_order(lListElem *ep, char *ruser, char *rhost, lList **topp, monitoring_t *monitor, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   uint32_t job_number, task_number;
   const char *or_pe, *q_name = nullptr;
   uint32_t or_type;
   lListElem *jep, *qep, *hep, *jatp = nullptr;
   const lListElem *oep;
   uint32_t state;
   uint32_t pe_slots = 0, q_slots = 0, q_version;
   lListElem *pe = nullptr;
   uint64_t now = sge_get_gmt64();

   or_type = ocs::tom::get<OR_type>(ep);
   or_pe = ocs::tom::get<OR_pe>(ep);

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

         job_number = ocs::tom::get<OR_job_number>(ep);
         if (job_number == 0) {
            ERROR(SFNMAX, MSG_JOB_NOJOBID);
            DRETURN(-2);
         }

         task_number = ocs::tom::get<OR_ja_task_number>(ep);
         if (task_number == 0) {
            ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_start_job");
            DRETURN(-2);
         }

         ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_number);
         if (jep == nullptr) {
            WARNING(MSG_JOB_FINDJOB_U, job_number);
            DRETURN(-1);
         }

         DPRINTF("ORDER to start Job %ld Task %ld\n", (long) job_number, (long) task_number);

         /* if job got modified in the meantime (qalter), do not start it */
         if (ocs::tom::get<JB_version>(jep) != ocs::tom::get<OR_job_version>(ep)) {
            WARNING(MSG_ORD_OLDVERSION_UUU, job_number, task_number, ocs::tom::get<OR_job_version>(ep));
            DRETURN(-1);
         }

         /* search and enroll task */
         bool enrolled_task = false;
         jatp = job_search_task(jep, nullptr, task_number);
         if (jatp == nullptr) {
            if (range_list_is_id_within(ocs::tom::get<JB_ja_n_h_ids>(jep), task_number)) {
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
         if (ocs::tom::get<JAT_status>(jatp) != JIDLE) {
            if (enrolled_task) {
               job_unenroll(jep, nullptr, &jatp);
            }
            // @todo add more info, e.g. in which state / status the job is
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
            ocs::tom::set<JAT_granted_pe>(jatp, or_pe);  /* free me on error! */
         }

         /* job is running in an advance reservation - find it */
         uint32_t ar_id = ocs::tom::get<JB_ar>(jep);
         if (ar_id != 0) {
            lListElem *ar = ar_list_locate(master_ar_list, ocs::tom::get<JB_ar>(jep));
            if (ar == nullptr) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, ocs::tom::get<JB_ar>(jep), ocs::tom::get<JB_job_number>(jep));
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               DRETURN(-2);
            }
            ocs::tom::set<JAT_wallclock_limit>(jatp,
                      ocs::tom::get<AR_end_time>(ar) - now - sge_gmt32_to_gmt64(sconf_get_duration_offset()));
         }

         /* fill number of tickets into job */
         ocs::tom::set<JAT_tix>(jatp, ocs::tom::get<OR_ticket>(ep));
         ocs::tom::set<JAT_ntix>(jatp, ocs::tom::get<OR_ntix>(ep));
         ocs::tom::set<JAT_prio>(jatp, ocs::tom::get<OR_prio>(ep));

         /**
          * Move the order granted resource list into a JAT granted
          * resource list, which is sent to the execution daemon. This is
          * done for the RSMAP complex type and hard and soft requests
          * which are passed to the execution daemon.
          **/
         ocs::tom::swap<JAT_granted_resources_list, OR_granted_resources_list>(jatp, ep);

         if ((oep = lFirst(static_cast<const lList *>(ocs::tom::get<OR_queuelist>(ep))))) {
            ocs::tom::set<JAT_oticket>(jatp, ocs::tom::get<OQ_oticket>(oep));
            ocs::tom::set<JAT_fticket>(jatp, ocs::tom::get<OQ_fticket>(oep));
            ocs::tom::set<JAT_sticket>(jatp, ocs::tom::get<OQ_sticket>(oep));
         }

         for_each_ep(oep, static_cast<const lList *>(ocs::tom::get<OR_queuelist>(ep))) {

            q_name = ocs::tom::get<OQ_dest_queue>(oep);
            q_version = ocs::tom::get<OQ_dest_version>(oep);
            q_slots = ocs::tom::get<OQ_slots>(oep);

            /* ----------------------
             *  find and check queue
             */
            if (!q_name) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               ERROR(SFNMAX, MSG_OBJ_NOQNAME);
               DRETURN(-2);
            }

            DPRINTF("ORDER: start %d slots of job \"%d\" on"
                    " queue \"%s\" v" sge_u32 " with " sge_u32 " initial tickets\n",
                    q_slots, job_number, q_name, q_version, static_cast<uint32_t>(ocs::tom::get<OR_ticket>(ep)));

            qep = cqueue_list_locate_qinstance(master_cqueue_list, q_name);
            if (qep == nullptr) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, q_name, job_number);
               DRETURN(-2);
            }

            /* check queue version */
            if (q_version != ocs::tom::get<QU_version>(qep)) {
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               /* try to repair schedd data */
               qinstance_add_event(qep, sgeE_QINSTANCE_MOD, gdi_session);
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               WARNING(MSG_ORD_QVERSION_SUU, q_name, q_version,  ocs::tom::get<QU_version>(qep));
               DRETURN(-1);
            }

            /* the first queue is the master queue */
            if (master_qep == nullptr) {
               master_qep = qep;
            }

            DPRINTF("Queue version: %d\n", q_version);

            /* ensure that the jobs owner has access to this queue */
            if (!sge_has_access_(ocs::tom::get<JB_owner>(jep), ocs::tom::get<JB_group>(jep), ocs::tom::get<JB_grp_list>(jep),
                                 ocs::tom::get<QU_acl>(qep), ocs::tom::get<QU_xacl>(qep),
                                 master_userset_list)) {
               ERROR(MSG_JOB_JOBACCESSQ_US, job_number, q_name);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               DRETURN(-1);
            }

            /* ensure that this queue has enough free slots */
            if (ocs::tom::get<QU_job_slots>(qep) - qinstance_slots_used(qep) < q_slots) {
               ERROR(MSG_JOB_FREESLOTS_USUU, q_slots, q_name, job_number, task_number);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
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
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               DRETURN(-1);
            }
            if (qinstance_state_is_cal_suspended(qep)) {
               WARNING(MSG_JOB_QSUSPCAL_S, q_name);
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               DRETURN(-1);
            }
            if (qinstance_state_is_cal_disabled(qep)) {
               WARNING(MSG_JOB_QDISABLECAL_S, q_name);
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               DRETURN(-1);
            }

            /* ----------------------
             *  find and check host
             */
            hep = host_list_locate(exec_host_list, ocs::tom::get<QU_qhostname>(qep));
            if (hep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDHOST_S, static_cast<const char *>(ocs::tom::get<QU_qhostname>(qep)));
               if (enrolled_task) {
                  job_unenroll(jep, nullptr, &jatp);
               }
               lFreeList(&gdil);
               ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               DRETURN(-2);
            } else {
               ocs::tom::ConstElement<ocs::tom::RU> ruep;

               /* host not yet clean after reschedule unknown? */
               for_each_ep(ruep, ocs::tom::get<EH_reschedule_unknown_list>(hep)) {
                  if (job_number == ocs::tom::get<RU_job_number>(ruep)
                      && task_number == ocs::tom::get<RU_task_number>(ruep)) {
                     ERROR(MSG_JOB_UNABLE2STARTJOB_US, ocs::tom::get<RU_job_number>(ruep), static_cast<const char *>(ocs::tom::get<QU_qhostname>(qep)));
                     if (enrolled_task) {
                        job_unenroll(jep, nullptr, &jatp);
                     }
                     lFreeList(&gdil);
                     ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
                     DRETURN(-1);
                  }
               }
            }

            /* ------------------------------------------------
             *  build up granted_destin_identifier_list (gdil)
             */
            ocs::tom::Element<ocs::tom::JG> gdil_ep = ocs::tom::emplace_back<JG_qname>(&gdil, q_name); /* free me on error! */
            ocs::tom::set<JG_qhostname>(gdil_ep, ocs::tom::get<QU_qhostname>(qep));
            ocs::tom::set<JG_slots>(gdil_ep, q_slots);
            // the execd uses the version to tell a queue instance change from an unchanged one
            ocs::tom::set<JG_qversion>(gdil_ep, ocs::tom::get<QU_version>(qep));
            ocs::tom::set<JG_binding_to_use>(gdil_ep, lCopyList("binding_to_use",ocs::tom::get<OQ_binding_to_use>(oep)));

            /* ------------------------------------------------
             *  tag each gdil entry of slave exec host
             *  in case of sge controlled slaves
             *  this triggers our retry for delivery of slave jobs
             *  and gets untagged when ack has arrived
             */
            if (pe && ocs::tom::get<PE_control_slaves>(pe)) {
               ocs::tom::set<JG_ticket>(gdil_ep, ocs::tom::get<OQ_ticket>(oep));
               ocs::tom::set<JG_oticket>(gdil_ep, ocs::tom::get<OQ_oticket>(oep));
               ocs::tom::set<JG_fticket>(gdil_ep, ocs::tom::get<OQ_fticket>(oep));
               ocs::tom::set<JG_sticket>(gdil_ep, ocs::tom::get<OQ_sticket>(oep));

               /* the master host is the host of the master queue */
               if (master_host != nullptr && master_host != hep) {

                  /* ensure each host gets tagged only one time
                   * we tag the first entry for a host in the existing gdil
                   */
                  ocs::tom::Element<ocs::tom::JG> first_at_host = ocs::tom::find_first_rw<JG_qhostname>(gdil, ocs::tom::get<EH_name>(hep));
                  if (!first_at_host) {
                     ERROR(MSG_JOB_HOSTNAMERESOLVE_US, ocs::tom::get<JB_job_number>(jep), static_cast<const char *>(ocs::tom::get<EH_name>( hep)));
                  } else {
                     ocs::tom::set<JG_tag_slave_job>(first_at_host, 1);
                  }
               } else {
                  master_host = hep;
                  DPRINTF("master host %s\n", static_cast<const char *>(ocs::tom::get<EH_name>(master_host)));
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
            ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list = ocs::tom::get<JAT_granted_resources_list>(jatp);

            /* loop over gdil */
            host_name = ocs::tom::get<JG_qhostname>(next_gdil_ep);
            while ((gdil_ep = next_gdil_ep) != nullptr) {
               uint32_t slots = ocs::tom::get<JG_slots>(gdil_ep);

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
               if (next_gdil_ep == nullptr || strcmp(host_name, ocs::tom::get<JG_qhostname>(next_gdil_ep)) != 0) {
                  hep = host_list_locate(exec_host_list, host_name);
                  debit_host_consumable(jep, jatp, granted_resources_list, pe, hep, master_centry_list, host_slots, is_master,
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
                     host_name = ocs::tom::get<JG_qhostname>(next_gdil_ep);
                     host_slots = 0;
                  }
               }
            }

            /* Per host checks were OK? Then check global host. */
            if (consumables_ok) {
               lListElem *global_hep = host_list_locate(exec_host_list, SGE_GLOBAL_NAME);
               debit_host_consumable(jep, jatp, granted_resources_list, pe, global_hep, master_centry_list, total_slots, true, true,
                                     &consumables_ok);
            }

            /* Consumable check failed - we cannot start this job! */
            if (!consumables_ok) {
               ERROR(MSG_JOB_RESOURCESNOLONGERAVAILABLE_UU, job_number, task_number);
               if (enrolled_task) {
                  // frees the task and sets jatp to nullptr
                  job_unenroll(jep, nullptr, &jatp);
               } else {
                  ocs::tom::set<JAT_granted_pe>(jatp, nullptr);
               }
               lFreeList(&gdil);
               DRETURN(0);
            }
         }

         /* fill in master_queue */
         ocs::tom::set<JAT_master_queue>(jatp, ocs::tom::get<QU_full_name>(master_qep));
         ocs::tom::set<JAT_granted_destin_identifier_list>(jatp, gdil);
         // store the pe - we need it for unbooking when the job terminates
         if (pe != nullptr) {
            ocs::tom::set<JAT_pe_object>(jatp, lCopyElem(pe));
         }

         // @todo: can this be summarized with the mod event that will set the job in t-state?
         sge_add_event(now, enrolled_task ? sgeE_JATASK_ADD : sgeE_JATASK_MOD, job_number, task_number,
                       nullptr, nullptr, ocs::tom::get<JB_session>(jep), jatp, gdi_session);

         if (sge_give_job(jep, jatp, master_qep, master_host, monitor, gdi_session)) {
            /* setting of queues in state unheard is done by sge_give_job() */
            sge_commit_job(jep, jatp, nullptr, COMMIT_ST_DELIVERY_FAILED, COMMIT_DEFAULT, monitor, gdi_session);
            /* This was sge_commit_job(jep, COMMIT_ST_RESCHEDULED). It raised problems if a job
               could not be delivered. The jobslotsfree had been increased even if
               they were not decreased before. */

            ERROR(MSG_JOB_JOBDELIVER_UU, ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatp));
            DRETURN(-3);
         }

         /* job is now sent and goes into transferring state */
         /* mode == COMMIT_ST_SENT -> really accept when execd acks */
         sge_commit_job(jep, jatp, nullptr, COMMIT_ST_SENT, COMMIT_DEFAULT, monitor, gdi_session);

         /* now send events and spool the job */
         {
            lList *answer_list = nullptr;
            const char *session = ocs::tom::get<JB_session>(jep);

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
            sge_add_event(now, sgeE_PE_MOD, 0, 0, ocs::tom::get<JAT_granted_pe>(jatp), nullptr, nullptr, pe, gdi_session);
         }

         DPRINTF("successfully handed off job \"" sge_u32 "\" to queue \"%s\"\n",
                 ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_master_queue>(jatp));

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

      DPRINTF("ORDER ORT_clear_pri_info\n");
         {
            lListElem *next_ja_task = nullptr;
            const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

            job_number = ocs::tom::get<OR_job_number>(ep);
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            task_number = ocs::tom::get<OR_ja_task_number>(ep);

            ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_number);
            if (jep == nullptr) {
               // CS-2765: INFO, not a warning and certainly not an error. The
               // scheduler computed this order for a job that has finished in the
               // meantime -- ordinary operation in any busy cluster, and the line
               // below says as much. Logged at all because it explains a dropped
               // order when a scheduling decision is traced.
               INFO(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }

            next_ja_task = lFirstRW(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(jep)));

            /* we have to iterate over the ja-tasks and the template */
            jatp = job_get_ja_task_template_pending(jep, 0);
            if (jatp == nullptr) {
               ERROR(MSG_JOB_FINDJOBTASK_UU, static_cast<uint32_t>(0), job_number);
               DRETURN(-2);
            }

            while (jatp != nullptr) {
               ocs::tom::set<JAT_tix>(jatp, 0);
               ocs::tom::set<JAT_oticket>(jatp, 0);
               ocs::tom::set<JAT_fticket>(jatp, 0);
               ocs::tom::set<JAT_sticket>(jatp, 0);
               ocs::tom::set<JAT_share>(jatp, 0);
               ocs::tom::set<JAT_prio>(jatp, 0);
               ocs::tom::set<JAT_ntix>(jatp, 0);
               if (task_number != 0) {   /* if task_number == 0, we only change the */
                  sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
                  jatp = next_ja_task;   /* pending tickets, otherwise all */
                  next_ja_task = lNextRW(next_ja_task);
               } else {
                  jatp = nullptr;
               }
            }

            ocs::tom::set<JB_nurg>(jep, 0);
            ocs::tom::set<JB_urg>(jep, 0);
            ocs::tom::set<JB_rrcontr>(jep, 0);
            ocs::tom::set<JB_dlcontr>(jep, 0);
            ocs::tom::set<JB_wtcontr>(jep, 0);

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
            const lListElem *joker;
            const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

            job_number = ocs::tom::get<OR_job_number>(ep);
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            DPRINTF("ORDER : job(" sge_u32 ")->ticket = %f\n", job_number, ocs::tom::get<OR_ticket>(ep));

            ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_number);
            if (jep == nullptr) {
               // CS-2765: INFO for the reason given at the first of these three.
               INFO(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }

            task_number = ocs::tom::get<OR_ja_task_number>(ep);
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
                  // CS-3031: a task that the job defines but that is neither enrolled
                  // nor pending any more has finished after the scheduler computed
                  // this order. Ignore the order like the one for a finished job:
                  // the task's DEL event has already been sent, a second one makes
                  // every event client fail, and -2 would drop the remaining orders
                  // and reinitialize the scheduler.
                  if (job_is_ja_task_defined(jep, task_number)) {
                     INFO(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
                     DRETURN(0);
                  }
                  ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
                  sge_add_event(now, sgeE_JATASK_DEL, job_number, task_number,
                                nullptr, nullptr, ocs::tom::get<JB_session>(jep), nullptr, gdi_session);
                  DRETURN(-2);
               }
            }

            if (ocs::tom::get<JAT_status>(jatp) == JFINISHED) {
               WARNING(MSG_JOB_CHANGEPTICKETS_UU, ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatp));
               DRETURN(0);
            }

            /* modify jobs ticket amount */
            ocs::tom::set<JAT_tix>(jatp, ocs::tom::get<OR_ticket>(ep));

            /* check several fields to be updated */
            if ((joker = lFirst(ocs::tom::get<OR_joker>(ep)))) {

               ocs::tom::ConstElement<ocs::tom::JAT> joker_task = lFirst(ocs::tom::get<JB_ja_tasks>(joker));

               ocs::tom::set<JAT_oticket>(jatp, ocs::tom::get<JAT_oticket>(joker_task));
               ocs::tom::set<JAT_fticket>(jatp, ocs::tom::get<JAT_fticket>(joker_task));
               ocs::tom::set<JAT_sticket>(jatp, ocs::tom::get<JAT_sticket>(joker_task));
               ocs::tom::set<JAT_share>(jatp, ocs::tom::get<JAT_share>(joker_task));
               ocs::tom::set<JAT_prio>(jatp, ocs::tom::get<JAT_prio>(joker_task));
               ocs::tom::set<JAT_ntix>(jatp, ocs::tom::get<JAT_ntix>(joker_task));

               // @todo do we really update the *job* values with every ja_task update?
               ocs::tom::set<JB_nurg>(jep, ocs::tom::get<JB_nurg>(joker));
               ocs::tom::set<JB_urg>(jep, ocs::tom::get<JB_urg>(joker));
               ocs::tom::set<JB_rrcontr>(jep, ocs::tom::get<JB_rrcontr>(joker));
               ocs::tom::set<JB_dlcontr>(jep, ocs::tom::get<JB_dlcontr>(joker));
               ocs::tom::set<JB_wtcontr>(jep, ocs::tom::get<JB_wtcontr>(joker));
            }

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

            job_number = ocs::tom::get<OR_job_number>(ep);
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            DPRINTF("ORDER: job(" sge_u32 ")->ticket = %f\n",
                    job_number, ocs::tom::get<OR_ticket>(ep));

            ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_number);
            if (jep == nullptr) {
               // CS-2765: INFO for the reason given at the first of these three.
               INFO(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }
            task_number = ocs::tom::get<OR_ja_task_number>(ep);
            if (task_number == 0) {
               ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_tickets");
               DRETURN(-2);
            }
            jatp = job_search_task(jep, nullptr, task_number);
            if (jatp == nullptr) {
               // CS-3031: a finished task, for the reason given at ORT_ptickets.
               if (job_is_ja_task_defined(jep, task_number)) {
                  INFO(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
                  DRETURN(0);
               }
               ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
               sge_add_event(now, sgeE_JATASK_DEL, job_number, task_number, nullptr, nullptr, ocs::tom::get<JB_session>(jep), nullptr, gdi_session);
               DRETURN(-2);
            }

            /* job must be running */
            if (ocs::tom::get<JAT_status>(jatp) != JRUNNING &&
                ocs::tom::get<JAT_status>(jatp) != JTRANSFERING) {

               /* if the job just finished, ignore the order */
               if (ocs::tom::get<JAT_status>(jatp) != JFINISHED) {
                  WARNING(MSG_JOB_CHANGETICKETS_UUU, ocs::tom::get<JB_job_number>(jep),  ocs::tom::get<JAT_task_number>(jatp), ocs::tom::get<JAT_status>(jatp));
                  DRETURN(0);
               }
            } else {
               bool distribute_tickets = false;
               /* modify jobs ticket amount and spool job */
               ocs::tom::set<JAT_tix>(jatp, ocs::tom::get<OR_ticket>(ep));
               DPRINTF("TICKETS: " sge_u32 "." sge_u32 " %f tickets\n",
                       ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatp), ocs::tom::get<JAT_tix>(jatp));

               /* check several fields to be updated */
               if ((joker = lFirst(ocs::tom::get<OR_joker>(ep)))) {

                  ocs::tom::ConstElement<ocs::tom::JAT> joker_task = lFirst(ocs::tom::get<JB_ja_tasks>(joker));
                  distribute_tickets = (ocs::tom::has<JAT_granted_destin_identifier_list>(joker_task)) ? true : false;

                  ocs::tom::set<JAT_oticket>(jatp, ocs::tom::get<JAT_oticket>(joker_task));
                  ocs::tom::set<JAT_fticket>(jatp, ocs::tom::get<JAT_fticket>(joker_task));
                  ocs::tom::set<JAT_sticket>(jatp, ocs::tom::get<JAT_sticket>(joker_task));
                  ocs::tom::set<JAT_share>(jatp, ocs::tom::get<JAT_share>(joker_task));
                  ocs::tom::set<JAT_prio>(jatp, ocs::tom::get<JAT_prio>(joker_task));
                  ocs::tom::set<JAT_ntix>(jatp, ocs::tom::get<JAT_ntix>(joker_task));

                  ocs::tom::set<JB_nurg>(jep, ocs::tom::get<JB_nurg>(joker));
                  ocs::tom::set<JB_urg>(jep, ocs::tom::get<JB_urg>(joker));
                  ocs::tom::set<JB_rrcontr>(jep, ocs::tom::get<JB_rrcontr>(joker));
                  ocs::tom::set<JB_dlcontr>(jep, ocs::tom::get<JB_dlcontr>(joker));
                  ocs::tom::set<JB_wtcontr>(jep, ocs::tom::get<JB_wtcontr>(joker));
               }

               /* tickets should only be further distributed in the scheduler reprioritize_interval. Only in
                  those intervales does the ticket order structure contain a JAT_granted_destin_identifier_list.
                  We use that as an identifier to go on, or not. */
               if (distribute_tickets && topp != nullptr) {
                  const lDescr *rdp = nullptr;

                  lEnumeration *what = lIntVector2What(OR_Type, ticket_orders_field);
                  lReduceDescr(&rdp, OR_Type, what);

                  /* If a ticket order has a queuelist, then this is a parallel job
                     with controlled sub-tasks. We generate a ticket order for
                     each host in the queuelist containing the total tickets for
                     all job slots being used on the host */
                  if (ocs::tom::get<OR_queuelist>(ep)) {
                     lList *host_tickets_cache = lCreateList("", UA_Type); /* cashed temporary hash list */
                     /* set granted slot tickets */
                     for_each_ep(oep, static_cast<const lList *>(ocs::tom::get<OR_queuelist>(ep))) {
                        ocs::tom::Element<ocs::tom::JG> gdil_ep = ocs::tom::find_first_in_rw<JAT_granted_destin_identifier_list, JG_qname>(jatp, ocs::tom::get<OQ_dest_queue>(oep));
                        if (gdil_ep != nullptr) {
                           double tickets = ocs::tom::get<OQ_ticket>(oep);
                           const char *hostname = ocs::tom::get<JG_qhostname>(gdil_ep);

                           ocs::tom::set<JG_ticket>(gdil_ep, tickets);
                           ocs::tom::set<JG_oticket>(gdil_ep, ocs::tom::get<OQ_oticket>(oep));
                           ocs::tom::set<JG_fticket>(gdil_ep, ocs::tom::get<OQ_fticket>(oep));
                           ocs::tom::set<JG_sticket>(gdil_ep, ocs::tom::get<OQ_sticket>(oep));

                           ocs::tom::Element<ocs::tom::UA> chost_ep = ocs::tom::find_first_rw<UA_name>(host_tickets_cache, hostname);
                           if (chost_ep == nullptr) {
                              chost_ep = ocs::tom::emplace_back<UA_name>(&host_tickets_cache, hostname);
                           }
                           ocs::tom::add<UA_value>(chost_ep, tickets);
                        }
                     }

                     /* map cached tickets back to RTIC_Type list */
                     for_each_ep(oep, host_tickets_cache) {
                        const char *hostname = ocs::tom::get<UA_name>(oep);
                        lList *host_tickets;

                        lListElem *newep = lSelectElemDPack(ep, nullptr, rdp, what, false, nullptr);
                        ocs::tom::set<OR_ticket>(newep, ocs::tom::get<UA_value>(oep));

                        ocs::tom::Element<ocs::tom::RTIC> rtic_ep = ocs::tom::find_first_rw<RTIC_host>(*topp, hostname);
                        if (rtic_ep == nullptr) {
                           rtic_ep = ocs::tom::emplace_back<RTIC_host>(topp, hostname);
                        }
                        host_tickets = ocs::tom::get<RTIC_tickets>(rtic_ep);
                        if (host_tickets == nullptr) {
                           host_tickets = lCreateList("ticket orders", rdp);
                           ocs::tom::set<RTIC_tickets>(rtic_ep, host_tickets);
                        }
                        lAppendElem(host_tickets, newep);
                     }
                     lFreeList(&host_tickets_cache);
                  } else {
                     ocs::tom::ConstList<ocs::tom::JG> gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(jatp);
                     if (gdil != nullptr) {
                        lListElem *newep = lSelectElemDPack(ep, nullptr, rdp, what, false, nullptr);
                        lList *host_tickets;
                        const char *hostname = ocs::tom::get<JG_qhostname>(lFirst(gdil));
                        ocs::tom::Element<ocs::tom::RTIC> rtic_ep = ocs::tom::find_first_rw<RTIC_host>(*topp, hostname);
                        if (rtic_ep == nullptr) {
                           rtic_ep = ocs::tom::emplace_back<RTIC_host>(topp, hostname);
                        }
                        host_tickets = ocs::tom::get<RTIC_tickets>(rtic_ep);
                        if (host_tickets == nullptr) {
                           host_tickets = lCreateList("ticket orders", rdp);
                           ocs::tom::set<RTIC_tickets>(rtic_ep, host_tickets);
                        }
                        lAppendElem(host_tickets, newep);
                     }

                  }
                  lFreeWhat(&what);
                  ocs_cull_descr_release(&rdp);
               }
            }
            sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
            sge_add_event(now, sgeE_JOB_MOD, job_number, 0, nullptr, nullptr, nullptr, jep, gdi_session);
         }
         break;

         /* -----------------------------------------------------------------------
          * REMOVE IMMEDIATE JOBS THAT COULD NOT GET SCHEDULED IN THIS PASS
          *
          * Using this order schedd can only remove idle immediate jobs
          * (former ORT_remove_interactive_job)
          *
          * CS-1239: ORT_remove_job is gone - finished-job removal now happens
          * inline in the worker thread (sge_commit_job(COMMIT_ST_FINISHED_FAILED_EE)
          * books usage and buries the job in one step), so the scheduler never
          * emits ORT_remove_job and this case no longer needs to handle the
          * shared "remove finished job" path.
          * ----------------------------------------------------------------------- */
      case ORT_remove_immediate_job: {
         lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
         DPRINTF("ORDER: ORT_remove_immediate_job\n");

         job_number = ocs::tom::get<OR_job_number>(ep);
         if (job_number == 0) {
            ERROR(SFNMAX, MSG_JOB_NOJOBID);
            DRETURN(-2);
         }
         task_number = ocs::tom::get<OR_ja_task_number>(ep);
         if (task_number == 0) {
            ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_remove_immediate_job");
            DRETURN(-2);
         }
         DPRINTF("ORDER: remove immediate job " sge_u32 "." sge_u32 "\n", job_number, task_number);
         ocs::tom::Element<ocs::tom::JB> jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, job_number);
         if (jep == nullptr) {
            /* in case of an immediate parallel job the job could be missing */
            INFO(MSG_JOB_FINDJOB_U, job_number);
            DRETURN(0);
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

            sge_event_spool(&answer_list, 0, sgeE_JATASK_ADD,
                            job_number, task_number, nullptr, nullptr,
                            ocs::tom::get<JB_session>(jep),
                            jep, jatp, nullptr, true, true, gdi_session);
            answer_list_output(&answer_list);
         }

         if (!JOB_TYPE_IS_IMMEDIATE(ocs::tom::get<JB_type>(jep))) {
            if (ocs::tom::get<JB_script_file>(jep)) {
               ERROR(MSG_JOB_REMOVENONINTERACT_U, ocs::tom::get<JB_job_number>(jep));
            } else {
               ERROR(MSG_JOB_REMOVENONIMMEDIATE_U, ocs::tom::get<JB_job_number>(jep));
            }
            DRETURN(-1);
         }
         if (ocs::tom::get<JAT_status>(jatp) != JIDLE) {
            ERROR(MSG_JOB_REMOVENOTIDLEIA_U, ocs::tom::get<JB_job_number>(jep));
            DRETURN(-1);
         }
         INFO(MSG_JOB_NOFREERESOURCEIA_UU, ocs::tom::get<JB_job_number>(jep), ocs::tom::get<JAT_task_number>(jatp), ocs::tom::get<JB_owner>(jep));

         /* remove it */
         sge_commit_job(jep, jatp, nullptr, COMMIT_ST_NO_RESOURCES, COMMIT_DEFAULT | COMMIT_NEVER_RAN, monitor, gdi_session);
         break;
      }

      /* CS-1239: ORT_update_project_usage and ORT_update_user_usage handlers
       * removed. User/project debited-job-usage updates now happen directly
       * in the worker thread (sge_book_finished_job_usage), which marks the
       * affected user/project event-dirty + spool-dirty via
       * ocs::SharetreeUsage. The TET share-tree tick handler
       * (sge_sharetree_tick_handler in sge_thread_timer.cc) drains the event
       * FIFOs and batches USER_MOD / PROJECT_MOD with the trailing
       * NEW_SHARETREE inside one event-master transaction; the TET dirty-
       * objects flush handler takes care of throttled spool persistence. */

      /* CS-1239 step 5: ORT_share_tree handler removed. Recomputation +
       * sgeE_NEW_SHARETREE emission now happen in the merged TET share-tree
       * tick handler (see above), driven by ocs::SharetreeUsage::
       * mark_share_tree_dirty() set from sge_book_finished_job_usage and
       * from the decay step of the tick itself. */

         /* -----------------------------------------------------------------------
          * UPDATE FIELDS IN SCHEDULING CONFIGURATION
          * ----------------------------------------------------------------------- */
      case ORT_sched_conf: {
         DPRINTF("ORDER: ORT_sched_conf\n");

         if (sconf_is()) {
            if (const lListElem *joker = lFirst(ocs::tom::get<OR_joker>(ep)); joker != nullptr) {
               if (int pos = lGetPosViaElem(joker, SC_weight_tickets_override, SGE_NO_ABORT); pos > -1) {
                  uint32_t old_wto = sconf_get_weight_tickets_override();
                  uint32_t new_wto = lGetPosUlong(joker, pos);

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
         uint32_t jobid;
         lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
         lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

         DPRINTF("ORDER: ORT_suspend_on_threshold\n");

         jobid = ocs::tom::get<OR_job_number>(ep);
         task_number = ocs::tom::get<OR_ja_task_number>(ep);

         if (!(jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, jobid))
             || !(jatp = job_search_task(jep, nullptr, task_number))
             || !ocs::tom::get<JAT_granted_destin_identifier_list>(jatp)) {
            /* don't panic - it is probably an exiting job */
            WARNING(MSG_JOB_SUSPOTNOTRUN_UU, jobid, task_number);
         } else {
            const char *qnm = ocs::tom::get<JG_qname>(lFirst(static_cast<const lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatp))));
            queueep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);
            if (queueep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, qnm, jobid);
               DRETURN(-1);
            }

            INFO(MSG_JOB_SUSPTQ_UUS, jobid, task_number, qnm);

            if (!ISSET(ocs::tom::get<JAT_state>(jatp), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatp, monitor);
               state = ocs::tom::get<JAT_state>(jatp);
               CLEARBIT(JRUNNING, state);
               ocs::tom::set<JAT_state>(jatp, state);
            }
            state = ocs::tom::get<JAT_state>(jatp);
            SETBIT(JSUSPENDED_ON_THRESHOLD, state);
            ocs::tom::set<JAT_state>(jatp, state);

            {
               lList *answer_list = nullptr;
               const char *session = ocs::tom::get<JB_session>(jep);
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, jobid, task_number, nullptr, nullptr, session,
                               jep, jatp, nullptr, true, true, gdi_session);
               answer_list_output(&answer_list);
            }

            /* update queues time stamp in schedd */
            ocs::tom::set<QU_last_suspend_threshold_ckeck>(queueep, now);
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD, gdi_session);
         }
      }
         break;

      case ORT_unsuspend_on_threshold: {
         lListElem *queueep;
         uint32_t jobid;
         lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
         lList *master_cqueue_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);

         DPRINTF("ORDER: ORT_unsuspend_on_threshold\n");

         jobid = ocs::tom::get<OR_job_number>(ep);
         task_number = ocs::tom::get<OR_ja_task_number>(ep);

         if (!(jep = ocs::tom::find_first_rw<JB_job_number>(master_job_list, jobid))
             || !(jatp = job_search_task(jep, nullptr, task_number))
             || !ocs::tom::get<JAT_granted_destin_identifier_list>(jatp)) {
            /* don't panic - it is probably an exiting job */
            WARNING(MSG_JOB_UNSUSPOTNOTRUN_UU, jobid, task_number);
         } else {
            const char *qnm = ocs::tom::get<JG_qname>(lFirst(static_cast<const lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatp))));
            queueep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);
            if (queueep == nullptr) {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, qnm, jobid);
               DRETURN(-1);
            }

            INFO(MSG_JOB_UNSUSPOT_UUS, jobid, task_number, qnm);

            if (!ISSET(ocs::tom::get<JAT_state>(jatp), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGCONT, queueep, jep, jatp, monitor);
               state = ocs::tom::get<JAT_state>(jatp);
               SETBIT(JRUNNING, state);
               ocs::tom::set<JAT_state>(jatp, state);
            }
            state = ocs::tom::get<JAT_state>(jatp);
            CLEARBIT(JSUSPENDED_ON_THRESHOLD, state);
            ocs::tom::set<JAT_state>(jatp, state);
            {
               lList *answer_list = nullptr;
               const char *session = ocs::tom::get<JB_session>(jep);
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, jobid, task_number, nullptr, nullptr, session,
                               jep, jatp, nullptr, true, true, gdi_session);
               answer_list_output(&answer_list);
            }
            /* update queues time stamp in schedd */
            ocs::tom::set<QU_last_suspend_threshold_ckeck>(queueep, now);
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD, gdi_session);
         }
      }
         break;

      case ORT_job_schedd_info: {
         lList *sub_order_list = ocs::tom::get<OR_joker>(ep);
         lList **master_job_schedd_info_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB_SCHEDD_INFO);

         DPRINTF("ORDER: ORT_job_schedd_info\n");

         if (sub_order_list != nullptr) {
            lListElem *sme = lFirstRW(sub_order_list);

            if (sme != nullptr) {
               lListElem *first;

               DPRINTF("ORDER: got %d schedd infos\n", lGetNumberOfElem(ocs::tom::get<SME_message_list>(sme)));

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
/** @brief Send the scheduler's ticket decisions on to the execution hosts
 *
 * Tickets are what the priority translation facility on each host turns into
 * nice values, so an order that never arrives leaves that host scheduling on
 * stale entitlement.
 *
 * @param ticket_orders the orders to distribute
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int distribute_ticket_orders(lList *ticket_orders, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   uint64_t now = sge_get_gmt64();
   unsigned long last_heard_from = 0;
   int cl_err = CL_RETVAL_OK;
   const lListElem *ep;
   const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);

   for_each_ep(ep, ticket_orders) {
      ocs::tom::ConstList<ocs::tom::OR> to_send = ocs::tom::get<RTIC_tickets>(ep);
      const char *host_name = ocs::tom::get<RTIC_host>(ep);
      const lListElem *hep = host_list_locate(master_ehost_list, host_name);

      int n = lGetNumberOfElem(to_send);

      if (hep) {
         cl_commlib_get_last_message_time((cl_com_get_handle(to_cstr(QMASTER), 0)),
                                          host_name, to_cstr(EXECD), 1, &last_heard_from);
      }
      if (hep &&sge_gmt32_to_gmt64(last_heard_from + 10 * mconf_get_load_report_time()) > now) {
         sge_pack_buffer pb;

         if (init_packbuffer(&pb, sizeof(uint32_t) * 3 * n) == PACK_SUCCESS) {
            uint32_t dummyid = 0;
            ocs::tom::ConstElement<ocs::tom::OR> ep2;
            for_each_ep(ep2, to_send) {
               packint(&pb, ocs::tom::get<OR_job_number>(ep2));
               packint(&pb, ocs::tom::get<OR_ja_task_number>(ep2));
               packdouble(&pb, ocs::tom::get<OR_ticket>(ep2));
            }
            cl_err = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, to_cstr(EXECD), 1, host_name,
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
