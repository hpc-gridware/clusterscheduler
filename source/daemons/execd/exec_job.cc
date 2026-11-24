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
 * @brief Writing a job's configuration to disk and forking its shepherd
 */

#include <cfloat>
#include <cstring>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdlib>
#include <csignal>

#include <map>
#include <string>

#include "uti/ocs_Systemd.h"
#include "uti/sge_afsutil.h"
#include "uti/sge_arch.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "sgeobj/ocs_BindingInstance.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_grantedres.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_ulong.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "sgeobj/tom/ocs_tom_CE.h"

#include "comm/commlib.h"

#include "execd.h"
#include "get_path.h"
#include "sge_job_qmaster.h"
#include "tmpdir.h"
#include "exec_job.h"
#include "ocs_execd_systemd.h"

#include "execution_states.h"
#include "mail.h"
#include <cinttypes>
#include "pdc.h"
#include "job_report_execd.h"
#include "sge.h"
#include "symbols.h"
#include "msg_common.h"
#include "msg_execd.h"
#include "msg_daemons_common.h"
#include "ocs_Bootstrap.h"
#include "ocs_GrantedResources.h"
#include "ocs_GrantedResources.h"
#include "ocs_TopologyString.h"

#if defined(SOLARIS)
#   include "sge_smf.h"
#endif

/** @brief Name of the file the job's environment is written to */
#define ENVIRONMENT_FILE "environment"
/** @brief Name of the file the job's configuration is written to */
#define CONFIG_FILE "config"

static int ck_login_sh(char *shell);

static int get_nhosts(ocs::tom::ConstList<ocs::tom::JG> gdil_orig);


#if COMPILE_DC
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

/* local functions */
static int addgrpid_already_in_use(long add_grp_id);

static long get_next_addgrpid(ocs::tom::ConstList<ocs::tom::RN> rlp, long last_addgrpid);

#endif
#endif

/**
 * @brief The queue instance a job, task or PE task is accounted against, read only
 *
 * For a job or an array task it is the queue of the first entry of the array
 * task's granted destination identifier list; for a PE task it is the entry
 * of that list whose queue name is the PE task's queue, which is the
 * appropriate entry of the slave job's list.
 *
 * @param jep the job, unused
 * @param jatep the array task
 * @param petep the PE task, or an empty handle
 * @return the queue instance, or an empty handle
 */
