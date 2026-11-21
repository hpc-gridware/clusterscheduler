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
#include <optional>
#include <pthread.h>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_ShareTree.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_host.h"
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
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"

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
#include "tom/ocs_PackCore.h"

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

/// The order fields a ticket order to an execd carries
static constexpr int ticket_orders_field[] = {OR_job_number,
                                              OR_ja_task_number,
                                              OR_ticket};

/**
 * @brief Unenroll an array task that was enrolled for a start order which cannot be followed
 *
 * job_unenroll() has no typed form for its out-parameter yet, so the handle is
 * bridged through a raw pointer here.
 *
 * @param jep the job
 * @param jatp the array task; an empty handle afterwards, the task is freed
 */
static void
ocs_follow_unenroll_task(const ocs::tom::Element<ocs::tom::JB> jep, ocs::tom::Element<ocs::tom::JAT> &jatp) {
   DENTER(TOP_LAYER);

   lListElem *raw_jatp = jatp;
   job_unenroll(jep, nullptr, &raw_jatp);
   jatp = ocs::tom::Element<ocs::tom::JAT>::from_typed(raw_jatp);

   DRETURN_VOID;
}

/**
 * @brief The ticket orders collected for one host, with the host's entry and the lists created if needed
 *
 * @param ticket_orders the ticket orders per host; the list is created if the handle is empty
 * @param hostname the host
 * @param what the fields a ticket order to an execd carries; a list created for the host gets the layout this
 *        selection reduces the ticket orders to, the layout of the copies OR elements select() makes with it
 * @return the ticket orders of the host
 * @throw ocs::RuntimeException if there is no memory
 */
static ocs::tom::List<ocs::tom::OR>
ocs_follow_host_ticket_orders(ocs::tom::ListPtr<ocs::tom::RTIC> &ticket_orders, const char *hostname,
                              const ocs::tom::What<ocs::tom::OR> &what) {
   DENTER(TOP_LAYER);
   if (!ticket_orders) {
      // the name and hashing ocs::tom::emplace_back() gave a list it created
      ticket_orders = ocs::tom::ListPtr<ocs::tom::RTIC>::create("");
   }
   auto rtic_ep = ticket_orders->find_first<RTIC_host>(ocs::tom::Host(hostname));
   if (!rtic_ep) {
      rtic_ep = ticket_orders->emplace_back<RTIC_host>(ocs::tom::Host(hostname));
   }
   auto host_tickets = rtic_ep.get<RTIC_tickets>();
   if (!host_tickets) {
      rtic_ep.set<RTIC_tickets>(ocs::tom::ListPtr<ocs::tom::OR>::create("ticket orders", what));
      host_tickets = rtic_ep.get<RTIC_tickets>();
   }
   DRETURN(host_tickets);
}

/**
 * @brief Add a reduced copy of a ticket order to the ticket orders of one host, on the soft path of the old code
 *
 * Runs in a qmaster worker thread, where an exception that escapes
 * terminates the daemon. A copy that cannot be made (no memory) is left out,
 * as lSelectElemDPack() gave nullptr and lAppendElem() refused it; the entry
 * of the host and its list are made all the same, as before. If even they
 * cannot be made, nothing is added, where the old code went on with a null
 * list as well.
 *
 * @param ticket_orders the ticket orders per host; the list is created if the handle is empty
 * @param hostname the host
 * @param order the ticket order to copy
 * @param what the fields the copy carries
 * @param tickets the tickets of the copy, or no value to keep those of @p order
 */