ocs::tom::ConstElement<ocs::tom::QU>
responsible_queue([[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::JB> jep,
                  const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                  const ocs::tom::ConstElement<ocs::tom::PET> petep) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::JG> gdil_ep;
   if (!petep) {
      gdil_ep = jatep.get<JAT_granted_destin_identifier_list>().front();
   } else {
      const auto pe_queue = petep.get<PET_granted_destin_identifier_list>().front();
      gdil_ep = jatep.get<JAT_granted_destin_identifier_list>().find_first<JG_qname>(
         pe_queue.get<JG_qname>());
   }
   ocs::tom::ConstElement<ocs::tom::QU> master_q;
   if (gdil_ep) {
      master_q = gdil_ep.get_as<JG_queue, ocs::tom::QU>();
   }

   DRETURN(master_q);
}

/**
 * @brief The queue instance a job, task or PE task is accounted against, to change it
 *
 * As the read-only form; the queue instance lives in the array task's
 * granted destination identifier list, so a caller that changes it, such as
 * its used slots, passes a mutable array task.
 *
 * @param jep the job, unused
 * @param jatep the array task
 * @param petep the PE task, or an empty handle
 * @return the queue instance, or an empty handle
 */
ocs::tom::Element<ocs::tom::QU>
responsible_queue([[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::JB> jep,
                  const ocs::tom::Element<ocs::tom::JAT> jatep,
                  const ocs::tom::ConstElement<ocs::tom::PET> petep) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::JG> gdil_ep;
   if (!petep) {
      gdil_ep = jatep.get<JAT_granted_destin_identifier_list>().front();
   } else {
      const auto pe_queue = petep.get<PET_granted_destin_identifier_list>().front();
      gdil_ep = jatep.get<JAT_granted_destin_identifier_list>().find_first<JG_qname>(
         pe_queue.get<JG_qname>());
   }
   ocs::tom::Element<ocs::tom::QU> master_q;
   if (gdil_ep) {
      master_q = gdil_ep.get_as<JG_queue, ocs::tom::QU>();
   }

   DRETURN(master_q);
}

#if COMPILE_DC
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

/**
 * @brief The additional group id after the last one, taken from a range list
 *
 * @param rlp the ranges of the additional group ids, see gid_range
 * @param last_addgrpid the id used last, 0 if none has been used yet
 * @return the next id within the ranges; the first one of the first range if no id has been used yet
 *         or the last one was the end of the last range; 0 if the list has no range
 */
static long get_next_addgrpid(const ocs::tom::ConstList<ocs::tom::RN> rlp, long last_addgrpid) {
   DENTER(TOP_LAYER);

   int take_next = 0;

   /* uninitialized => return first number in list */
   if (last_addgrpid == 0) {
      const auto rep = rlp.front();
      if (rep) {
         DRETURN(rep.get<RN_min>());
      } else {
         DRETURN(0);
      }
   }

   /* search range and return next id */
   for (const auto rep : rlp) {
      long min, max;

      min = rep.get<RN_min>();
      max = rep.get<RN_max>();
      if (take_next) {
         DRETURN(min);
      } else if (last_addgrpid >= min && last_addgrpid < max) {
         DRETURN(++last_addgrpid);
      } else if (last_addgrpid == max) {
         take_next = 1;
      }
   }

   /* not successful until now => take first number */
   const auto rep = rlp.front();
   if (rep) {
      DRETURN(rep.get<RN_min>());
   }

   DRETURN(0);
}

/**
 * @brief Whether an array task or a PE task of the execd's job list uses an additional group id
 *
 * @param add_grp_id the additional group id
 * @return 1 if the id is in use, 0 otherwise
 */
static int addgrpid_already_in_use(const long add_grp_id) {
   DENTER(TOP_LAYER);

   for (const auto job : ocs::DataStore::master_list<SGE_TYPE_JOB>()) {
      for (const auto ja_task : job.get<JB_ja_tasks>()) {
         const char *id = ja_task.get<JAT_osjobid>();
         if (id != nullptr && atol(id) == add_grp_id) {
            DRETURN(1);
         }

         for (const auto pe_task : ja_task.get<JAT_task_list>()) {
            id = pe_task.get<PET_osjobid>();
            if (id != nullptr && atol(id) == add_grp_id) {
               DRETURN(1);
            }
         }
      }
   }
   DRETURN(0);
}

#endif
#endif

/**
 * @brief the granted resources of a job which apply on one host
 *
 * The granted resource list of a job holds the grants of all hosts it runs on, one entry per
 * resource and host, and qmaster sends the whole list to every execd. What the job sees on a
 * host - SGE_HGR_<name> and the devices it may access - has to come from that host's entries
 * only, otherwise a task on a slave host is handed the identifiers another host granted.
 *
 * Grants booked on the global host (GRU_host is SGE_GLOBAL_NAME), e.g. licence seats, belong to
 * the whole job and apply on every host. A resource can be configured both on the global host
 * and on some hosts, and then a job may hold it from the global host for one of its hosts and
 * from the host itself for another; on a host with a grant of its own, that one applies.
 *
 * @param granted_resources_list the granted resources of the job, JAT_granted_resources_list
 * @param host_name              the host the job is started on
 * @return the owning handle on a new list holding copies of the entries of granted_resources_list which
 *         apply on host_name, in list order - an empty handle when there are none
 */
static ocs::tom::ListPtr<ocs::tom::GRU>
exec_job_granted_resources_for_host(const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list,
                                    const char *host_name) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::GRU> ret;
   for (const auto gru : granted_resources_list) {
      const char *gru_host = gru.get<GRU_host>();
      bool applies = sge_hostcmp(gru_host, host_name) == 0;

      // a global grant applies, unless this host holds the same resource itself
      if (!applies && sge_hostcmp(gru_host, SGE_GLOBAL_NAME) == 0) {
         const char *name = gru.get<GRU_name>();
         applies = true;
         for (const auto other : granted_resources_list) {
            if (sge_strnullcmp(other.get<GRU_name>(), name) == 0 &&
                sge_hostcmp(other.get<GRU_host>(), host_name) == 0) {
               applies = false;
               break;
            }
         }
      }

      if (applies) {
         if (!ret) {
            ret = ocs::tom::ListPtr<ocs::tom::GRU>::create("host granted resources");
         }
         ret->push_back(gru.clone());
      }
   }

   DRETURN(ret);
}

/**
 * @brief Does a task get the sum of the limits of all its slots on the host?
 *
 * A task gets the limit of all slots of the job on the host if it runs the processes of the other
 * slots itself:
 * - the master task of a loosely integrated parallel job, which starts the processes of all slots;
 * - for a tightly integrated parallel job the master task with master_forks_slaves, and every pe
 *   task with daemon_forks_slaves - the one daemon per host which forks the slave tasks. With
 *   daemon_forks_slaves, the master host runs the job script and one such daemon; the job script
 *   gets the limit of all slots only with master_forks_slaves.
 * Every other task, and a sequential job, gets the limit of a single slot.
 *
 * A limit the job requests as a consumable of type JOB is the limit of the whole job, it is never
 * summed up.
 *
 * @param jep the job
 * @param jatep the ja_task
 * @param petep the pe task, an empty handle for the master task
 * @param limit_name the name of the limit, e.g. h_rss
 * @return true if the limits of all slots of the job on the host are summed up
 */
static bool
exec_job_task_limit_is_summed(const ocs::tom::ConstElement<ocs::tom::JB> jep,
                              const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                              const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *limit_name) {
   DENTER(TOP_LAYER);

   bool ret = false;
   const auto pe = jatep.get<JAT_pe_object>();
   if (pe && !job_is_requesting_consumable(jep, limit_name)) {
      if (!pe.get<PE_control_slaves>()) {
         // loosely integrated: the master task starts the processes of all slots
         ret = !petep;
      } else {
         // the master task forks the slaves on the master host, or the one pe task (daemon)
         // per host forks the slaves on its host
         if (!petep) {
            ret = pe.get<PE_master_forks_slaves>();
         } else {
            ret = pe.get<PE_daemon_forks_slaves>();
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Parse a limit of a queue instance, warning about a value that cannot be parsed
 *
 * The caller treats a limit that cannot be parsed as INFINITY (CS-3175); the warning names the
 * queue instance and the value, so a broken limit does not stay silent.
 *
 * @param queue the queue instance the limit belongs to
 * @param limit_name the name of the limit, e.g. h_rt
 * @param type the type of the limit, e.g. ocs::CEntry::Type::TIME
 * @param limit_str the text of the limit, not nullptr and not INFINITY
 * @param[out] dvalp receives the value as a double, nullptr if not wanted
 * @param[out] uvalp receives the value as a 32 bit unsigned, nullptr if not wanted
 * @return true if the limit was parsed, false (after logging a warning) if it cannot be parsed
 */
bool
ocs_execd_parse_queue_limit(const ocs::tom::ConstElement<ocs::tom::QU> queue, const char *limit_name,
                            const ocs::CEntry::Type type, const char *limit_str, double *dvalp, uint32_t *uvalp) {
   DENTER(TOP_LAYER);

   if (parse_ulong_val(dvalp, uvalp, type, limit_str, nullptr, 0) == 0) {
      WARNING(MSG_EXECD_INVALIDQUEUELIMIT_SSS, limit_name, queue.get<QU_full_name>(), limit_str);
      DRETURN(false);
   }

   DRETURN(true);
}

/**
 * @brief Sum up a limit over all slots of a job on a host
 *
 * A limit of a queue instance that cannot be parsed makes the sum infinity.
 *
 * @tparam F the limit field of the queue instance, e.g. QU_h_rss
 * @param limit_name the name of the limit, e.g. h_rss
 * @param type the type of the limit, e.g. ocs::CEntry::Type::MEM
 * @param jatep the ja_task
 * @param qualified_hostname the host
 * @return the sum of the limit of every queue instance of the job on the host multiplied by its
 *         slots, DBL_MAX (infinity) if one of the queue instances has no limit
 */
template <auto F>
static double
exec_job_sum_host_limit(const char *limit_name, const ocs::CEntry::Type type,
                        const ocs::tom::ConstElement<ocs::tom::JAT> jatep, const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   double limit = 0;
   const auto gdil = jatep.get<JAT_granted_destin_identifier_list>();
   for (const auto gdil_ep : gdil.find_all<JG_qhostname>(ocs::tom::Host(qualified_hostname))) {
      const ocs::tom::ConstElement<ocs::tom::QU> queue = gdil_ep.get_as<JG_queue, ocs::tom::QU>();
      if (queue) {
         const char *limit_str = queue.get<F>();
         if (limit_str != nullptr) {
            // if one of the queue instances has a limit of infinity the sum is infinity,
            // CS-3175: so is a limit that cannot be parsed
            double dbl = 0.0;
            if (strcasecmp(limit_str, INFINITY_STR) == 0 ||
                !ocs_execd_parse_queue_limit(queue, limit_name, type, limit_str, &dbl, nullptr)) {
               DPRINTF("exec_job_sum_host_limit: qinstance %s has infinity\n",
                       gdil_ep.get<JG_qname>());
               limit = DBL_MAX;
               break;
            }
            uint32_t slots = gdil_ep.get<JG_slots>();
            limit += dbl * slots;
            DPRINTF("exec_job_sum_host_limit: qinstance %s has limit %s, slots " sge_u32
                    ", sum %f\n",
                    gdil_ep.get<JG_qname>(), limit_str, slots, dbl * slots);
         }
      }
   }

   DRETURN(limit);
}

/**
 * @brief Get the limit of a task of a job
 *
 * This is the limit sge_shepherd applies to the task, see sge_exec_job_get_limit(). sge_execd
 * checks the usage of the task against the same value.
 *
 * @tparam F the limit field of the queue instance, e.g. QU_h_rss
 * @param limit_name the name of the limit, e.g. h_rss
 * @param type the type of the limit, e.g. ocs::CEntry::Type::MEM
 * @param queue the queue instance the task is running in
 * @param jep the job
 * @param jatep the ja_task
 * @param petep the pe task, an empty handle for the master task
 * @param qualified_hostname the host the task is running on
 * @return the limit, DBL_MAX (infinity) if the task has no limit
 */
template <auto F>
double
execd_get_task_limit(const char *limit_name, const ocs::CEntry::Type type,
                     const ocs::tom::ConstElement<ocs::tom::QU> queue, const ocs::tom::ConstElement<ocs::tom::JB> jep,
                     const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                     const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   double limit = DBL_MAX;
   const char *limit_str = queue.get<F>();
   if (limit_str != nullptr && strcasecmp(limit_str, INFINITY_STR) != 0) {
      if (exec_job_task_limit_is_summed(jep, jatep, petep, limit_name)) {
         limit = exec_job_sum_host_limit<F>(limit_name, type, jatep, qualified_hostname);
      } else if (!ocs_execd_parse_queue_limit(queue, limit_name, type, limit_str, &limit, nullptr)) {
         // CS-3175: a limit that cannot be parsed is treated as no limit
         limit = DBL_MAX;
      }
   }

   DRETURN(limit);
}

// the limits sge_execd checks itself, see force_job_rlimit_task() in execd_ck_to_do.cc
template double
execd_get_task_limit<QU_h_cpu>(const char *, ocs::CEntry::Type, ocs::tom::ConstElement<ocs::tom::QU>,
                               ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstElement<ocs::tom::JAT>,
                               ocs::tom::ConstElement<ocs::tom::PET>, const char *);
template double
execd_get_task_limit<QU_h_rss>(const char *, ocs::CEntry::Type, ocs::tom::ConstElement<ocs::tom::QU>,
                               ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstElement<ocs::tom::JAT>,
                               ocs::tom::ConstElement<ocs::tom::PET>, const char *);
template double
execd_get_task_limit<QU_h_vmem>(const char *, ocs::CEntry::Type, ocs::tom::ConstElement<ocs::tom::QU>,
                                ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstElement<ocs::tom::JAT>,
                                ocs::tom::ConstElement<ocs::tom::PET>, const char *);
template double
execd_get_task_limit<QU_s_cpu>(const char *, ocs::CEntry::Type, ocs::tom::ConstElement<ocs::tom::QU>,
                               ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstElement<ocs::tom::JAT>,
                               ocs::tom::ConstElement<ocs::tom::PET>, const char *);
template double
execd_get_task_limit<QU_s_rss>(const char *, ocs::CEntry::Type, ocs::tom::ConstElement<ocs::tom::QU>,
                               ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstElement<ocs::tom::JAT>,
                               ocs::tom::ConstElement<ocs::tom::PET>, const char *);
template double
execd_get_task_limit<QU_s_vmem>(const char *, ocs::CEntry::Type, ocs::tom::ConstElement<ocs::tom::QU>,
                                ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstElement<ocs::tom::JAT>,
                                ocs::tom::ConstElement<ocs::tom::PET>, const char *);

/**
 * @brief Get the limit of a task as it is written into the config file of sge_shepherd
 *
 * For sequential jobs and for the tasks of a parallel job which get the limit of a single slot,
 * this is the limit of the queue instance as it is configured. For the tasks which get the limit
 * of all slots of the job on the host, see exec_job_task_limit_is_summed(), it is the sum.
 *
 * @tparam F the limit field of the queue instance, e.g. QU_h_rss
 * @param dstr buffer for a summed up limit
 * @param limit_name the name of the limit, e.g. h_rss
 * @param type the type of the limit, e.g. ocs::CEntry::Type::MEM
 * @param master_q the queue instance the task is running in
 * @param jep the job
 * @param jatep the ja_task
 * @param petep the pe task, an empty handle for the master task
 * @param qualified_hostname the host the task is running on
 * @return the limit as a string
 */
template <auto F>
static const char *
sge_exec_job_get_limit(dstring *dstr, const char *limit_name, const ocs::CEntry::Type type,
                       const ocs::tom::ConstElement<ocs::tom::QU> master_q,
                       const ocs::tom::ConstElement<ocs::tom::JB> jep,
                       const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                       const ocs::tom::ConstElement<ocs::tom::PET> petep, const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   const char *ret = master_q.get<F>();
   // can not really happen but just to be sure
   if (ret == nullptr) {
      ret = INFINITY_STR;
   }

   DPRINTF("sge_exec_job_get_limit: limit_name=%s, limit=%s\n", limit_name, ret);

   // if the limit in the master queue is infinity there is no need to do calculations
   if (strcasecmp(ret, INFINITY_STR) != 0 &&
       exec_job_task_limit_is_summed(jep, jatep, petep, limit_name)) {
      DPRINTF("sge_exec_job_get_limit: we need to sum up the limits\n");
      double limit = exec_job_sum_host_limit<F>(limit_name, type, jatep, qualified_hostname);
      if (limit == DBL_MAX) {
         ret = INFINITY_STR;
      } else {
         // double_print_to_dstring() appends, and dstr is used for all limits of the task
         sge_dstring_clear(dstr);
         double_print_to_dstring(limit, dstr, type);
         ret = sge_dstring_get_string(dstr);
      }
      DPRINTF("sge_exec_job_get_limit: sum of limits %s\n", ret);
   }

   DRETURN(ret);
}

/** @def WRITE_COMPLEX_AND_CONSUMABLE_ATTR
 * @brief Write one resource limit to the shepherd config, and whether it is a job consumable
 *
 * Defined inside sge_exec_job() and used only there; relies on the local `fp`,
 * `master_q`, `jatep`, `petep` and `qualified_hostname`.
 *
 * @param A the attribute's name, used both as text and as a `QU_` field suffix
 * @param T the attribute's type
 */

/** @def WRITE_COMPLEX_ATTR
 * @brief #WRITE_COMPLEX_AND_CONSUMABLE_ATTR without the consumable flag
 * @param A the attribute's name
 * @param T the attribute's type
 */

/** @brief Write the job's configuration to disk and fork its shepherd
 *
 * Everything the shepherd will need - the `config` file, the `environment`
 * file, the job script - is written into the job's active_jobs directory
 * first, because once the shepherd is forked the daemon can no longer tell it
 * anything.
 *
 * The forked child execs the shepherd and never returns; if the exec fails, it writes the
 * "error" and "exit_status" files the way the shepherd does and exits.
 *
 * @param jep the job
 * @param jatep the array task
 * @param petep the PE task, or an empty handle
 * @param[out] err_str receives the reason on failure
 * @param err_length size of that buffer
 * @return the pid of the shepherd (in the parent) on success; on failure -1 (error), -2 (general
 *         error, halt the queue) or -3 (general error, halt the job), with err_str set to the reason
 */
int sge_exec_job(const ocs::tom::Element<ocs::tom::JB> jep, const ocs::tom::Element<ocs::tom::JAT> jatep,
                 const ocs::tom::Element<ocs::tom::PET> petep, char *err_str, const int err_length) {
   DENTER(TOP_LAYER);

   int i;
   char ps_name[128];
   FILE *fp;
   uint32_t interval;
   struct passwd *pw;
   SGE_STRUCT_STAT buf;
   int used_slots, pe_slots = 0, host_slots = 0, nhosts = 0;
   // process-lifetime object, freed and refilled for every master task
   static ocs::tom::ListPtr<ocs::tom::RN> processor_set;
   const char *cp;
   char *shell;
   const char *cwd = nullptr;
   char cwd_out_buffer[SGE_PATH_MAX];
   dstring cwd_out;

   ocs::tom::ConstList<ocs::tom::PA> path_aliases = nullptr;
   bool starting_shepherd_ok = true;

#if COMPILE_DC
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
   static gid_t last_addgrpid;
#endif
#endif

   dstring active_dir;

   char shepherd_path[SGE_PATH_MAX] = "",
           coshepherd_path[SGE_PATH_MAX] = "",
           tmpdir[SGE_PATH_MAX] = "",
           active_dir_buffer[SGE_PATH_MAX] = "",
           shell_path[SGE_PATH_MAX] = "",
           stdout_path[SGE_PATH_MAX] = "",
           stderr_path[SGE_PATH_MAX] = "",
           stdin_path[SGE_PATH_MAX] = "",
           pe_stdout_path[SGE_PATH_MAX] = "",
           pe_stderr_path[SGE_PATH_MAX] = "",
           fs_stdin_host[SGE_PATH_MAX] = "\"\"",
           fs_stdin_path[SGE_PATH_MAX] = "",
           fs_stdout_host[SGE_PATH_MAX] = "\"\"",
           fs_stdout_path[SGE_PATH_MAX] = "",
           fs_stderr_host[SGE_PATH_MAX] = "\"\"",
           fs_stderr_path[SGE_PATH_MAX] = "";

   DSTRING_STATIC(dstr_fname, SGE_PATH_MAX);
   const char *str_fname;
   DSTRING_STATIC(dstr_script_file, SGE_PATH_MAX);
   // @todo initialization shouldn't be necessary, but compiler seems to find a path in which str_script_file is
   //       not initialized!
   const char *str_script_file = "";

   char mail_str[1024];
   const char *shepherd_name;
   ocs::tom::ConstList<ocs::tom::JG> gdil;
   ocs::tom::Element<ocs::tom::QU> master_q;
   ocs::tom::ConstElement<ocs::tom::CK> ep;
   ocs::tom::ConstElement<ocs::tom::VA> env;
   ocs::tom::ListPtr<ocs::tom::VA> environmentList;
   const char *arch = sge_get_arch();
   const char *sge_root = bootstrap_get_sge_root();
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *default_cell = bootstrap_get_sge_cell();
   const char *binary_path = ocs::Bootstrap::get_binary_path();
   const char *admin_user = ocs::Bootstrap::get_admin_user();
   const char *masterhost = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
   bool tls_mode = false;
   sigset_t sigset, sigset_oset;
   struct passwd pw_struct;
   char *pw_buffer;
   size_t pw_buffer_size;

   int write_osjob_id = 1;

   const char *fs_stdin_file = "", *fs_stdout_file = "", *fs_stderr_file = "";

   bool bInputFileStaging, bOutputFileStaging, bErrorFileStaging;

   /* env var reflecting SGE_BINDING if set */
   char *sge_binding_environment = nullptr;
   /* string reflecting the logical socket,core pairs if "pe" is set */
   dstring core_binding_strategy_string = DSTRING_INIT;

   /* retrieve the job, jatask and petask id once */
   uint32_t job_id;
   uint32_t ja_task_id;
   const char *pe_task_id = nullptr;

   char *shell_start_mode = nullptr;
   char *pag_cmd = nullptr;
   char *notify_kill = nullptr;
   char *notify_susp = nullptr;
   char *shepherd_cmd = nullptr;
   char *set_token_cmd = nullptr;

   sge_dstring_init(&cwd_out, cwd_out_buffer, sizeof(cwd_out_buffer));
   sge_dstring_init(&active_dir, active_dir_buffer, sizeof(active_dir_buffer));

   SGE_ASSERT((jep));
   SGE_ASSERT((jatep));

   job_id = jep.get<JB_job_number>();


   ja_task_id = jatep.get<JAT_task_number>();
   gdil = jatep.get<JAT_granted_destin_identifier_list>();

   if (petep) {
      pe_task_id = petep.get<PET_id>();
   }

   DPRINTF("job: " sge_u32 " jatask: " sge_u32 " petask: %s\n", job_id, ja_task_id,
           pe_task_id != nullptr ? pe_task_id : "none");

   master_q = responsible_queue(jep, jatep, petep);
   SGE_ASSERT((master_q));

   pw_buffer_size = get_pw_buffer_size();
   pw_buffer = sge_malloc(pw_buffer_size);
   pw = sge_getpwnam_r(jep.get<JB_owner>(), &pw_struct, pw_buffer, pw_buffer_size);
   if (!pw) {
      snprintf(err_str, err_length, MSG_SYSTEM_GETPWNAMFAILED_S, jep.get<JB_owner>());
      sge_free(&pw_buffer);
      DRETURN(-3); /* error only relevant for this user */
   }

   sge_get_active_job_file_path(&active_dir, job_id, ja_task_id, pe_task_id, nullptr);

   umask(022);

   /* make tmpdir only when this is the first task that gets started
      in this queue instance. QU_job_slots_used holds actual number of used
      slots for this job in the queue */
   if (!(used_slots = qinstance_slots_used(master_q))) {
      if (!(sge_make_tmpdir(master_q, job_id, ja_task_id,
                            pw->pw_uid, pw->pw_gid, tmpdir, SGE_PATH_MAX))) {
         snprintf(err_str, err_length, SFNMAX, MSG_SYSTEM_CANTMAKETMPDIR);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }
   } else {
      SGE_STRUCT_STAT statbuf;
      if (!(sge_get_tmpdir(master_q, job_id, ja_task_id, tmpdir, SGE_PATH_MAX))) {
         snprintf(err_str, err_length, SFNMAX, MSG_SYSTEM_CANTGETTMPDIR);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }

      if (SGE_STAT(tmpdir, &statbuf)) {
         snprintf(err_str, err_length, MSG_SYSTEM_CANTOPENTMPDIR_S, tmpdir);
         sge_free(&pw_buffer);
         DRETURN(-2);
      }
   }

   /* increment used slots */
   DPRINTF("%s: used slots increased from %d to %d\n", master_q.get<QU_full_name>(), used_slots,
           used_slots + 1);
   qinstance_set_slots_used(master_q, used_slots + 1);

   nhosts = get_nhosts(gdil);
   pe_slots = 0;
   for (ocs::tom::ConstElement<ocs::tom::JG> gdil_ep : gdil) {
      pe_slots += (int) gdil_ep.get<JG_slots>();
   }

   // Calculate a combined binding_to_use for all tasks of a job on that host
   bool binding_to_use_is_initialized = false;
#if defined(OCS_HWLOC)
   ocs::BindingInstance::Instance binding_instance = ocs::Job::binding_get_instance(jep);
   ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list = jatep.get<JAT_granted_resources_list>();
   ocs::TopologyString binding_to_use;
   ocs::GrantedResources::get_combined_binding_for_host(granted_resources_list, qualified_hostname, binding_to_use);
   binding_to_use_is_initialized = true;
#endif

   // Prepare binding mask containing the PU's using the binding_to_use string
   bool binding_cpuset_is_initialized = false;
#if defined(OCS_HWLOC)
   hwloc_bitmap_t cpuset = nullptr;
   cpuset = hwloc_bitmap_alloc();
   if (binding_to_use_is_initialized) {
      ocs::Topo::make_cpuset(cpuset, binding_to_use.to_string(true, false, false, false, false, false, true));
      binding_cpuset_is_initialized = true;
   }
#endif

   /***************** write out sge host file ******************************/
   /* JG: TODO: create function write_pe_hostfile() */
   /* JG: TODO (254) use function sge_get_active_job.... */
   if (petep == nullptr) {
      DSTRING_STATIC(dstr_hostfilename, SGE_PATH_MAX);
      const char *str_hostfilename;
      processor_set.reset();

      str_hostfilename = sge_dstring_sprintf(&dstr_hostfilename, "%s/%s/%s", execd_spool_dir, active_dir_buffer,
                                             PE_HOSTFILE);
      fp = fopen(str_hostfilename, "w");
      if (!fp) {
         snprintf(err_str, err_length, MSG_FILE_NOOPEN_SS, str_hostfilename, strerror(errno));
         sge_free(&pw_buffer);
         DRETURN(-2);
      }

      /*
         Get number of hosts 'nhosts' where the user got queues on

         The granted_destination_identifier_list holds
         on entry for each queue, not host. But each
         entry also contais the hosts name where the
         queue resides on.

         We need to combine the processor sets of all queues on this host.
         They need to get passed to shepherd
      */

      // If we have a binding_to_use then prepare a string for the rankfile
      std::string ids_str;
#if defined(OCS_HWLOC)
      if (binding_to_use_is_initialized && binding_instance == ocs::BindingInstance::PE) {
         auto ids = binding_to_use.get_socket_and_cores_or_thread_tuples(true);
         ids_str = ocs::TopologyString::id_tuple2string(ids);
      }
#endif

      host_slots = 0;
      for (ocs::tom::ConstElement<ocs::tom::JG> gdil_ep : gdil) {
         // raw out parameter of range_list_parse_from_string()
         ocs::tom::ListPtr<ocs::tom::AN> alp;

         // get processor set configuration (used when available and thread binding is not used
         const char *q_set = gdil_ep.get<JG_processors>();

         // get all other values for the rankfile
         int slots = static_cast<int>(gdil_ep.get<JG_slots>());
         const char *hostname = gdil_ep.get<JG_qhostname>();
         const char *qname = gdil_ep.get<JG_qname>();

         fprintf(fp, "%s %d %s %s\n", hostname, slots, qname,
                 ids_str.empty() ? (q_set ? q_set : "<nullptr>") : ids_str.c_str());

         if (!sge_hostcmp(master_q.get<QU_qhostname>(), gdil_ep.get<JG_qhostname>())) {
            host_slots += slots;
            if (q_set && strcasecmp(q_set, "UNDEFINED")) {
               range_list_parse_from_string(processor_set, &alp, q_set, false, false, INF_ALLOWED);
               /* TODO: should we not print the answerlist in case of an error? */
               alp.reset();
            }
         }
      }

      FCLOSE(fp);
   }
   /*************************** finished writing sge hostfile  ********/

   /********************** setup environment file ***************************/
   str_fname = sge_dstring_sprintf(&dstr_fname, "%s/%s/environment", execd_spool_dir, active_dir_buffer);
   fp = fopen(str_fname, "w");
   if (!fp) {
      snprintf(err_str, err_length, MSG_FILE_NOOPEN_SS, str_fname, strerror(errno));
      sge_free(&pw_buffer);
      DRETURN(-2);
   }


   /* write environment of job */
   var_list_copy_env_vars_and_value(environmentList, jep.get<JB_env_list>());

   /* write environment of petask */
   if (petep != nullptr) {
      var_list_copy_env_vars_and_value(environmentList,
                                       petep.get<PET_environment>());
   }

   {
      ocs::tom::ConstElement<ocs::tom::VA> user_path;
      dstring buffer = DSTRING_INIT;

      if ((user_path = environmentList->find_first<VA_variable>("PATH"))) {
         sge_dstring_sprintf(&buffer, "%s:%s", tmpdir, user_path.get<VA_value>());
      } else {
         sge_dstring_sprintf(&buffer, "%s:%s", tmpdir, SGE_DEFAULT_PATH);
      }
      var_list_set_string(environmentList, "PATH", sge_dstring_get_string(&buffer));
      sge_dstring_free(&buffer);
   }

   /* 1.) try to read cwd from pe task */
   if (petep != nullptr) {
      cwd = petep.get<PET_cwd>();
      path_aliases = petep.get<PET_path_aliases>();
   }

   /* 2.) try to read cwd from job */
   if (cwd == nullptr) {
      cwd = jep.get<JB_cwd>();
      path_aliases = jep.get<JB_path_aliases>();
   }

   /* 3.) if pe task or job set cwd: do path mapping */
   if (cwd != nullptr) {
      /* path aliasing only for cwd flag set */
      path_alias_list_get_path(path_aliases, nullptr,
                               cwd, qualified_hostname,
                               &cwd_out);
      cwd = sge_dstring_get_string(&cwd_out);
      var_list_set_string(environmentList, "PWD", cwd);
   } else {
      /* 4.) if cwd not set in job: take users home dir
       *     copy the string as we want to free the pw buffer later
       */
      cwd = sge_dstring_copy_string(&cwd_out, pw->pw_dir);
   }

   {
      const char *reqname = petep == nullptr ? jep.get<JB_job_name>() : petep.get<PET_name>();
      if (reqname != nullptr) {
         var_list_set_string(environmentList, "REQNAME", reqname);
      }
   }

   var_list_set_string(environmentList, VAR_PREFIX "CELL", default_cell);

   var_list_set_string(environmentList, "HOME", pw->pw_dir);
   var_list_set_string(environmentList, "SHELL", pw->pw_shell);
   var_list_set_string(environmentList, "USER", pw->pw_name);
   var_list_set_string(environmentList, "LOGNAME", pw->pw_name);

   /* do not access pw->* from here on! */
   sge_free(&pw_buffer);

   if (sge_binding_environment != nullptr) {
      var_list_set_string(environmentList, "SGE_BINDING", sge_binding_environment);
      sge_free(&sge_binding_environment);
   }

   // SGE_HGR_* is set by execd alone. A job may have brought its own, from qsub -v or from the
   // environment of the master task which a qrsh -inherit -V passes on, and would keep it where
   // it was granted no resource of that name. The __SGE_PREFIX__ forms are turned into SGE_*
   // further down, the first one even replacing what execd sets here.
   var_list_remove_prefix_vars(*environmentList, "SGE_HGR_");
   var_list_remove_prefix_vars(*environmentList, VAR_PREFIX "HGR_");
   var_list_remove_prefix_vars(*environmentList, VAR_PREFIX_NR "HGR_");

   /* new RSMAP resource map consumable feature */
   {
      // this host's grants and those of the global host - not the whole list, which holds the
      // grants of every host of the job
      const auto host_grants = exec_job_granted_resources_for_host(
         jatep.get<JAT_granted_resources_list>(), qualified_hostname);

      /* now setting the granted resources list */
      for (const auto gr : *host_grants) {
         auto gru_type = static_cast<ocs::GrantedResources::Type>(gr.get<GRU_type>());
         if (gru_type == ocs::GrantedResources::Type::GRU_HARD_REQUEST_TYPE
             || gru_type == ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {

            /* if the type is a hard resource request add it to the string */
            std::string str_hgr{"SGE_HGR_"};
            str_hgr += gr.get<GRU_name>();

            std::string id_buffer;
            if (gru_type == ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
               granted_res_get_id_string(id_buffer, gr.get<GRU_resource_map_list>());
            } else {
               // @todo use function like double_print_*_to_dstring for formatting
               id_buffer = std::to_string(gr.get<GRU_amount>());
            }

            var_list_set_string(environmentList,
                                str_hgr.c_str(),
                                id_buffer.c_str());
         }
      }
   }

   /*
    * Handling of script_file and JOB_NAME:
    * script_file: For batch jobs, it is the path to the spooled
    *              script file, for interactive jobs, a fixed string
    *              (macro), e.g. JOB_TYPE_STR_QSH
    * JOB_NAME:    If a name is passed in JB_job_name or PET_name, i
    *              this name is used.
    *              Otherwise basename(script_file) is used.
    */
   {
      uint32_t jb_now;
      const char *job_name;

      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
         job_name = petep.get<PET_name>();
      } else {
         jb_now = jep.get<JB_type>();
         job_name = jep.get<JB_job_name>();
      }

      /* set script_file */
      JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      if (jb_now & JOB_TYPE_QSH) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QSH);
      } else if (jb_now & JOB_TYPE_QLOGIN) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QLOGIN);
      } else if (jb_now & JOB_TYPE_QRSH) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QRSH);
      } else if (jb_now & JOB_TYPE_QRLOGIN) {
         str_script_file = sge_dstring_copy_string(&dstr_script_file, JOB_TYPE_STR_QRLOGIN);
      } else if (jb_now & JOB_TYPE_BINARY) {
         const char *sfile;

         sfile = jep.get<JB_script_file>();
         if (sfile != nullptr) {
            DSTRING_STATIC(dstr_script_file_out, SGE_PATH_MAX);
            path_alias_list_get_path(jep.get<JB_path_aliases>(), nullptr,
                                     sfile, qualified_hostname,
                                     &dstr_script_file_out);
            str_script_file = sge_dstring_copy_dstring(&dstr_script_file, &dstr_script_file_out);
         }
      } else {
         if (jep.get<JB_script_file>() != nullptr) {
            /* JG: TODO: use some function to create path */
            str_script_file = sge_dstring_sprintf(&dstr_script_file, "%s/%s/" sge_u32, execd_spool_dir, EXEC_DIR,
                                                  job_id);
         } else {
            /*
             * This is an error that will be handled in shepherd.
             * When we implement binary submission (Issue #25), this case
             * might become valid and the binary to execute might be the
             * first argument to execute.
             */
            str_script_file = sge_dstring_copy_string(&dstr_script_file, "none");
         }
      }

      /* set JOB_NAME */
      if (job_name == nullptr) {
         job_name = sge_basename(str_script_file, PATH_SEPARATOR_CHAR);
      }

      var_list_set_string(environmentList, "JOB_NAME", job_name);
   }

   {
      uint32_t type = jep.get<JB_type>();
      const char *var_name = "QRSH_COMMAND";

      if (!JOB_TYPE_IS_BINARY(type) && petep == nullptr) {
         const char *old_qrsh_command_s = nullptr;
         dstring old_qrsh_command = DSTRING_INIT;

         old_qrsh_command_s = var_list_get_string(*environmentList, var_name);
         if (old_qrsh_command_s != nullptr) {
            char delim[2];
            const char *buffer;
            const char *token;
            int is_first_token = 1;
            dstring new_qrsh_command = DSTRING_INIT;

            snprintf(delim, sizeof(delim), "%c", 0xff);
            sge_dstring_copy_string(&old_qrsh_command, old_qrsh_command_s);
            buffer = sge_dstring_get_string(&old_qrsh_command);
            token = sge_strtok(buffer, delim);
            while (token != nullptr) {
               if (is_first_token) {
                  sge_dstring_sprintf(&new_qrsh_command, "%s/%s/" sge_u32,
                                      execd_spool_dir, EXEC_DIR, job_id);
                  is_first_token = 0;
               } else {
                  sge_dstring_append(&new_qrsh_command, delim);
                  sge_dstring_append(&new_qrsh_command, token);
               }
               token = sge_strtok(nullptr, delim);
            }
            var_list_set_string(environmentList, var_name, sge_dstring_get_string(&new_qrsh_command));
            sge_dstring_free(&new_qrsh_command);
         }
         sge_dstring_free(&old_qrsh_command);
      } else {
         const char *sfile;

         sfile = var_list_get_string(*environmentList, var_name);
         if (sfile != nullptr) {
            dstring dstr_script_file_out = DSTRING_INIT;

            path_alias_list_get_path(jep.get<JB_path_aliases>(), nullptr,
                                     sfile, qualified_hostname,
                                     &dstr_script_file_out);
            var_list_set_string(environmentList, var_name, sge_dstring_get_string(&dstr_script_file_out));
            sge_dstring_free(&dstr_script_file_out);
         }
      }
   }

   var_list_set_string(environmentList, "JOB_SCRIPT", str_script_file);
   str_fname = sge_dstring_sprintf(&dstr_fname, "%s/%s", binary_path, arch);
   var_list_set_string(environmentList, "SGE_BINARY_PATH", str_fname);

   /* JG: TODO (ENV): do we need REQNAME and REQUEST? */
   var_list_set_string(environmentList, "REQUEST",
                       petep == nullptr ? jep.get<JB_job_name>() : petep.get<PET_name>());
   var_list_set_string(environmentList, "HOSTNAME", master_q.get<QU_qhostname>());
   var_list_set_string(environmentList, "QUEUE", master_q.get<QU_qname>());

   var_list_set_uint32t(environmentList, "JOB_ID", job_id);

   if (job_is_array(jep)) {
      const auto [start, end, step] = job_get_submit_task_ids(jep);

      var_list_set_uint32t(environmentList, VAR_PREFIX "TASK_ID", ja_task_id);
      var_list_set_uint32t(environmentList, VAR_PREFIX "TASK_FIRST", start);
      var_list_set_uint32t(environmentList, VAR_PREFIX "TASK_LAST", end);
      var_list_set_uint32t(environmentList, VAR_PREFIX "TASK_STEPSIZE", step);
   } else {
      const char *udef = "undefined";

      var_list_set_string(environmentList, VAR_PREFIX "TASK_ID", udef);
      var_list_set_string(environmentList, VAR_PREFIX "TASK_FIRST", udef);
      var_list_set_string(environmentList, VAR_PREFIX "TASK_LAST", udef);
      var_list_set_string(environmentList, VAR_PREFIX "TASK_STEPSIZE", udef);
   }

   if (pe_task_id != nullptr) {
      var_list_set_string(environmentList, VAR_PREFIX "PE_TASK_ID", pe_task_id);
   }

   var_list_set_string(environmentList, "ENVIRONMENT", "BATCH");
   var_list_set_string(environmentList, "ARC", arch);

   var_list_set_string(environmentList, VAR_PREFIX "ARCH", arch);

   if ((cp = getenv("TZ")) && strlen(cp))
      var_list_set_string(environmentList, "TZ", cp);

   if ((cp = getenv("SGE_QMASTER_PORT")) && strlen(cp))
      var_list_set_string(environmentList, "SGE_QMASTER_PORT", cp);

   if ((cp = getenv("SGE_EXECD_PORT")) && strlen(cp))
      var_list_set_string(environmentList, "SGE_EXECD_PORT", cp);

   var_list_set_string(environmentList, VAR_PREFIX "ROOT", sge_root);

   var_list_set_int(environmentList, "NQUEUES",
                    static_cast<int>(gdil.size()));
   var_list_set_int(environmentList, "NSLOTS", pe_slots);
   var_list_set_int(environmentList, "NHOSTS", nhosts);

   var_list_set_int(environmentList, "RESTARTED", (int) jatep.get<JAT_job_restarted>());

   var_list_set_string(environmentList, "TMPDIR", tmpdir);
   var_list_set_string(environmentList, "TMP", tmpdir);

   var_list_set_string(environmentList, VAR_PREFIX "ACCOUNT", (jep.get<JB_account>() ?
                                                                jep.get<JB_account>() : DEFAULT_ACCOUNT));

   // binding specific environment
#if defined(OCS_HWLOC)
   if (binding_to_use_is_initialized) {
      var_list_set_string(environmentList, "SGE_BINDING_INSTANCE",
                          ocs::BindingInstance::to_string(binding_instance).c_str());
      var_list_set_string(environmentList, "SGE_BINDING_TOPOLOGY",
         binding_to_use.is_empty() ? NONE_STR : binding_to_use.to_product_topology_string().c_str());
   }
   if (binding_cpuset_is_initialized) {
      char *cpuset_str = NULL;
      hwloc_bitmap_asprintf(&cpuset_str, cpuset);
      var_list_set_string(environmentList, "SGE_BINDING_CPUSET", cpuset_str);
      sge_free(&cpuset_str);
   }
#endif

   sge_get_path(qualified_hostname, jep.get<JB_shell_list>(), cwd,
                jep.get<JB_owner>(),
                petep == nullptr ? jep.get<JB_job_name>() : petep.get<PET_name>(),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_SHELL, shell_path, SGE_PATH_MAX);

   if (shell_path[0] == 0) {
      strcpy(shell_path, master_q.get<QU_shell>());
   }
   var_list_set_string(environmentList, "SHELL", shell_path);

   /* forward name of pe to job */
   if (jatep.get<JAT_granted_pe>() != nullptr) {
      DSTRING_STATIC(dstr_buffer, SGE_PATH_MAX);
      const char *str_buffer;
      ocs::tom::ConstElement<ocs::tom::PE> pe;

      var_list_set_string(environmentList, "PE", jatep.get<JAT_granted_pe>());
      /* forward PE_HOSTFILE only to master task */
      if (petep == nullptr) {
         str_buffer = sge_dstring_sprintf(&dstr_buffer, "%s/%s/%s", execd_spool_dir, active_dir_buffer, PE_HOSTFILE);
         var_list_set_string(environmentList, "PE_HOSTFILE", str_buffer);
      }
      // for tightly integrated parallel jobs, also set the rsh_command SGE_RSH_COMMAND
      // it will be read by qrsh -inherit in the job script
      pe = jatep.get<JAT_pe_object>();
      if (pe != nullptr && pe.get<PE_control_slaves>()) {
         const char *mconf_string = mconf_get_rsh_command();
         if (mconf_string != nullptr && sge_strnullcasecmp(mconf_string, "none") != 0) {
            var_list_set_string(environmentList, "SGE_RSH_COMMAND", mconf_string);
         } else {
            char default_buffer[SGE_PATH_MAX];
            dstring default_dstring;

            // @todo CS-1262 there is no utilbin/arch/rsh any more
            sge_dstring_init(&default_dstring, default_buffer, SGE_PATH_MAX);
            sge_dstring_sprintf(&default_dstring, "%s/utilbin/%s/rsh", sge_root, arch);
            var_list_set_string(environmentList, "SGE_RSH_COMMAND", sge_dstring_get_string(&default_dstring));
         }
         sge_free(&mconf_string);

         /* transport the notify kill and susp signals to qrsh -inherit */
         if (mconf_get_notify_kill_type() == 0) {
            mconf_string = mconf_get_notify_kill();
            if (mconf_string != nullptr) {
               var_list_set_string(environmentList, "SGE_NOTIFY_KILL_SIGNAL", mconf_string);
               sge_free(&mconf_string);
            }
         }
         if (mconf_get_notify_susp_type() == 0) {
            mconf_string = mconf_get_notify_susp();
            if (mconf_string != nullptr) {
               var_list_set_string(environmentList, "SGE_NOTIFY_SUSP_SIGNAL", mconf_string);
               sge_free(&mconf_string);
            }
         }
      }
   }

   /* forward name of ckpt env to job */
   if ((ep = jep.get<JB_checkpoint_object>())) {
      var_list_set_string(environmentList, VAR_PREFIX "CKPT_ENV", ep.get<CK_name>());
      if (ep.get<CK_ckpt_dir>())
         var_list_set_string(environmentList, VAR_PREFIX "CKPT_DIR", ep.get<CK_ckpt_dir>());
   }

   {
      DSTRING_STATIC(dstr_buffer, SGE_PATH_MAX);
      const char *str_buffer;

      str_buffer = sge_dstring_sprintf(&dstr_buffer, "%s/%s", execd_spool_dir, active_dir_buffer);
      var_list_set_string(environmentList, VAR_PREFIX "JOB_SPOOL_DIR", str_buffer);
   }