static void
ocs_follow_add_host_ticket_order(ocs::tom::ListPtr<ocs::tom::RTIC> &ticket_orders, const char *hostname,
                                 const ocs::tom::ConstElement<ocs::tom::OR> order,
                                 const ocs::tom::What<ocs::tom::OR> &what, const std::optional<double> tickets) {
   DENTER(TOP_LAYER);
   try {
      ocs::tom::ElementPtr<ocs::tom::OR> copy;
      try {
         // the selection caches its reduction, so every copy has the layout of the lists of the hosts
         copy = order.select(what, ocs::tom::Hashing::Off);
      } catch (const ocs::RuntimeException &) {
         // no copy, as lSelectElemDPack() gave none; the entry of the host is made all the same
      }
      if (copy && tickets.has_value()) {
         copy->set<OR_ticket>(*tickets);
      }
      const auto host_tickets = ocs_follow_host_ticket_orders(ticket_orders, hostname, what);
      if (copy) {
         host_tickets.push_back(std::move(copy));
      }
   } catch (const ocs::RuntimeException &) {
      // no memory for the entry or the list of the host; the old code went on without them
   } catch (const ocs::tom::FieldException &) {
      // cannot happen for a full ticket order and a selection of its fields; the old code went on as well
   }
   DRETURN_VOID;
}

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
 * @param ep          the order; request data, structure checked by the listener (CS-3125)
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
sge_follow_order(const ocs::tom::Element<ocs::tom::OR> ep, [[maybe_unused]] char *ruser, [[maybe_unused]] char *rhost,
                 ocs::tom::ListPtr<ocs::tom::RTIC> *topp, monitoring_t *monitor, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   uint32_t job_number, task_number;
   const char *or_pe, *q_name = nullptr;
   uint32_t or_type;
   ocs::tom::Element<ocs::tom::JAT> jatp;
   uint32_t state;
   uint32_t pe_slots = 0, q_slots = 0, q_version;
   uint64_t now = sge_get_gmt64();

   or_type = ep.get<OR_type>();
   or_pe = ep.get<OR_pe>();

   DPRINTF("-----------------------------------------------------------------------\n");

   switch (or_type) {

      /* -----------------------------------------------------------------------
       * START JOB
       * ----------------------------------------------------------------------- */
      case ORT_start_job: {
         ocs::tom::ListPtr<ocs::tom::JG> gdil;
         ocs::tom::ConstElement<ocs::tom::QU> master_qep;
         ocs::tom::Element<ocs::tom::EH> master_host;
         ocs::tom::Element<ocs::tom::PE> pe;
         const auto exec_host_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
         const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
         const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true);
         const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
         const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
         const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
         const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();

         DPRINTF("ORDER ORT_start_job\n");

         job_number = ep.get<OR_job_number>();
         if (job_number == 0) {
            ERROR(SFNMAX, MSG_JOB_NOJOBID);
            DRETURN(-2);
         }

         task_number = ep.get<OR_ja_task_number>();
         if (task_number == 0) {
            ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_start_job");
            DRETURN(-2);
         }

         const auto jep = master_job_list.find_first<JB_job_number>(job_number);
         if (!jep) {
            WARNING(MSG_JOB_FINDJOB_U, job_number);
            DRETURN(-1);
         }

         DPRINTF("ORDER to start Job %ld Task %ld\n", (long) job_number, (long) task_number);

         /* if job got modified in the meantime (qalter), do not start it */
         if (jep.get<JB_version>() != ep.get<OR_job_version>()) {
            WARNING(MSG_ORD_OLDVERSION_UUU, job_number, task_number, ep.get<OR_job_version>());
            DRETURN(-1);
         }

         /* search and enroll task */
         bool enrolled_task = false;
         jatp = job_search_task(jep, nullptr, task_number);
         if (!jatp) {
            if (range_list_is_id_within(jep.get<JB_ja_n_h_ids>(), task_number)) {
               jatp = job_create_task(jep, nullptr, task_number);
               if (!jatp) {
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
         if (jatp.get<JAT_status>() != JIDLE) {
            if (enrolled_task) {
               ocs_follow_unenroll_task(jep, jatp);
            }
            // @todo add more info, e.g. in which state / status the job is
            ERROR(MSG_ORD_TWICE_UU, job_number, task_number);
            DRETURN(-1);
         }

         /* pe job - find the pe object */
         if (or_pe) {
            pe = pe_list_locate(master_pe_list, or_pe);
            if (!pe) {
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               ERROR(MSG_OBJ_UNABLE2FINDPE_S, or_pe);
               DRETURN(-2);
            }
            jatp.set<JAT_granted_pe>(or_pe);  /* free me on error! */
         }

         /* job is running in an advance reservation - find it */
         uint32_t ar_id = jep.get<JB_ar>();
         if (ar_id != 0) {
            const auto ar = ar_list_locate(master_ar_list, jep.get<JB_ar>());
            if (!ar) {
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               ERROR(MSG_CONFIG_CANTFINDARXREFERENCEDINJOBY_UU, jep.get<JB_ar>(), jep.get<JB_job_number>());
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               DRETURN(-2);
            }
            jatp.set<JAT_wallclock_limit>(
                      ar.get<AR_end_time>() - now - sge_gmt32_to_gmt64(sconf_get_duration_offset()));
         }

         /* fill number of tickets into job */
         jatp.set<JAT_tix>(ep.get<OR_ticket>());
         jatp.set<JAT_ntix>(ep.get<OR_ntix>());
         jatp.set<JAT_prio>(ep.get<OR_prio>());

         /**
          * Move the order granted resource list into a JAT granted
          * resource list, which is sent to the execution daemon. This is
          * done for the RSMAP complex type and hard and soft requests
          * which are passed to the execution daemon.
          **/
         jatp.swap<JAT_granted_resources_list, OR_granted_resources_list>(ep);

         if (const auto oep = ep.get<OR_queuelist>().front(); oep) {
            jatp.set<JAT_oticket>(oep.get<OQ_oticket>());
            jatp.set<JAT_fticket>(oep.get<OQ_fticket>());
            jatp.set<JAT_sticket>(oep.get<OQ_sticket>());
         }

         for (const auto oep : ep.get<OR_queuelist>()) {

            q_name = oep.get<OQ_dest_queue>();
            q_version = oep.get<OQ_dest_version>();
            q_slots = oep.get<OQ_slots>();

            /* ----------------------
             *  find and check queue
             */
            if (!q_name) {
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               ERROR(SFNMAX, MSG_OBJ_NOQNAME);
               DRETURN(-2);
            }

            DPRINTF("ORDER: start %d slots of job \"%d\" on"
                    " queue \"%s\" v" sge_u32 " with " sge_u32 " initial tickets\n",
                    q_slots, job_number, q_name, q_version, static_cast<uint32_t>(ep.get<OR_ticket>()));

            const auto qep = cqueue_list_locate_qinstance(master_cqueue_list, q_name);
            if (!qep) {
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               ERROR(MSG_CONFIG_CANTFINDQUEUEXREFERENCEDINJOBY_SU, q_name, job_number);
               DRETURN(-2);
            }

            /* check queue version */
            if (q_version != qep.get<QU_version>()) {
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               /* try to repair schedd data */
               qinstance_add_event(qep, sgeE_QINSTANCE_MOD, gdi_session);
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               WARNING(MSG_ORD_QVERSION_SUU, q_name, q_version,  qep.get<QU_version>());
               DRETURN(-1);
            }

            /* the first queue is the master queue */
            if (!master_qep) {
               master_qep = qep;
            }

            DPRINTF("Queue version: %d\n", q_version);

            /* ensure that the jobs owner has access to this queue */
            if (!sge_has_access_(jep.get<JB_owner>(), jep.get<JB_group>(), jep.get<JB_grp_list>(),
                                 qep.get<QU_acl>(), qep.get<QU_xacl>(),
                                 master_userset_list)) {
               ERROR(MSG_JOB_JOBACCESSQ_US, job_number, q_name);
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               DRETURN(-1);
            }

            /* ensure that this queue has enough free slots */
            if (qep.get<QU_job_slots>() - qinstance_slots_used(qep) < q_slots) {
               ERROR(MSG_JOB_FREESLOTS_USUU, q_slots, q_name, job_number, task_number);
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
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
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               DRETURN(-1);
            }
            if (qinstance_state_is_cal_suspended(qep)) {
               WARNING(MSG_JOB_QSUSPCAL_S, q_name);
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               DRETURN(-1);
            }
            if (qinstance_state_is_cal_disabled(qep)) {
               WARNING(MSG_JOB_QDISABLECAL_S, q_name);
               gdil.reset();
               jatp.set<JAT_granted_pe>(nullptr);
               DRETURN(-1);
            }

            /* ----------------------
             *  find and check host
             */
            const ocs::tom::Element<ocs::tom::EH> hep(
               host_list_locate(exec_host_list, qep.get<QU_qhostname>()));
            if (!hep) {
               ERROR(MSG_JOB_UNABLE2FINDHOST_S, qep.get<QU_qhostname>().c_str());
               if (enrolled_task) {
                  ocs_follow_unenroll_task(jep, jatp);
               }
               gdil.reset();
               if (jatp) {   // empty if the enrolled task was unenrolled above
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               DRETURN(-2);
            } else {
               /* host not yet clean after reschedule unknown? */
               for (const auto ruep : hep.get<EH_reschedule_unknown_list>()) {
                  if (job_number == ruep.get<RU_job_number>()
                      && task_number == ruep.get<RU_task_number>()) {
                     ERROR(MSG_JOB_UNABLE2STARTJOB_US, ruep.get<RU_job_number>(), qep.get<QU_qhostname>().c_str());
                     if (enrolled_task) {
                        ocs_follow_unenroll_task(jep, jatp);
                     }
                     gdil.reset();
                     if (jatp) {   // empty if the enrolled task was unenrolled above
                        jatp.set<JAT_granted_pe>(nullptr);
                     }
                     DRETURN(-1);
                  }
               }
            }

            /* ------------------------------------------------
             *  build up granted_destin_identifier_list (gdil)
             */
            if (!gdil) {
               // the name the free emplace_back() gave a list it created
               gdil = ocs::tom::ListPtr<ocs::tom::JG>::create("");
            }
            const auto gdil_ep = gdil->emplace_back<JG_qname>(q_name); /* free me on error! */
            gdil_ep.set<JG_qhostname>(qep.get<QU_qhostname>());
            gdil_ep.set<JG_slots>(q_slots);
            // the execd uses the version to tell a queue instance change from an unchanged one
            gdil_ep.set<JG_qversion>(qep.get<QU_version>());
            gdil_ep.set<JG_binding_to_use>(oep.get<OQ_binding_to_use>().clone("binding_to_use"));

            /* ------------------------------------------------
             *  tag each gdil entry of slave exec host
             *  in case of sge controlled slaves
             *  this triggers our retry for delivery of slave jobs
             *  and gets untagged when ack has arrived
             */
            if (pe && pe.get<PE_control_slaves>()) {
               gdil_ep.set<JG_ticket>(oep.get<OQ_ticket>());
               gdil_ep.set<JG_oticket>(oep.get<OQ_oticket>());
               gdil_ep.set<JG_fticket>(oep.get<OQ_fticket>());
               gdil_ep.set<JG_sticket>(oep.get<OQ_sticket>());

               /* the master host is the host of the master queue */
               if (master_host && master_host != hep) {

                  /* ensure each host gets tagged only one time
                   * we tag the first entry for a host in the existing gdil
                   */
                  const auto first_at_host = gdil->find_first<JG_qhostname>(hep.get<EH_name>());
                  if (!first_at_host) {
                     ERROR(MSG_JOB_HOSTNAMERESOLVE_US, jep.get<JB_job_number>(), hep.get<EH_name>().c_str());
                  } else {
                     first_at_host.set<JG_tag_slave_job>(1);
                  }
               } else {
                  master_host = hep;
                  DPRINTF("master host %s\n", master_host.get<EH_name>().c_str());
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
            auto next_gdil_ep = gdil->front();
            ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list = jatp.get<JAT_granted_resources_list>();

            /* loop over gdil */
            host_name = next_gdil_ep.get<JG_qhostname>();
            while (next_gdil_ep) {
               const auto gdil_ep = next_gdil_ep;
               uint32_t slots = gdil_ep.get<JG_slots>();

               /* sum up slots */
               host_slots += slots;
               total_slots += slots;

               /* gdil end or switch to next host: check booking on the current host */
               next_gdil_ep = gdil_ep.next();
               if (!next_gdil_ep || strcmp(host_name, next_gdil_ep.get<JG_qhostname>()) != 0) {
                  const ocs::tom::Element<ocs::tom::EH> hep(host_list_locate(exec_host_list, host_name));
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
                  if (next_gdil_ep) {
                     host_name = next_gdil_ep.get<JG_qhostname>();
                     host_slots = 0;
                  }
               }
            }

            /* Per host checks were OK? Then check global host. */
            if (consumables_ok) {
               const ocs::tom::Element<ocs::tom::EH> global_hep(host_list_locate(exec_host_list, SGE_GLOBAL_NAME));
               debit_host_consumable(jep, jatp, granted_resources_list, pe, global_hep, master_centry_list, total_slots, true, true,
                                     &consumables_ok);
            }

            /* Consumable check failed - we cannot start this job! */
            if (!consumables_ok) {
               ERROR(MSG_JOB_RESOURCESNOLONGERAVAILABLE_UU, job_number, task_number);
               if (enrolled_task) {
                  // frees the task and sets jatp to nullptr
                  ocs_follow_unenroll_task(jep, jatp);
               } else {
                  jatp.set<JAT_granted_pe>(nullptr);
               }
               gdil.reset();
               DRETURN(0);
            }
         }

         /* fill in master_queue */
         jatp.set<JAT_master_queue>(master_qep.get<QU_full_name>());
         // the array task takes the list over; the handle stays for the subordinate queues below
         const ocs::tom::List<ocs::tom::JG> granted_gdil = *gdil;
         jatp.set<JAT_granted_destin_identifier_list>(std::move(gdil));
         // store the pe - we need it for unbooking when the job terminates
         if (pe) {
            jatp.set<JAT_pe_object>(pe.clone());
         }

         // @todo: can this be summarized with the mod event that will set the job in t-state?
         sge_add_event(now, enrolled_task ? sgeE_JATASK_ADD : sgeE_JATASK_MOD, job_number, task_number,
                       nullptr, nullptr, jep.get<JB_session>(), jatp, gdi_session);

         if (sge_give_job(jep, jatp, master_qep, master_host, monitor, gdi_session)) {
            /* setting of queues in state unheard is done by sge_give_job() */
            sge_commit_job(jep, jatp, nullptr, COMMIT_ST_DELIVERY_FAILED, COMMIT_DEFAULT, monitor, gdi_session);
            /* This was sge_commit_job(jep, COMMIT_ST_RESCHEDULED). It raised problems if a job
               could not be delivered. The jobslotsfree had been increased even if
               they were not decreased before. */

            ERROR(MSG_JOB_JOBDELIVER_UU, jep.get<JB_job_number>(), jatp.get<JAT_task_number>());
            DRETURN(-3);
         }

         /* job is now sent and goes into transferring state */
         /* mode == COMMIT_ST_SENT -> really accept when execd acks */
         sge_commit_job(jep, jatp, nullptr, COMMIT_ST_SENT, COMMIT_DEFAULT, monitor, gdi_session);

         /* now send events and spool the job */
         {
            ocs::tom::ListPtr<ocs::tom::AN> answer_list;
            const char *session = jep.get<JB_session>();

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

         if (pe) {
            pe_debit_slots(pe, pe_slots, job_number);
            /* this info is not spooled */
            sge_add_event(now, sgeE_PE_MOD, 0, 0, jatp.get<JAT_granted_pe>(), nullptr, nullptr, pe, gdi_session);
         }

         DPRINTF("successfully handed off job \"" sge_u32 "\" to queue \"%s\"\n",
                 jep.get<JB_job_number>(), jatp.get<JAT_master_queue>());

         /* now after successfully (we hope) sent the job to execd
          * suspend all subordinated queues that need suspension
          */
         cqueue_list_x_on_subordinate_gdil(master_cqueue_list, true, granted_gdil, monitor, gdi_session);
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
            const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true);

            job_number = ep.get<OR_job_number>();
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            task_number = ep.get<OR_ja_task_number>();

            const auto jep = master_job_list.find_first<JB_job_number>(job_number);
            if (!jep) {
               // CS-2765: INFO, not a warning and certainly not an error. The
               // scheduler computed this order for a job that has finished in the
               // meantime -- ordinary operation in any busy cluster, and the line
               // below says as much. Logged at all because it explains a dropped
               // order when a scheduling decision is traced.
               INFO(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }

            auto next_ja_task = jep.get<JB_ja_tasks>().front();

            /* we have to iterate over the ja-tasks and the template */
            jatp = job_get_ja_task_template_pending(jep, 0);
            if (!jatp) {
               ERROR(MSG_JOB_FINDJOBTASK_UU, static_cast<uint32_t>(0), job_number);
               DRETURN(-2);
            }

            while (jatp) {
               jatp.set<JAT_tix>(0);
               jatp.set<JAT_oticket>(0);
               jatp.set<JAT_fticket>(0);
               jatp.set<JAT_sticket>(0);
               jatp.set<JAT_share>(0);
               jatp.set<JAT_prio>(0);
               jatp.set<JAT_ntix>(0);
               if (task_number != 0) {   /* if task_number == 0, we only change the */
                  sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
                  jatp = next_ja_task;   /* pending tickets, otherwise all */
                  next_ja_task = next_ja_task.next();
               } else {
                  jatp = nullptr;
               }
            }

            jep.set<JB_nurg>(0);
            jep.set<JB_urg>(0);
            jep.set<JB_rrcontr>(0);
            jep.set<JB_dlcontr>(0);
            jep.set<JB_wtcontr>(0);

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
            const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true);

            job_number = ep.get<OR_job_number>();
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            DPRINTF("ORDER : job(" sge_u32 ")->ticket = %f\n", job_number, ep.get<OR_ticket>());

            const auto jep = master_job_list.find_first<JB_job_number>(job_number);
            if (!jep) {
               // CS-2765: INFO for the reason given at the first of these three.
               INFO(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }

            task_number = ep.get<OR_ja_task_number>();
            if (task_number == 0) {
               ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_ptickets");
               DRETURN(-2);
            }

            bool send_task_event = true;
            jatp = job_search_task(jep, nullptr, task_number);
            if (!jatp) {
               send_task_event = false;
               jatp = job_get_ja_task_template_pending(jep, task_number);

               if (!jatp) {
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
                                nullptr, nullptr, jep.get<JB_session>(), nullptr, gdi_session);
                  DRETURN(-2);
               }
            }

            if (jatp.get<JAT_status>() == JFINISHED) {
               WARNING(MSG_JOB_CHANGEPTICKETS_UU, jep.get<JB_job_number>(), jatp.get<JAT_task_number>());
               DRETURN(0);
            }

            /* modify jobs ticket amount */
            jatp.set<JAT_tix>(ep.get<OR_ticket>());

            /* check several fields to be updated */
            // OR_joker names no sub-type in the model; for a ticket order it holds a reduced job
            if (const auto joker = ocs::tom::ConstList<ocs::tom::JB>(ep.get<OR_joker>()).front(); joker) {

               const auto joker_task = joker.get<JB_ja_tasks>().front();

               jatp.set<JAT_oticket>(joker_task.get<JAT_oticket>());
               jatp.set<JAT_fticket>(joker_task.get<JAT_fticket>());
               jatp.set<JAT_sticket>(joker_task.get<JAT_sticket>());
               jatp.set<JAT_share>(joker_task.get<JAT_share>());
               jatp.set<JAT_prio>(joker_task.get<JAT_prio>());
               jatp.set<JAT_ntix>(joker_task.get<JAT_ntix>());

               // @todo do we really update the *job* values with every ja_task update?
               jep.set<JB_nurg>(joker.get<JB_nurg>());
               jep.set<JB_urg>(joker.get<JB_urg>());
               jep.set<JB_rrcontr>(joker.get<JB_rrcontr>());
               jep.set<JB_dlcontr>(joker.get<JB_dlcontr>());
               jep.set<JB_wtcontr>(joker.get<JB_wtcontr>());
            }

            if (send_task_event) {
               sge_add_event(now, sgeE_JATASK_MOD, job_number, task_number, nullptr, nullptr, nullptr, jatp, gdi_session);
            }
            sge_add_event(now, sgeE_JOB_MOD, job_number, 0, nullptr, nullptr, nullptr, jep, gdi_session);

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
            const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>(true);

            job_number = ep.get<OR_job_number>();
            if (job_number == 0) {
               ERROR(SFNMAX, MSG_JOB_NOJOBID);
               DRETURN(-2);
            }

            DPRINTF("ORDER: job(" sge_u32 ")->ticket = %f\n",
                    job_number, ep.get<OR_ticket>());

            const auto jep = master_job_list.find_first<JB_job_number>(job_number);
            if (!jep) {
               // CS-2765: INFO for the reason given at the first of these three.
               INFO(MSG_JOB_UNABLE2FINDJOBORD_U, job_number);
               DRETURN(0); /* it's ok - job has exited - forget about him */
            }
            task_number = ep.get<OR_ja_task_number>();
            if (task_number == 0) {
               ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_tickets");
               DRETURN(-2);
            }
            jatp = job_search_task(jep, nullptr, task_number);
            if (!jatp) {
               // CS-3031: a finished task, for the reason given at ORT_ptickets.
               if (job_is_ja_task_defined(jep, task_number)) {
                  INFO(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
                  DRETURN(0);
               }
               ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
               sge_add_event(now, sgeE_JATASK_DEL, job_number, task_number, nullptr, nullptr, jep.get<JB_session>(),
                             nullptr, gdi_session);
               DRETURN(-2);
            }

            /* job must be running */
            if (jatp.get<JAT_status>() != JRUNNING &&
                jatp.get<JAT_status>() != JTRANSFERING) {

               /* if the job just finished, ignore the order */
               if (jatp.get<JAT_status>() != JFINISHED) {
                  WARNING(MSG_JOB_CHANGETICKETS_UUU, jep.get<JB_job_number>(),  jatp.get<JAT_task_number>(),
                          jatp.get<JAT_status>());
                  DRETURN(0);
               }
            } else {
               bool distribute_tickets = false;
               /* modify jobs ticket amount and spool job */
               jatp.set<JAT_tix>(ep.get<OR_ticket>());
               DPRINTF("TICKETS: " sge_u32 "." sge_u32 " %f tickets\n",
                       jep.get<JB_job_number>(), jatp.get<JAT_task_number>(), jatp.get<JAT_tix>());

               /* check several fields to be updated */
               // OR_joker names no sub-type in the model; for a ticket order it holds a reduced job
               if (const auto joker = ocs::tom::ConstList<ocs::tom::JB>(ep.get<OR_joker>()).front(); joker) {

                  const auto joker_task = joker.get<JB_ja_tasks>().front();
                  distribute_tickets = (joker_task.has<JAT_granted_destin_identifier_list>()) ? true : false;

                  jatp.set<JAT_oticket>(joker_task.get<JAT_oticket>());
                  jatp.set<JAT_fticket>(joker_task.get<JAT_fticket>());
                  jatp.set<JAT_sticket>(joker_task.get<JAT_sticket>());
                  jatp.set<JAT_share>(joker_task.get<JAT_share>());
                  jatp.set<JAT_prio>(joker_task.get<JAT_prio>());
                  jatp.set<JAT_ntix>(joker_task.get<JAT_ntix>());

                  jep.set<JB_nurg>(joker.get<JB_nurg>());
                  jep.set<JB_urg>(joker.get<JB_urg>());
                  jep.set<JB_rrcontr>(joker.get<JB_rrcontr>());
                  jep.set<JB_dlcontr>(joker.get<JB_dlcontr>());
                  jep.set<JB_wtcontr>(joker.get<JB_wtcontr>());
               }

               /* tickets should only be further distributed in the scheduler reprioritize_interval. Only in
                  those intervales does the ticket order structure contain a JAT_granted_destin_identifier_list.
                  We use that as an identifier to go on, or not. */
               if (distribute_tickets && topp != nullptr) {
                  // the selection caches its reduction, so every ticket order of the host shares one layout
                  ocs::tom::What<ocs::tom::OR> what;
                  try {
                     what = ocs::tom::what<ocs::tom::OR>(ticket_orders_field);
                  } catch (const ocs::RuntimeException &) {
                     // no memory: no ticket orders for the execds this time, a worker thread must not see the
                     // exception
                  }

                  /* If a ticket order has a queuelist, then this is a parallel job
                     with controlled sub-tasks. We generate a ticket order for
                     each host in the queuelist containing the total tickets for
                     all job slots being used on the host */
                  if (!what) {
                     // no selection, no ticket orders to distribute
                  } else if (ep.get<OR_queuelist>()) {
                     /* cashed temporary hash list */
                     auto host_tickets_cache = ocs::tom::ListPtr<ocs::tom::UA>::create("");
                     /* set granted slot tickets */
                     for (const auto oep : ep.get<OR_queuelist>()) {
                        const auto gdil_ep = jatp.find_first_in_rw<JAT_granted_destin_identifier_list, JG_qname>(
                                 oep.get<OQ_dest_queue>());
                        if (gdil_ep) {
                           double tickets = oep.get<OQ_ticket>();
                           const char *hostname = gdil_ep.get<JG_qhostname>();

                           gdil_ep.set<JG_ticket>(tickets);
                           gdil_ep.set<JG_oticket>(oep.get<OQ_oticket>());
                           gdil_ep.set<JG_fticket>(oep.get<OQ_fticket>());
                           gdil_ep.set<JG_sticket>(oep.get<OQ_sticket>());

                           auto chost_ep = host_tickets_cache->find_first<UA_name>(hostname);
                           if (!chost_ep) {
                              chost_ep = host_tickets_cache->emplace_back<UA_name>(hostname);
                           }
                           chost_ep.add<UA_value>(tickets);
                        }
                     }

                     /* map cached tickets back to RTIC list */
                     for (const auto cached : *host_tickets_cache) {
                        const char *hostname = cached.get<UA_name>();
                        ocs_follow_add_host_ticket_order(*topp, hostname, ep, what, cached.get<UA_value>());
                     }
                     host_tickets_cache.reset();
                  } else {
                     ocs::tom::ConstList<ocs::tom::JG> gdil = jatp.get<JAT_granted_destin_identifier_list>();
                     if (gdil) {
                        const char *hostname = gdil.front().get<JG_qhostname>();
                        ocs_follow_add_host_ticket_order(*topp, hostname, ep, what, std::nullopt);
                     }

                  }
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
         const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
         DPRINTF("ORDER: ORT_remove_immediate_job\n");

         job_number = ep.get<OR_job_number>();
         if (job_number == 0) {
            ERROR(SFNMAX, MSG_JOB_NOJOBID);
            DRETURN(-2);
         }
         task_number = ep.get<OR_ja_task_number>();
         if (task_number == 0) {
            ERROR(MSG_JOB_NOORDERTASK_US, job_number, "ORT_remove_immediate_job");
            DRETURN(-2);
         }
         DPRINTF("ORDER: remove immediate job " sge_u32 "." sge_u32 "\n", job_number, task_number);
         const auto jep = master_job_list.find_first<JB_job_number>(job_number);
         if (!jep) {
            /* in case of an immediate parallel job the job could be missing */
            INFO(MSG_JOB_FINDJOB_U, job_number);
            DRETURN(0);
         }
         jatp = job_search_task(jep, nullptr, task_number);

         /* if ja task doesn't exist yet, create it */
         if (!jatp) {
            ocs::tom::ListPtr<ocs::tom::AN> answer_list;

            jatp = job_create_task(jep, nullptr, task_number);

            /* new jatask has to be spooled and event sent */
            if (!jatp) {
               ERROR(MSG_JOB_FINDJOBTASK_UU, task_number, job_number);
               DRETURN(-1);
            }

            sge_event_spool(&answer_list, 0, sgeE_JATASK_ADD,
                            job_number, task_number, nullptr, nullptr,
                            jep.get<JB_session>(),
                            jep, jatp, nullptr, true, true, gdi_session);
            answer_list_output(&answer_list);
         }

         if (!JOB_TYPE_IS_IMMEDIATE(jep.get<JB_type>())) {
            if (jep.get<JB_script_file>()) {
               ERROR(MSG_JOB_REMOVENONINTERACT_U, jep.get<JB_job_number>());
            } else {
               ERROR(MSG_JOB_REMOVENONIMMEDIATE_U, jep.get<JB_job_number>());
            }
            DRETURN(-1);
         }
         if (jatp.get<JAT_status>() != JIDLE) {
            ERROR(MSG_JOB_REMOVENOTIDLEIA_U, jep.get<JB_job_number>());
            DRETURN(-1);
         }
         INFO(MSG_JOB_NOFREERESOURCEIA_UU, jep.get<JB_job_number>(), jatp.get<JAT_task_number>(), jep.get<JB_owner>());

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
            // OR_joker names no sub-type in the model; for this order it holds a reduced scheduler configuration
            if (const auto joker = ocs::tom::ConstList<ocs::tom::SC>(ep.get<OR_joker>()).front(); joker) {
               if (joker.has<SC_weight_tickets_override>()) {
                  uint32_t old_wto = sconf_get_weight_tickets_override();
                  uint32_t new_wto = joker.get<SC_weight_tickets_override>();

                  if (old_wto != new_wto) {
                     sconf_set_weight_tickets_override(new_wto);

                     // no need to spool but other data stores need to be updated
                     const auto sconfig = ocs::DataStore::master_list_rw<SGE_TYPE_SCHEDD_CONF>(true).front();
                     sge_add_event(now, sgeE_SCHED_CONF, 0, 0,
                                   "schedd_conf", nullptr, nullptr, sconfig, gdi_session);
                  }
               }
            }
         }
         break;
      }
      case ORT_suspend_on_threshold: {
         uint32_t jobid;
         ocs::tom::Element<ocs::tom::JB> jep;
         const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
         const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

         DPRINTF("ORDER: ORT_suspend_on_threshold\n");

         jobid = ep.get<OR_job_number>();
         task_number = ep.get<OR_ja_task_number>();

         if (!(jep = master_job_list.find_first<JB_job_number>(jobid))
             || !(jatp = job_search_task(jep, nullptr, task_number))
             || !jatp.get<JAT_granted_destin_identifier_list>()) {
            /* don't panic - it is probably an exiting job */
            WARNING(MSG_JOB_SUSPOTNOTRUN_UU, jobid, task_number);
         } else {
            const char *qnm = jatp.get<JAT_granted_destin_identifier_list>().front().get<JG_qname>();
            const auto queueep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);
            if (!queueep) {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, qnm, jobid);
               DRETURN(-1);
            }

            INFO(MSG_JOB_SUSPTQ_UUS, jobid, task_number, qnm);

            if (!ISSET(jatp.get<JAT_state>(), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGSTOP, queueep, jep, jatp, monitor);
               state = jatp.get<JAT_state>();
               CLEARBIT(JRUNNING, state);
               jatp.set<JAT_state>(state);
            }
            state = jatp.get<JAT_state>();
            SETBIT(JSUSPENDED_ON_THRESHOLD, state);
            jatp.set<JAT_state>(state);

            {
               ocs::tom::ListPtr<ocs::tom::AN> answer_list;
               const char *session = jep.get<JB_session>();
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, jobid, task_number, nullptr, nullptr, session,
                               jep, jatp, nullptr, true, true, gdi_session);
               answer_list_output(&answer_list);
            }

            /* update queues time stamp in schedd */
            queueep.set<QU_last_suspend_threshold_ckeck>(now);
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD, gdi_session);
         }
      }
         break;

      case ORT_unsuspend_on_threshold: {
         uint32_t jobid;
         ocs::tom::Element<ocs::tom::JB> jep;
         const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
         const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

         DPRINTF("ORDER: ORT_unsuspend_on_threshold\n");

         jobid = ep.get<OR_job_number>();
         task_number = ep.get<OR_ja_task_number>();

         if (!(jep = master_job_list.find_first<JB_job_number>(jobid))
             || !(jatp = job_search_task(jep, nullptr, task_number))
             || !jatp.get<JAT_granted_destin_identifier_list>()) {
            /* don't panic - it is probably an exiting job */
            WARNING(MSG_JOB_UNSUSPOTNOTRUN_UU, jobid, task_number);
         } else {
            const char *qnm = jatp.get<JAT_granted_destin_identifier_list>().front().get<JG_qname>();
            const auto queueep = cqueue_list_locate_qinstance(master_cqueue_list, qnm);
            if (!queueep) {
               ERROR(MSG_JOB_UNABLE2FINDMQ_SU, qnm, jobid);
               DRETURN(-1);
            }

            INFO(MSG_JOB_UNSUSPOT_UUS, jobid, task_number, qnm);

            if (!ISSET(jatp.get<JAT_state>(), JSUSPENDED)) {
               sge_signal_queue(SGE_SIGCONT, queueep, jep, jatp, monitor);
               state = jatp.get<JAT_state>();
               SETBIT(JRUNNING, state);
               jatp.set<JAT_state>(state);
            }
            state = jatp.get<JAT_state>();
            CLEARBIT(JSUSPENDED_ON_THRESHOLD, state);
            jatp.set<JAT_state>(state);
            {
               ocs::tom::ListPtr<ocs::tom::AN> answer_list;
               const char *session = jep.get<JB_session>();
               sge_event_spool(&answer_list, 0, sgeE_JATASK_MOD, jobid, task_number, nullptr, nullptr, session,
                               jep, jatp, nullptr, true, true, gdi_session);
               answer_list_output(&answer_list);
            }
            /* update queues time stamp in schedd */
            queueep.set<QU_last_suspend_threshold_ckeck>(now);
            qinstance_add_event(queueep, sgeE_QINSTANCE_MOD, gdi_session);
         }
      }
         break;

      case ORT_job_schedd_info: {
         // OR_joker names no sub-type in the model; for this order it holds the scheduler messages
         const ocs::tom::List<ocs::tom::SME> sub_order_list(ep.get<OR_joker>());
         DPRINTF("ORDER: ORT_job_schedd_info\n");

         if (sub_order_list) {
            const auto sme = sub_order_list.front();

            if (sme) {
               DPRINTF("ORDER: got %d schedd infos\n",
                       static_cast<int>(sme.get<SME_message_list>().size()));

               // the master list slot may still hold no list, it is created here
               const auto info_list = ocs::DataStore::master_list_create<SGE_TYPE_JOB_SCHEDD_INFO>("schedd info");
               while (!info_list.empty()) {
                  info_list.erase(info_list.front());
               }
               info_list.push_back(sub_order_list.extract(sme));

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

/** @brief Send the scheduler's ticket decisions on to the execution hosts
 *
 * Tickets are what the priority translation facility on each host turns into
 * nice values, so an order that never arrives leaves that host scheduling on
 * stale entitlement.
 *
 * @param ticket_orders the orders to distribute, as sge_follow_order() collected them
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 *
 * @note MT-NOTE: distribute_ticket_orders() is NOT MT safe
 */
int distribute_ticket_orders(const ocs::tom::ConstList<ocs::tom::RTIC> ticket_orders, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   uint64_t now = sge_get_gmt64();
   unsigned long last_heard_from = 0;
   int cl_err = CL_RETVAL_OK;
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();

   for (const auto ep : ticket_orders) {
      ocs::tom::ConstList<ocs::tom::OR> to_send = ep.get<RTIC_tickets>();
      const char *host_name = ep.get<RTIC_host>();
      const ocs::tom::ConstElement<ocs::tom::EH> hep(host_list_locate(master_ehost_list, host_name));

      int n = static_cast<int>(to_send.size());

      if (hep) {
         cl_commlib_get_last_message_time((cl_com_get_handle(to_cstr(QMASTER), 0)),
                                          host_name, to_cstr(EXECD), 1, &last_heard_from);
      }
      if (hep &&sge_gmt32_to_gmt64(last_heard_from + 10 * mconf_get_load_report_time()) > now) {
         sge_pack_buffer pb;

         if (ocs::tom::try_init_buffer(&pb, sizeof(uint32_t) * 3 * n) == PACK_SUCCESS) {
            uint32_t dummyid = 0;
            for (const auto ep2 : to_send) {
               ocs::tom::try_put_uint32(&pb, ep2.get<OR_job_number>());
               ocs::tom::try_put_uint32(&pb, ep2.get<OR_ja_task_number>());
               ocs::tom::try_put_double(&pb, ep2.get<OR_ticket>());
            }
            cl_err = ocs::gdi::ClientServerBase::gdi_send_message_pb(0, to_cstr(EXECD), 1, host_name,
                                                                 ocs::gdi::ClientServerBase::TAG_CHANGE_TICKET, &pb, &dummyid);
            MONITOR_MESSAGES_OUT(monitor);
            ocs::tom::clear_buffer(&pb);
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