#ifdef OCS_WITH_SYSTEMD
   // forward the name of the systemd scope of the job or pe task, e.g. for the epilog to query
   // the Result of the scope
   if (ocs::execd::execd_use_systemd()) {
      std::string systemd_slice;
      std::string systemd_scope;
      DSTRING_STATIC(scope_error_dstr, MAX_STRING_SIZE);
      if (ocs::Job::job_get_systemd_slice_and_scope(jep, jatep, petep, systemd_slice, systemd_scope,
                                                     &scope_error_dstr)) {
         var_list_set_string(environmentList, VAR_PREFIX "JOB_SCOPE", systemd_scope.c_str());
      }
   }
#endif

   /* Bugfix: Issuezilla 1300
    * Because this change could break pre-existing installations, it's been
    * made optional. */
   if (mconf_get_set_lib_path()) {
      /* If we're supposed to be inheriting the environment from shell that
       * spawned the execd, we end up clobbering the lib path with the
       * following call because what we write out overwrites what gets
       * inherited.  To solve this, we have to set the inherited lib path
       * into the environmentList so that the following call finds it,
       * prepends to it, and write it out.
       * This is really completely backwards from the point of this bug fix,
       * but if we don't do this the resulting behavior is not what an
       * average user would expect.
       * This approach has the side effect that when inherit_env and
       * set_lib_path are both true, the lib path will very likely end up
       * containing two SGE lib entries because the shell that spawned the
       * execd mostly likely had the SGE lib set in its lib path.  The hassle
       * of checking through the lib path to see if the SGE lib is already
       * set is not worth it, in my opinion. */
      if (mconf_get_inherit_env()) {
         const char *lib_path_env = var_get_sharedlib_path_name();
         const char *lib_path = sge_getenv(lib_path_env);

         if (lib_path != nullptr) {
            var_list_set_string(environmentList, lib_path_env, lib_path);
         }
      }
      var_list_set_sharedlib_path(environmentList);
   }

   /* set final of variables whose value shall be replaced */
   var_list_copy_prefix_vars(environmentList, *environmentList,
                             VAR_PREFIX, "SGE_");

   /* set final of variables whose value shall not be replaced */
   var_list_copy_prefix_vars_undef(environmentList, *environmentList,
                                   VAR_PREFIX_NR, "SGE_");
   var_list_remove_prefix_vars(*environmentList, VAR_PREFIX);
   var_list_remove_prefix_vars(*environmentList, VAR_PREFIX_NR);

   var_list_dump_to_file(*environmentList, fp);


   FCLOSE(fp);
   /*************************** finished writing environment *****************/

   /**************** write out config file ******************************/
   /* JG: TODO (254) use function sge_get_active_job.... */
   str_fname = sge_dstring_sprintf(&dstr_fname, "%s/config", active_dir_buffer);
   fp = fopen(str_fname, "w");
   if (!fp) {
      environmentList.reset();
      snprintf(err_str, err_length, MSG_FILE_NOOPEN_SS, str_fname, strerror(errno));
      DRETURN(-2);
   }

   // add sge_root and sge_cell to the config file
   // they can be used as special variables in prolog/epilog/pe/ckpt configuration
   fprintf(fp, "sge_root=%s\n", sge_root);
   fprintf(fp, "sge_cell=%s\n", default_cell);

#ifdef COMPILE_DC

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)

#if defined(LINUX)
   if (!sup_groups_in_proc()) {
      environmentList.reset();
      snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOSGID);
      FCLOSE(fp);
      DRETURN(-2);
   }
#endif

   // The usage_collection mode.
   {
      usage_collection_t usage_collection = mconf_get_usage_collection();
      fprintf(fp, "usage_collection=" sge_u32 "\n", usage_collection);
      if (petep == nullptr) {
         jatep.set<JAT_usage_collection>(usage_collection);
      } else {
         petep.set<PET_usage_collection>(usage_collection);
      }
   }

   // Set the additional group id.
   // When we are using systemd we do not need to set an additional group id - we use value 0.
   // Unless in hybrid usage collection mode where we get usage from both Systemd and via PDC.
   // And when we enabled killing by add_grp_id, we also need to set it.
   if (ocs::execd::execd_use_pdc_for_usage_collection() || mconf_get_enable_addgrp_kill()) {
      // parse range and create list
      ocs::tom::ListPtr<ocs::tom::RN> rlp;
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      gid_t temp_id;
      char *gid_range = mconf_get_gid_range();
      DPRINTF("gid_range = %s\n", gid_range);
      range_list_parse_from_string(rlp, &alp, gid_range,
                                   0, 0, INF_NOT_ALLOWED);
      sge_free(&gid_range);
      if (!rlp) {
         snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOPARSEGIDRANGE);
         environmentList.reset();
         FCLOSE(fp);
         DRETURN(-2);
      }

      /* search next add_grp_id */
      temp_id = last_addgrpid;
      last_addgrpid = get_next_addgrpid(*rlp, last_addgrpid);
      while (addgrpid_already_in_use(last_addgrpid)) {
         last_addgrpid = get_next_addgrpid(*rlp, last_addgrpid);
         if (temp_id == last_addgrpid) {
            snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOADDGID);
            environmentList.reset();
            FCLOSE(fp);
            DRETURN(-1);
         }
      }
   } else {
      // We do not use PDC for usage collection nor do we kill by addrgp,
      // no need to set an additional group id.
      last_addgrpid = 0;
   }

   // write add_grp_id to config file and to the job-structure
   fprintf(fp, "add_grp_id=" gid_t_fmt "\n", last_addgrpid);
   {
      char str_id[256];
      snprintf(str_id, sizeof(str_id), gid_t_fmt, last_addgrpid);
      if (petep == nullptr) {
         jatep.set<JAT_osjobid>(str_id);
      } else {
         petep.set<PET_osjobid>(str_id);
      }
   }

   if (mconf_get_ignore_ngroups_max_limit()) {
      fprintf(fp, "skip_ngroups_max_silently=yes\n");
   }

#endif

#endif /*  COMPILE_DC */

   /* handle stdout/stderr */
   /* Setting stdin/stdout/stderr
    * These must point to the 'physical' files, so it must be the given
    * input/output/error-path when file staging is disabled and the
    * temp directory when file staging is enabled.
    */

   /* File Staging */

   /* When there is no path given, we will fill it later (in the shepherd) with
    * the default path. */

   bInputFileStaging = sge_get_fs_path(jep.get<JB_stdin_path_list>(),
                                       fs_stdin_host, SGE_PATH_MAX, fs_stdin_path, SGE_PATH_MAX);

   bOutputFileStaging = sge_get_fs_path(jep.get<JB_stdout_path_list>(),
                                        fs_stdout_host, SGE_PATH_MAX, fs_stdout_path, SGE_PATH_MAX);

   bErrorFileStaging = sge_get_fs_path(jep.get<JB_stderr_path_list>(),
                                       fs_stderr_host, SGE_PATH_MAX, fs_stderr_path, SGE_PATH_MAX);


   /* The fs_stdin_tmp_path and stdin_path (and so on) will be set correctly
    * in the shepherd. But we need the 'standard' stdin_path (like it is without
    * file staging) later anyway, so we generate it here. */

   sge_get_path(qualified_hostname,
                jep.get<JB_stdout_path_list>(), cwd,
                jep.get<JB_owner>(),
                jep.get<JB_job_name>(),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_STDOUT, stdout_path, SGE_PATH_MAX);
   sge_get_path(qualified_hostname,
                jep.get<JB_stderr_path_list>(), cwd,
                jep.get<JB_owner>(),
                jep.get<JB_job_name>(),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_STDERR, stderr_path, SGE_PATH_MAX);
   sge_get_path(qualified_hostname,
                jep.get<JB_stdin_path_list>(), cwd,
                jep.get<JB_owner>(),
                jep.get<JB_job_name>(),
                job_id,
                job_is_array(jep) ? ja_task_id : 0,
                SGE_STDIN, stdin_path, SGE_PATH_MAX);

   DPRINTF("fs_stdin_host=%s\n", fs_stdin_host);
   DPRINTF("fs_stdin_path=%s\n", fs_stdin_path);
   DPRINTF("fs_stdin_tmp_path=%s/%s\n", tmpdir, fs_stdin_file ? fs_stdin_file : "");
   DPRINTF("fs_stdin_file_staging=%d\n", bInputFileStaging);

   DPRINTF("fs_stdout_host=%s\n", fs_stdout_host);
   DPRINTF("fs_stdout_path=%s\n", fs_stdout_path);
   DPRINTF("fs_stdout_tmp_path=%s/%s\n", tmpdir, fs_stdout_file ? fs_stdout_file : "");
   DPRINTF("fs_stdout_file_staging=%d\n", bOutputFileStaging);

   DPRINTF("fs_stderr_host=%s\n", fs_stderr_host);
   DPRINTF("fs_stderr_path=%s\n", fs_stderr_path);
   DPRINTF("fs_stderr_tmp_path=%s/%s\n", tmpdir, fs_stderr_file ? fs_stderr_file : "");
   DPRINTF("fs_stderr_file_staging=%d\n", bErrorFileStaging);

   fprintf(fp, "fs_stdin_host=%s\n", fs_stdin_host);
   fprintf(fp, "fs_stdin_path=%s\n", fs_stdin_path);
   fprintf(fp, "fs_stdin_tmp_path=%s/%s\n", tmpdir, fs_stdin_file ? fs_stdin_file : "");
   fprintf(fp, "fs_stdin_file_staging=%d\n", bInputFileStaging);

   fprintf(fp, "fs_stdout_host=%s\n", fs_stdout_host);
   fprintf(fp, "fs_stdout_path=%s\n", fs_stdout_path);
   fprintf(fp, "fs_stdout_tmp_path=%s/%s\n", tmpdir, fs_stdout_file ? fs_stdout_file : "");
   fprintf(fp, "fs_stdout_file_staging=%d\n", bOutputFileStaging);

   fprintf(fp, "fs_stderr_host=%s\n", fs_stderr_host);
   fprintf(fp, "fs_stderr_path=%s\n", fs_stderr_path);
   fprintf(fp, "fs_stderr_tmp_path=%s/%s\n", tmpdir, fs_stderr_file ? fs_stderr_file : "");
   fprintf(fp, "fs_stderr_file_staging=%d\n", bErrorFileStaging);

   fprintf(fp, "stdout_path=%s\n", stdout_path);
   fprintf(fp, "stderr_path=%s\n", stderr_path);
   fprintf(fp, "stdin_path=%s\n", stdin_path);
   fprintf(fp, "merge_stderr=%d\n", (int) jep.get<JB_merge_stderr>());

   fprintf(fp, "tmpdir=%s\n", tmpdir);

   DPRINTF("stdout_path=%s\n", stdout_path);
   DPRINTF("stderr_path=%s\n", stderr_path);
   DPRINTF("stdin_path=%s\n", stdin_path);
   DPRINTF("merge_stderr=%d\n", (int) jep.get<JB_merge_stderr>());

   {
      uint32_t jb_now = jep.get<JB_type>();
      int handle_as_binary = (JOB_TYPE_IS_BINARY(jb_now) ? 1 : 0);
      int no_shell = (JOB_TYPE_IS_NO_SHELL(jb_now) ? 1 : 0);

      fprintf(fp, "handle_as_binary=%d\n", handle_as_binary);
      fprintf(fp, "no_shell=%d\n", no_shell);
   }

   if (jep.get<JB_checkpoint_attr>() &&
       (ep = jep.get<JB_checkpoint_object>())) {
      fprintf(fp, "ckpt_job=1\n");
      fprintf(fp, "ckpt_restarted=%d\n", petep != nullptr ? 0 : (int) jatep.get<JAT_job_restarted>());
      fprintf(fp, "ckpt_pid=%d\n", (int) jatep.get<JAT_pvm_ckpt_pid>());
      fprintf(fp, "ckpt_osjobid=%s\n", jatep.get<JAT_osjobid>() ? jatep.get<JAT_osjobid>() : "0");
      fprintf(fp, "ckpt_interface=%s\n", ep.get<CK_interface>());
      fprintf(fp, "ckpt_command=%s\n", ep.get<CK_ckpt_command>());
      fprintf(fp, "ckpt_migr_command=%s\n", ep.get<CK_migr_command>());
      fprintf(fp, "ckpt_rest_command=%s\n", ep.get<CK_rest_command>());
      fprintf(fp, "ckpt_clean_command=%s\n", ep.get<CK_clean_command>());
      fprintf(fp, "ckpt_dir=%s\n", ep.get<CK_ckpt_dir>());
      fprintf(fp, "ckpt_signal=%s\n", ep.get<CK_signal>());

      if (!(jep.get<JB_checkpoint_attr>() & CHECKPOINT_AT_MINIMUM_INTERVAL)) {
         interval = 0;
      } else {
         parse_ulong_val(nullptr, &interval, ocs::CEntry::Type::TIME, master_q.get<QU_min_cpu_interval>(),
                         nullptr, 0);
         interval = MAX(interval, jep.get<JB_checkpoint_interval>());
      }
      fprintf(fp, "ckpt_interval=%d\n", (int) interval);
   } else {
      fprintf(fp, "ckpt_job=0\n");
   }

   /*
    * Write the limit A of the task, see sge_exec_job_get_limit(). For a task which stands for
    * several slots it is already the sum, sge_shepherd applies it as it is.
    */
#define WRITE_LIMIT(A, T) \
   fprintf(fp, #A"=%s\n", sge_exec_job_get_limit<QU_##A>(&dstr_limit, #A, T, master_q, jep, jatep, petep, \
                                                         qualified_hostname))
   {
      DSTRING_STATIC(dstr_limit, 512);
      WRITE_LIMIT(h_vmem, ocs::CEntry::Type::MEM);
      WRITE_LIMIT(s_vmem, ocs::CEntry::Type::MEM);

      WRITE_LIMIT(h_cpu, ocs::CEntry::Type::TIME);
      WRITE_LIMIT(s_cpu, ocs::CEntry::Type::TIME);

      WRITE_LIMIT(h_stack, ocs::CEntry::Type::MEM);
      WRITE_LIMIT(s_stack, ocs::CEntry::Type::MEM);

      WRITE_LIMIT(h_data, ocs::CEntry::Type::MEM);
      WRITE_LIMIT(s_data, ocs::CEntry::Type::MEM);

      WRITE_LIMIT(h_core, ocs::CEntry::Type::MEM);
      WRITE_LIMIT(s_core, ocs::CEntry::Type::MEM);

      WRITE_LIMIT(h_rss, ocs::CEntry::Type::MEM);
      WRITE_LIMIT(s_rss, ocs::CEntry::Type::MEM);

      WRITE_LIMIT(h_fsize, ocs::CEntry::Type::MEM);
      WRITE_LIMIT(s_fsize, ocs::CEntry::Type::MEM);
   }

   // instrumentation for the testsuite: on a host with systemd, sge_shepherd also sets
   // MemorySwapMax of the job scope, so that a job reaching MemoryMax is not just swapped out
   {
      const auto swap_max = jep.get<JB_env_list>().find_first<VA_variable>("SGE_DEBUG_MEMORY_SWAP_MAX");
      if (swap_max && swap_max.get<VA_value>() != nullptr) {
         fprintf(fp, "memory_swap_max=%s\n", swap_max.get<VA_value>());
      }
   }

   {
      char *s;

      mconf_get_s_descriptors(&s);
      fprintf(fp, "s_descriptors=%s\n", s);
      sge_free(&s);

      mconf_get_h_descriptors(&s);
      fprintf(fp, "h_descriptors=%s\n", s);
      sge_free(&s);

      mconf_get_s_maxproc(&s);
      fprintf(fp, "s_maxproc=%s\n", s);
      sge_free(&s);

      mconf_get_h_maxproc(&s);
      fprintf(fp, "h_maxproc=%s\n", s);
      sge_free(&s);

      mconf_get_s_memorylocked(&s);
      fprintf(fp, "s_memorylocked=%s\n", s);
      sge_free(&s);

      mconf_get_h_memorylocked(&s);
      fprintf(fp, "h_memorylocked=%s\n", s);
      sge_free(&s);

      mconf_get_s_locks(&s);
      fprintf(fp, "s_locks=%s\n", s);
      sge_free(&s);

      mconf_get_h_locks(&s);
      fprintf(fp, "h_locks=%s\n", s);
      sge_free(&s);
   }

   fprintf(fp, "priority=%s\n", master_q.get<QU_priority>());
   fprintf(fp, "shell_path=%s\n", shell_path);
   fprintf(fp, "script_file=%s\n", str_script_file);
   fprintf(fp, "job_owner=%s\n", jep.get<JB_owner>());
   fprintf(fp, "min_gid=" sge_u32 "\n", mconf_get_min_gid());
   fprintf(fp, "min_uid=" sge_u32 "\n", mconf_get_min_uid());

   /* do path substitutions also for cwd */
   {
      char cwd_buf[SGE_PATH_MAX];
      dstring dstr_cwd;

      sge_dstring_init(&dstr_cwd, cwd_buf, sizeof(cwd_buf));

      if ((cp = expand_path(&dstr_cwd, cwd, job_id, job_is_array(jep) ? ja_task_id : 0,
                            jep.get<JB_job_name>(),
                            jep.get<JB_owner>(),
                            qualified_hostname))) {
         cwd = sge_dstring_copy_string(&cwd_out, cp);
      }
      fprintf(fp, "cwd=%s\n", cwd);
   }

   /* do not start prolog/epilog in case of pe tasks */
   if (petep == nullptr) {
      char *prolog = mconf_get_prolog();
      char *epilog = mconf_get_epilog();

      fprintf(fp, "prolog=%s\n",
              ((cp = master_q.get<QU_prolog>()) && strcasecmp(cp, "none")) ?
              cp : prolog);
      fprintf(fp, "epilog=%s\n",
              ((cp = master_q.get<QU_epilog>()) && strcasecmp(cp, "none")) ?
              cp : epilog);
      sge_free(&prolog);
      sge_free(&epilog);
   } else {
      fprintf(fp, "prolog=%s\n", "none");
      fprintf(fp, "epilog=%s\n", "none");
   }

   fprintf(fp, "starter_method=%s\n", (cp = master_q.get<QU_starter_method>()) ? cp : "none");
   fprintf(fp, "suspend_method=%s\n", (cp = master_q.get<QU_suspend_method>()) ? cp : "none");
   fprintf(fp, "resume_method=%s\n", (cp = master_q.get<QU_resume_method>()) ? cp : "none");
   fprintf(fp, "terminate_method=%s\n", (cp = master_q.get<QU_terminate_method>()) ? cp : "none");

   fprintf(fp, "script_timeout=" sge_u32 "\n", mconf_get_script_timeout());

   fprintf(fp, "pe=%s\n", jatep.get<JAT_granted_pe>() ? jatep.get<JAT_granted_pe>() : "none");
   fprintf(fp, "pe_slots=%d\n", pe_slots);
   fprintf(fp, "host_slots=%d\n", host_slots);

   /* write pe related data */
   if (jatep.get<JAT_granted_pe>()) {
      ocs::tom::Element<ocs::tom::PE> pep = nullptr;
      /* no pe start/stop for petasks */
      if (petep == nullptr) {
         pep = jatep.get<JAT_pe_object>();
      }
      fprintf(fp, "pe_hostfile=%s/%s/%s\n", execd_spool_dir, active_dir_buffer, PE_HOSTFILE);
      fprintf(fp, "pe_start=%s\n", pep != nullptr && pep.get<PE_start_proc_args>() ?
                                   pep.get<PE_start_proc_args>() : "none");
      fprintf(fp, "pe_stop=%s\n", pep != nullptr && pep.get<PE_stop_proc_args>() ?
                                  pep.get<PE_stop_proc_args>() : "none");

      /* build path for stdout of pe scripts */
      sge_get_path(qualified_hostname,
                   jep.get<JB_stdout_path_list>(), cwd,
                   jep.get<JB_owner>(),
                   jep.get<JB_job_name>(),
                   job_id,
                   job_is_array(jep) ? ja_task_id : 0,
                   SGE_PAR_STDOUT, pe_stdout_path, SGE_PATH_MAX);
      fprintf(fp, "pe_stdout_path=%s\n", pe_stdout_path);

      /* build path for stderr of pe scripts */
      sge_get_path(qualified_hostname,
                   jep.get<JB_stderr_path_list>(), cwd,
                   jep.get<JB_owner>(),
                   jep.get<JB_job_name>(),
                   job_id,
                   job_is_array(jep) ? ja_task_id : 0,
                   SGE_PAR_STDERR, pe_stderr_path, SGE_PATH_MAX);
      fprintf(fp, "pe_stderr_path=%s\n", pe_stderr_path);
   }

   shell_start_mode = mconf_get_shell_start_mode();
   fprintf(fp, "shell_start_mode=%s\n",
           job_get_shell_start_mode(master_q, shell_start_mode));
   sge_free(&shell_start_mode);

   /* we need the basename for loginshell test */
   shell = strrchr(shell_path, '/');
   if (shell == nullptr) {
      shell = shell_path;
   } else {
      shell++;
   }
   fprintf(fp, "use_login_shell=%d\n", ck_login_sh(shell) ? 1 : 0);

   // systemd specific options:
   //    - enable_systemd
   //    - slice and scope
   //    - devices_allow
#ifdef OCS_WITH_SYSTEMD
   {
      bool enable_systemd = ocs::execd::execd_use_systemd();
      fprintf(fp, "enable_systemd=%d\n", enable_systemd ? 1 : 0);
      // factor for MemoryMax / MemoryHigh of the job scope, see execd_params MEMORY_MAX_FACTOR
      fprintf(fp, "memory_max_factor=%f\n", mconf_get_memory_max_factor());

      if (enable_systemd) {
         // slice and scope
         std::string systemd_slice;
         std::string systemd_scope;
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (ocs::Job::job_get_systemd_slice_and_scope(jep, jatep, petep, systemd_slice, systemd_scope, &error_dstr)) {
            fprintf(fp, "systemd_slice=%s\n", systemd_slice.c_str());
            fprintf(fp, "systemd_scope=%s\n", systemd_scope.c_str());

            if (petep == nullptr) {
               jatep.set<JAT_systemd_scope>(systemd_scope.c_str());
            } else {
               petep.set<PET_systemd_scope>(systemd_scope.c_str());
            }
         }

         // Device isolation for the shepherd (CS-1338 + CS-2462).
         //
         // The devices_allow= line is the union of the "devices" characteristic of every RSMAP
         // instance granted on this host. It comes from the configuration only - nothing the job
         // brings along may widen it (CS-2814).
         //
         // Value format of the characteristic and of the emitted line:
         //     path[:mode];path[:mode];...
         // where mode is one of r/w/rw. A missing mode defaults to "r" on
         // the shepherd side. A path named by several instances is merged
         // with the widest access mode ('r' and 'w' seen for the same path
         // becomes 'rw').
         {
            std::map<std::string, std::string> device_modes;

            auto merge_mode = [](const std::string &a, const std::string &b) -> std::string {
               const bool r = a.find('r') != std::string::npos || b.find('r') != std::string::npos;
               const bool w = a.find('w') != std::string::npos || b.find('w') != std::string::npos;
               if (r && w) return "rw";
               if (r) return "r";
               if (w) return "w";
               return "";
            };

            // A device systemd does not accept makes the creation of the whole job scope fail,
            // and with it the job start. Leave such a device out instead and say so in the
            // messages file: that is fail-closed, the job gets less access and never more. The
            // configuration is checked when it is written, so only a value spooled before that
            // check existed can still arrive here (CS-2924).
            auto add_devices = [&](const char *value, const char *rsmap_name, const char *id) {
               if (value == nullptr || *value == '\0') {
                  return;
               }
               char *buf = strdup(value);
               struct saved_vars_s *ctx = nullptr;
               for (char *tok = sge_strtok_r(buf, ";", &ctx); tok != nullptr;
                    tok = sge_strtok_r(nullptr, ";", &ctx)) {
                  char *colon = strchr(tok, ':');
                  std::string path;
                  std::string mode;
                  if (colon == nullptr) {
                     path = tok;
                  } else {
                     *colon = '\0';
                     path = tok;
                     mode = colon + 1;
                  }
                  if (path.empty()) {
                     continue;
                  }
                  if (!ocs::uti::systemd_device_is_valid(path)) {
                     WARNING(MSG_EXECD_SYSTEMD_DEVICE_INVALID_SSS, path.c_str(),
                             rsmap_name != nullptr ? rsmap_name : "",
                             id != nullptr ? id : "");
                     continue;
                  }
                  auto it = device_modes.find(path);
                  if (it == device_modes.end()) {
                     device_modes[path] = mode;
                  } else {
                     it->second = merge_mode(it->second, mode);
                  }
               }
               sge_free_saved_vars(ctx);
               free(buf);
            };

            // the "devices" characteristic of the RSMAP instances granted on this host - see
            // exec_job_granted_resources_for_host()
            const auto host_grants = exec_job_granted_resources_for_host(granted_resources_list,
                                                                         qualified_hostname);
            for (const auto gru : *host_grants) {
               if (static_cast<ocs::GrantedResources::Type>(gru.get<GRU_type>()) !=
                   ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
                  continue;
               }
               for (const auto resl : gru.get<GRU_resource_map_list>()) {
                  const auto dev_prop = resl.get<RESL_properties>().find_first<CE_name>(LOAD_ATTR_DEVICES);
                  if (dev_prop) {
                     add_devices(dev_prop.get<CE_stringval>(), gru.get<GRU_name>(),
                                 resl.get<RESL_value>());
                  }
               }
            }

            // Emit the merged line (sorted by path — deterministic for testing).
            std::string devices_allow;
            for (const auto &kv : device_modes) {
               if (!devices_allow.empty()) {
                  devices_allow += ';';
               }
               devices_allow += kv.first;
               if (!kv.second.empty()) {
                  devices_allow += ':';
                  devices_allow += kv.second;
               }
            }
            fprintf(fp, "devices_allow=%s\n", devices_allow.c_str());
         }

         // in case of tightly integrated parallel jobs, we need to store the systemd slice.
         // this is one place (where the master task is started),
         // the other one is when the slave container is started.
         if (petep == nullptr) {
            ocs::execd::execd_store_tight_pe_slice(jep, jatep, systemd_slice.c_str());
         }
      }
   }
#endif

   /* the following values are needed by the reaper */
   if (mailrec_unparse(jep.get<JB_mail_list>(), mail_str, sizeof(mail_str))) {
      ERROR(MSG_MAIL_MAILLISTTOOLONG_U, job_id);
   }
   fprintf(fp, "mail_list=%s\n", mail_str);
   fprintf(fp, "mail_options=" sge_u32 "\n", jep.get<JB_mail_options>());
   fprintf(fp, "forbid_reschedule=%d\n", mconf_get_forbid_reschedule() ? 1 : 0);
   fprintf(fp, "forbid_apperror=%d\n", mconf_get_forbid_apperror() ? 1 : 0);
   fprintf(fp, "queue=%s\n", master_q.get<QU_qname>());
   fprintf(fp, "host=%s\n", master_q.get<QU_qhostname>().c_str());

   // Write processor set
   {
      dstring range_string = DSTRING_INIT;
      range_list_print_to_string(*processor_set, &range_string, true, false, false);
      fprintf(fp, "processors=%s\n", sge_dstring_get_string(&range_string));
      sge_dstring_free(&range_string);
   }

   // Write binding info
   if (binding_to_use_is_initialized) {
#if defined (OCS_HWLOC)
      fprintf(fp, "binding_instance=%s\n", ocs::BindingInstance::to_string(binding_instance).c_str());
      fprintf(fp, "binding_to_use=%s\n", binding_to_use.to_product_topology_string().c_str());
#endif
   } else {
      fprintf(fp, "binding_instance=%s\n", NONE_STR);
      fprintf(fp, "binding_to_use=%s\n", NONE_STR);
   }

   // Write cpuset if available
   if (binding_cpuset_is_initialized) {
#if defined (OCS_HWLOC)
      char *cpuset_string = NULL;
      hwloc_bitmap_asprintf(&cpuset_string, cpuset);
      fprintf(fp, "binding_cpuset=%s\n", cpuset_string);
      sge_free(&cpuset_string);
      hwloc_bitmap_free(cpuset);
#else
      fprintf(fp, "binding_cpuset=%s\n", NONE_STR);
#endif
   }

   if (petep != nullptr) {
      fprintf(fp, "job_name=%s\n", petep.get<PET_name>());
   } else {
      fprintf(fp, "job_name=%s\n", jep.get<JB_job_name>());
   }
   fprintf(fp, "job_id=" sge_u32 "\n", job_id);
   fprintf(fp, "ja_task_id=" sge_u32 "\n", job_is_array(jep) ? ja_task_id : 0);
   if (petep != nullptr) {
      fprintf(fp, "pe_task_id=%s\n", pe_task_id);
   }
   fprintf(fp, "account=%s\n", (jep.get<JB_account>() ? jep.get<JB_account>() : DEFAULT_ACCOUNT));
   if (petep != nullptr) {
      fprintf(fp, "submission_time=" sge_u64 "\n", petep.get<PET_submission_time>());
   } else {
      fprintf(fp, "submission_time=" sge_u64 "\n", jep.get<JB_submission_time>());
   }

   {
      uint32_t notify = 0;
      if (jep.get<JB_notify>()) {
         parse_ulong_val(nullptr, &notify, ocs::CEntry::Type::TIME, master_q.get<QU_notify>(), nullptr, 0);
      }
      fprintf(fp, "notify=" sge_u32 "\n", notify);
   }

   /*
   ** interactive (qsh) jobs have no exec file
   */
   if (!jep.get<JB_script_file>()) {
      uint32_t jb_now;
      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
      } else {
         jb_now = jep.get<JB_type>();
      }

      if (!(JOB_TYPE_IS_QLOGIN(jb_now) ||
            JOB_TYPE_IS_QRSH(jb_now) ||
            JOB_TYPE_IS_QRLOGIN(jb_now))) {
         char *xterm = mconf_get_xterm();
         DPRINTF("interactive job\n");
         /*
         ** get xterm configuration value
         */

         if (xterm) {
            fprintf(fp, "exec_file=%s\n", xterm);
            DPRINTF("exec_file=%s\n", xterm);
         } else {
            snprintf(err_str, err_length, SFNMAX, MSG_EXECD_NOXTERM);
            FCLOSE(fp);
            environmentList.reset();
            sge_dstring_free(&core_binding_strategy_string);
            DRETURN(-2);
            /*
            ** this causes a general failure
            */
         }
         sge_free(&xterm);
      }
   }

   if ((cp = jep.get<JB_project>()) && *cp != '\0')
      fprintf(fp, "acct_project=%s\n", cp);
   else
      fprintf(fp, "acct_project=none\n");


   {
      ocs::tom::ConstList<ocs::tom::ST> args;

      int nargs = 1;

      /* for pe tasks we have no args - the daemon (rshd etc.) to start
       * comes from the cluster configuration
       */
      if (petep != nullptr) {
         args = nullptr;
      } else {
         args = jep.get<JB_job_args>();
      }

      fprintf(fp, "njob_args=" sge_u32 "\n", static_cast<uint32_t>(args.size()));

      for (const auto se : args) {
         const char *arg = se.get<ST_name>();
         if (arg != nullptr) {
            fprintf(fp, "job_arg%d=%s\n", nargs++, arg);
         } else {
            fprintf(fp, "job_arg%d=\n", nargs++);
         }
      }
   }

   fprintf(fp, "queue_tmpdir=%s\n", master_q.get<QU_tmpdir>());
   /*
   ** interactive jobs need a display to send output back to (only qsh - JG)
   */
   if (!jep.get<JB_script_file>()) {         /* interactive job  */
      uint32_t jb_now;
      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
      } else {
         jb_now = jep.get<JB_type>();    /* detect qsh case  */
      }

      /* check DISPLAY variable for qsh jobs */
      if (JOB_TYPE_IS_QSH(jb_now)) {
         // raw out parameter of job_check_qsh_display()
         ocs::tom::ListPtr<ocs::tom::AN> answer_list;

         if (job_check_qsh_display(jep, &answer_list, false) == STATUS_OK) {
            env = jep.get<JB_env_list>().find_first<VA_variable>("DISPLAY");
            fprintf(fp, "display=%s\n", env.get<VA_value>());
         } else {
            /* JG: TODO: the whole function should not use err_str to return
             *           error descriptions, but an answer list.
             */
            const char *error_string = ocs::tom::ConstList<ocs::tom::AN>(answer_list).front().get<AN_text>();
            if (error_string != nullptr) {
               snprintf(err_str, err_length, SFNMAX, error_string);
            }
            answer_list.reset();
            environmentList.reset();
            FCLOSE(fp);
            sge_dstring_free(&core_binding_strategy_string);
            DRETURN(-1);
         }
      }
   }

   /* if "pag_cmd" is not set, do not use AFS setup for this host */
   pag_cmd = mconf_get_pag_cmd();
   if (ocs::Bootstrap::has_security_mode(ocs::Bootstrap::BS_SEC_MODE_AFS) && pag_cmd &&
       strlen(pag_cmd) && strcasecmp(pag_cmd, "none")) {
      fprintf(fp, "use_afs=1\n");

      shepherd_name = SGE_COSHEPHERD;
      snprintf(coshepherd_path, sizeof(coshepherd_path), "%s/%s/%s", binary_path, sge_get_arch(), shepherd_name);
      fprintf(fp, "coshepherd=%s\n", coshepherd_path);
      set_token_cmd = mconf_get_set_token_cmd();
      fprintf(fp, "set_token_cmd=%s\n", set_token_cmd ? set_token_cmd : "none");
      sge_free(&set_token_cmd);
      fprintf(fp, "token_extend_time=%d\n", (int) mconf_get_token_extend_time());
   } else {
      fprintf(fp, "use_afs=0\n");
   }
   sge_free(&pag_cmd);

   fprintf(fp, "admin_user=%s\n", admin_user);

   /* notify method */
   fprintf(fp, "notify_kill_type=%d\n", mconf_get_notify_kill_type());
   notify_kill = mconf_get_notify_kill();
   fprintf(fp, "notify_kill=%s\n", notify_kill ? notify_kill : "default");
   sge_free(&notify_kill);
   fprintf(fp, "notify_susp_type=%d\n", mconf_get_notify_susp_type());
   notify_susp = mconf_get_notify_susp();
   fprintf(fp, "notify_susp=%s\n", notify_susp ? notify_susp : "default");
   sge_free(&notify_susp);
   if (mconf_get_use_qsub_gid()) {
      fprintf(fp, "qsub_gid=" sge_u32 "\n", jep.get<JB_gid>());
   } else {
      fprintf(fp, "qsub_gid=%s\n", "no");
   }

   /* config for interactive jobs */
   {
      uint32_t jb_now;
      int pty_option;
      if (petep != nullptr) {
         jb_now = JOB_TYPE_QRSH;
      } else {
         jb_now = jep.get<JB_type>();
      }

      if (petep != nullptr) {
         fprintf(fp, "pe_task_id=%s\n", pe_task_id);
      }

      pty_option = (int) jep.get<JB_pty>();
      if (pty_option < 0 || pty_option > 2) {
         pty_option = 2;   /* 2 = use default */
      }
      fprintf(fp, "pty=%d\n", pty_option);

      if (JOB_TYPE_IS_QLOGIN(jb_now) || JOB_TYPE_IS_QRSH(jb_now)
          || JOB_TYPE_IS_QRLOGIN(jb_now)) {
         ocs::tom::ConstElement<ocs::tom::VA> elem;
         char daemon[SGE_PATH_MAX];

         fprintf(fp, "master_host=%s\n", masterhost);
         fprintf(fp, "commd_port=-1\n");  /* commd_port not used for GE > 6.0 */

         if ((elem = environmentList->find_first<VA_variable>("QRSH_PORT"))) {
            fprintf(fp, "qrsh_control_port=%s\n", elem.get<VA_value>());
         }

         fprintf(fp, "ijs_reconnect_timeout=%d\n", mconf_get_ijs_reconnect_timeout());

         snprintf(daemon, sizeof(daemon), "%s/utilbin/%s/", sge_root, arch);

         if(JOB_TYPE_IS_QLOGIN(jb_now)) {
            char* qlogin_daemon = mconf_get_qlogin_daemon();
            fprintf(fp, "qlogin_daemon=%s\n", qlogin_daemon);
            sge_free(&qlogin_daemon);
         } else {
            // @todo CS-1262 we do no longer deliver a rshd or rlogind
            //       no longer need the write_osjob_id config value, remove it from here and from shepherd
            if (JOB_TYPE_IS_QRSH(jb_now)) {
               char *rsh_daemon = mconf_get_rsh_daemon();
               strcat(daemon, "rshd");
               if (strcasecmp(rsh_daemon, "none") == 0) {
                  strcat(daemon, " -l");
                  fprintf(fp, "rsh_daemon=%s\n", daemon);
                  write_osjob_id = 0; /* will be done by our rshd */
               } else {
                  fprintf(fp, "rsh_daemon=%s\n", rsh_daemon);
                  if (strncmp(daemon, rsh_daemon, strlen(daemon)) == 0) {
                     write_osjob_id = 0; /* will be done by our rshd */
                  }
               }
               sge_free(&rsh_daemon);

               fprintf(fp, "qrsh_tmpdir=%s\n", tmpdir);

               if (petep != nullptr) {
                  fprintf(fp, "qrsh_pid_file=%s/pid.%s\n", tmpdir, pe_task_id);
               } else {
                  fprintf(fp, "qrsh_pid_file=%s/pid\n", tmpdir);
               }
            } else {
               if (JOB_TYPE_IS_QRLOGIN(jb_now)) {
                  char *rlogin_daemon = mconf_get_rlogin_daemon();
                  strcat(daemon, "rlogind");
                  if (strcasecmp(rlogin_daemon, "none") == 0) {
                     strcat(daemon, " -l");
                     fprintf(fp, "rlogin_daemon=%s\n", daemon);
                     write_osjob_id = 0; /* will be done by our rlogind */
                  } else {
                     fprintf(fp, "rlogin_daemon=%s\n", rlogin_daemon);
                     if (strncmp(daemon, rlogin_daemon, strlen(daemon)) == 0) {
                        write_osjob_id = 0; /* will be done by our rlogind */
                     }
                  }
                  sge_free(&rlogin_daemon);
               }
            }
         }
      }
   }

   /* shall shepherd write osjob_id, or is it done by (our) rshd */
   fprintf(fp, "write_osjob_id=%d\n", write_osjob_id);

   /* should the job inherit the execd's environment? */
   fprintf(fp, "inherit_env=%d\n", (int) mconf_get_inherit_env());

   /* should the addgrp-id be used to kill processes? */
   fprintf(fp, "enable_addgrp_kill=%d\n", (int) mconf_get_enable_addgrp_kill());

   if (ocs::Bootstrap::has_security_mode(ocs::Bootstrap::BS_SEC_MODE_TLS)) {
      tls_mode = true;
   }
   fprintf(fp, "tls=%d\n", (int) tls_mode);

   /* with new interactive job support, shepherd needs ignore_fqdn and default_domain */
   fprintf(fp, "ignore_fqdn=%d\n", ocs::Bootstrap::get_ignore_fqdn());
   fprintf(fp, "default_domain=%s\n", ocs::Bootstrap::get_default_domain());

   environmentList.reset();
   FCLOSE(fp);

   sge_dstring_free(&core_binding_strategy_string);

   /********************** finished writing config ************************/

#if defined(OCS_WITH_OPENSSL)
   if (tls_mode) {
      // for qrsh type jobs sge_shepherd needs the tls certificate of the qrsh commlib server
      // write it to the jobs's spool directory
      const char *cert;
      if (petep == nullptr) {
         cert = jep.get<JB_cred>();
      } else {
         cert = petep.get<PET_cred>();
      }
      if (cert != nullptr) {
         DSTRING_STATIC(dstr_certfilename, MAXPATHLEN);
         const char *str_certfilename = sge_dstring_sprintf(&dstr_certfilename, "%s/%s/%s", execd_spool_dir,
                                                            active_dir_buffer, "cert.pem");
         if (sge_string2file(cert, 0, str_certfilename) != 0) {
            snprintf(err_str, err_length, MSG_EXECD_UNABLETOCREATECERTFILE_S, str_certfilename);
            DRETURN(-2);
         }
      }
   }
#endif

   /* test whether we can access scriptfile */
   /*
   ** tightly integrated (qrsh) and interactive jobs dont need to access script file
   */
   if (petep == nullptr) {
      uint32_t jb_now = jep.get<JB_type>();
      JOB_TYPE_CLEAR_IMMEDIATE(jb_now);            /* batch jobs can also be immediate */
      if (jb_now == 0) {                          /* it is a batch job */
         if (SGE_STAT(str_script_file, &buf)) {
            snprintf(err_str, err_length, MSG_EXECD_UNABLETOFINDSCRIPTFILE_SS,
                     str_script_file, strerror(errno));
            DRETURN(-2);
         }
      }
   }

   shepherd_name = SGE_SHEPHERD;
   snprintf(shepherd_path, sizeof(shepherd_path), "%s/%s/%s", binary_path, arch, shepherd_name);

   if (SGE_STAT(shepherd_path, &buf)) {
      /* second chance: without architecture */
      snprintf(shepherd_path, sizeof(shepherd_path), "%s/%s", binary_path, shepherd_name);
      if (SGE_STAT(shepherd_path, &buf)) {
         snprintf(err_str, err_length, MSG_EXECD_NOSHEPHERD_SSS, arch, shepherd_path, strerror(errno));
         DRETURN(-2);
      }
   }

   pag_cmd = mconf_get_pag_cmd();
   shepherd_cmd = mconf_get_shepherd_cmd();
   if (shepherd_cmd && strlen(shepherd_cmd) &&
       strcasecmp(shepherd_cmd, "none")) {
      if (SGE_STAT(shepherd_cmd, &buf)) {
         snprintf(err_str, err_length, MSG_EXECD_NOSHEPHERDWRAP_SS, shepherd_cmd, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
   }
   else if (ocs::Bootstrap::has_security_mode(ocs::Bootstrap::BS_SEC_MODE_AFS) && pag_cmd &&
              strlen(pag_cmd) && strcasecmp(pag_cmd, "none")) {
      int fd, len;
      const char *cp;

      if (SGE_STAT(coshepherd_path, &buf)) {
         shepherd_name = SGE_COSHEPHERD;
         snprintf(coshepherd_path, sizeof(coshepherd_path), "%s/%s", binary_path, shepherd_name);
         if (SGE_STAT(coshepherd_path, &buf)) {
            snprintf(err_str, err_length, MSG_EXECD_NOCOSHEPHERD_SSS, arch, coshepherd_path, strerror(errno));
            sge_free(&pag_cmd);
            sge_free(&shepherd_cmd);
            DRETURN(-2);
         }
      }
      set_token_cmd = mconf_get_set_token_cmd();
      if (!set_token_cmd ||
          !strlen(set_token_cmd) || !mconf_get_token_extend_time()) {
         snprintf(err_str, err_length, SFNMAX, MSG_EXECD_AFSCONFINCOMPLETE);
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
      sge_free(&set_token_cmd);

      /* JG: TODO (254) use function sge_get_active_job.... */
      str_fname = sge_dstring_sprintf(&dstr_fname, "%s/%s", active_dir_buffer, TOKEN_FILE);
      if ((fd = SGE_OPEN3(str_fname, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1) {
         snprintf(err_str, err_length, MSG_EXECD_NOCREATETOKENFILE_S, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }

      cp = jep.get<JB_tgt>();
      if (!cp || !(len = strlen(cp))) {
         snprintf(err_str, err_length, SFNMAX, MSG_EXECD_TOKENZERO);
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-3); /* problem of this user */
      }
      if (write(fd, cp, len) != len) {
         snprintf(err_str, err_length, MSG_EXECD_NOWRITETOKEN_S, strerror(errno));
         sge_free(&pag_cmd);
         sge_free(&shepherd_cmd);
         DRETURN(-2);
      }
      close(fd);
   }
   sge_free(&pag_cmd);
   sge_free(&shepherd_cmd);

   /* send mail to users if requested */
   if (petep == nullptr) {
      if (VALID(MAIL_AT_BEGINNING, jep.get<JB_mail_options>())) {
         dstring subject = DSTRING_INIT;
         dstring body = DSTRING_INIT;
         dstring ds = DSTRING_INIT;

         sge_ctime64(jatep.get<JAT_start_time>(), &ds);

         if (job_is_array(jep)) {
            sge_dstring_sprintf(&subject, MSG_MAIL_STARTSUBJECT_UUS, job_id,
                                ja_task_id, jep.get<JB_job_name>());
            sge_dstring_sprintf(&body, MSG_MAIL_STARTBODY_UUSSSSS,
                                job_id, ja_task_id,
                                jep.get<JB_job_name>(),
                                jep.get<JB_owner>(),
                                master_q.get<QU_qname>(),
                                master_q.get<QU_qhostname>().c_str(), sge_dstring_get_string(&ds));
         } else {
            sge_dstring_sprintf(&subject, MSG_MAIL_STARTSUBJECT_US, job_id,
                                jep.get<JB_job_name>());
            sge_dstring_sprintf(&body, MSG_MAIL_STARTBODY_USSSSS,
                                job_id,
                                jep.get<JB_job_name>(),
                                jep.get<JB_owner>(),
                                master_q.get<QU_qname>(),
                                master_q.get<QU_qhostname>().c_str(), sge_dstring_get_string(&ds));
         }

         mail_to_users(EXECD, jep.get<JB_mail_list>(), sge_dstring_get_string(&subject),
                   sge_dstring_get_string(&body), MSG_MAIL_TYPE_START);
         sge_dstring_free(&ds);
         sge_dstring_free(&subject);
         sge_dstring_free(&body);
      }
   }

   /* Change to jobs directory. Father changes back to cwd. We do this to
      ensure chdir() works before forking. */
   if (chdir(active_dir_buffer)) {
      snprintf(err_str, err_length, MSG_FILE_CHDIR_SS, active_dir_buffer, strerror(errno));
      DRETURN(-2);
   }

   /* CS-2226: create the per-job signal_pipe FIFO (job control signals) in the active job dir
    * (cwd is now the active dir, same contract as the "signal" file). The
    * execd writes ordered job-control-signal records here (SIGSTOP/suspend,
    * SIGCONT/resume, arbitrary signals); the shepherd polls and
    * forwards them in order - non-coalescing, unlike the truncating
    * "signal"-file + SIGTTIN relay. Direct control signals
    * (SIGKILL/SIGTSTP, SIGTTOU, SIGUSR1, SIGUSR2) are NOT routed here. Best-effort: on failure the execd falls back
    * to the legacy signal path (back-compat / safety). */
   if (mkfifo("signal_pipe", 0600) != 0 && errno != EEXIST) {
      ERROR("mkfifo(signal_pipe) failed: %s - falling back to legacy signal path\n", strerror(errno));
   }

   {

      /*
       * Add the signals to the set of blocked signals. Save previous signal
       * mask. We do this before the fork() to avoid any race conditions with
       * signals being immediately sent to the shepherd after the fork.
       * After the fork we  need to restore the oldsignal mask in the execd
       */
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGTTOU);
      sigaddset(&sigset, SIGTTIN);
      sigaddset(&sigset, SIGUSR1);
      sigaddset(&sigset, SIGUSR2);
      sigaddset(&sigset, SIGCONT);
      sigaddset(&sigset, SIGWINCH);
      sigaddset(&sigset, SIGTSTP);
      sigprocmask(SIG_BLOCK, &sigset, &sigset_oset);
   }

   /* now fork and exec the shepherd */
   if (getenv("SGE_FAILURE_BEFORE_FORK")) {
      i = -1;
   } else {
#if defined(SOLARIS)
      i = sge_smf_contract_fork(err_str, err_length);
      if (i == -4) {
          /* Could not load libcontract or libscf */
          sge_exit(1);
      } else if (i < -1) {
          i = -2; /* Disable queue */
      }
#else
      i = fork();
#endif
   }

   if (i != 0) { /* parent or -1 */
      sigprocmask(SIG_SETMASK, &sigset_oset, nullptr);

      if (petep == nullptr) {
         /* nothing to be done for petasks: We do not signal individual petasks, but always the whole jatask */
         jep.set<JB_hard_wallclock_gmt>(0); /* in case we are restarting! */
         jatep.set<JAT_pending_signal>(0);
         jatep.set<JAT_pending_signal_delivery_time>(0);
      }

      if (chdir(execd_spool_dir))       /* go back */
         /* if this happens (don't know how) we have a real problem */
         ERROR(MSG_FILE_CHDIR_SS, execd_spool_dir, strerror(errno));
      if (i == -1) {
         if (getenv("SGE_FAILURE_BEFORE_FORK")) {
            snprintf(err_str, err_length, "FAILURE_BEFORE_FORK");
         } else {
            snprintf(err_str, err_length, MSG_EXECD_NOFORK_S, strerror(errno));
         }
      }

      DRETURN(i);
   }

   // If we are running under systemd control,
   // we need to move the shepherd out of the execd scope.
   // We create a new scope for the shepherd, e.g. "ocs-shepherd.scope".
   // The shepherd process may *not* run within the scope of running a job - it might have limits which would lead
   // to sge_shepherd being killed.
   // Moving the shepherd pid to a new scope must be done here (in the child process), otherwise the shepherd
   // might already have forked and have started job processes within the execd scope.
   // The shepherd scope is automatically deleted again when the last shepherd process exits.
   // Therefore, we need to create a new scope when the first shepherd process is started,
   // and just attach the following shepherd processes to this scope.
#if defined (OCS_WITH_SYSTEMD)
   starting_shepherd_ok = ocs::execd::execd_move_shepherd_to_scope();
#endif

   if (starting_shepherd_ok) {  /* close all fd's except 0,1,2 */
      int keep_open[3];

      keep_open[0] = 0;
      keep_open[1] = 1;
      keep_open[2] = 2;
      sge_close_all_fds(keep_open, 3);
   }

   if (starting_shepherd_ok) {
      DPRINTF("**********************CHILD*********************\n");
      shepherd_name = SGE_SHEPHERD;
      snprintf(ps_name, sizeof(ps_name), "%s-" sge_u32, shepherd_name, job_id);

      pag_cmd = mconf_get_pag_cmd();
      shepherd_cmd = mconf_get_shepherd_cmd();
      if (shepherd_cmd && strlen(shepherd_cmd) &&
          strcasecmp(shepherd_cmd, "none")) {
         DPRINTF("CHILD - About to exec shepherd wrapper job ->%s< under queue -<%s<\n",
                 jep.get<JB_job_name>(),
                 master_q.get<QU_full_name>());
         if (ISTRACE) {
            execlp(shepherd_cmd, ps_name, nullptr);
         } else {
            execlp(shepherd_cmd, ps_name, "-bg", nullptr);
         }
      } else if (!ocs::Bootstrap::has_security_mode(ocs::Bootstrap::BS_SEC_MODE_AFS) || !pag_cmd ||
                 !strlen(pag_cmd) || !strcasecmp(pag_cmd, "none")) {
         DPRINTF("CHILD - About to exec ->%s< under queue -<%s<\n",
                 jep.get<JB_job_name>(),
                 master_q.get<QU_full_name>());

         if (ISTRACE) {
            execlp(shepherd_path, ps_name, nullptr);
         } else {
            execlp(shepherd_path, ps_name, "-bg", nullptr);
         }
      } else {
         char commandline[2048];

         DPRINTF("CHILD - About to exec PAG command job ->%s< under queue -<%s<\n",
                 jep.get<JB_job_name>(), master_q.get<QU_full_name>());
         if (ISTRACE) {
            snprintf(commandline, sizeof(commandline), "exec %s", shepherd_path);
         } else {
            snprintf(commandline, sizeof(commandline), "exec %s -bg", shepherd_path);
         }

         execlp(pag_cmd, pag_cmd, "-c", commandline, nullptr);
      }
      sge_free(&pag_cmd);
      sge_free(&shepherd_cmd);
   }

   /*---------------------------------------------------*/
   /* exec() failed - do what shepherd does if it fails */
   // starting_shepherd_ok is false only when the move into the shepherds scope failed, which
   // is a systemd problem and not a failing exec. Report it as such, so that the failure state
   // of the job names systemd and the host is set into error state rather than the job being
   // scheduled here over and over again.
   fp = fopen("error", "w");
   if (fp) {
      if (starting_shepherd_ok) {
         fprintf(fp, "failed to exec shepherd for job" sge_u32"\n", job_id);
      } else {
         fprintf(fp, "failed to move the shepherd of job " sge_u32 " into its systemd scope\n",
                 job_id);
      }
      FCLOSE(fp);
   }

   fp = fopen("exit_status", "w");
   if (fp) {
      fprintf(fp, "%d\n", starting_shepherd_ok ? 1 : SSTATE_SYSTEMD_ERROR);
      FCLOSE(fp);
   }

FCLOSE_ERROR:
   CRITICAL(SFNMAX, MSG_EXECD_NOSTARTSHEPHERD);

   exit(1);

   /* just to please insure */
   return -1;
}

/*****************************************************
 check whether a shell should be called as login shell
 *****************************************************/
static int ck_login_sh(char *shell) {
   DENTER(TOP_LAYER);

   char *cp;
   char *login_shells;
   int ret;

   login_shells = mconf_get_login_shells();

   if (login_shells == nullptr) {
      DRETURN(0);
   }

   cp = login_shells;

   while (*cp) {

      /* skip delimiters */
      while (*cp && (*cp == ',' || *cp == ' ' || *cp == '\t')) {
         cp++;
      }

      ret = strncmp(cp, shell, strlen(shell));
      DPRINTF("strncmp(\"%s\", \"%s\", %d) = %d\n", cp, shell, strlen(shell), ret);
      if (!ret) {
         sge_free(&login_shells);
         DRETURN(1);
      }

      /* skip name of shell, proceed until next delimiter */
      while (*cp && *cp != ',' && *cp != ' ' && *cp != '\t') {
         cp++;
      }
   }

   sge_free(&login_shells);

   DRETURN(0);
}


/**
 * @brief The number of different hosts in a granted destination identifier list
 *
 * @param gdil_orig the granted destination identifier list
 * @return the number of hosts
 */
static int
get_nhosts(const ocs::tom::ConstList<ocs::tom::JG> gdil_orig) {
   DENTER(TOP_LAYER);

   int nhosts = 0;
   const auto cache = ocs::tom::ListPtr<ocs::tom::STU>::create("");

   for (const auto ep : gdil_orig) {
      const char *hostname = ep.get<JG_qhostname>();
      if (!cache->find_first<STU_name>(hostname)) {
         nhosts++;
         // emplace_back() throws on a null key, where the free form added nothing
         if (hostname != nullptr) {
            cache->emplace_back<STU_name>(hostname);
         }
      }
   }

   DRETURN(nhosts);
}
