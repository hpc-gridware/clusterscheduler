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
 * Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Collecting this host's load values and assembling the load report
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <utility>

#include "uti/sge_arch.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_uidgid.h"
#include "uti/ocs_Topo.h"

#include "comm/commlib.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_object.h"

#include "sgeobj/ocs_Version.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_LR.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_LIC.h"
#include "sgeobj/tom/ocs_tom_REP.h"

#include "job_report_execd.h"
#include "sge_load_sensor.h"
#include "load_avg.h"
#include "sge_report_execd.h"
#include "msg_execd.h"
#include "ocs_Bootstrap.h"

#ifdef COMPILE_DC
#  include "ptf.h"
#endif

#if defined(LINUX)
#   include <dlfcn.h>
#endif

static ocs::tom::ListPtr<ocs::tom::JB>
get_reserved_usage(const char *qualified_hostname);
static int 
execd_add_load_report(ocs::tom::List<ocs::tom::REP> report_list, uint64_t now, uint64_t *next_send);
static int 
execd_add_conf_report(ocs::tom::List<ocs::tom::REP> report_list, uint64_t now, uint64_t *next_send);
static int 
execd_add_license_report(ocs::tom::List<ocs::tom::REP> report_list, uint64_t now, uint64_t *next_send);
static int 
execd_add_job_report(ocs::tom::List<ocs::tom::REP> report_list, uint64_t now, uint64_t *next_send);
static int 
sge_get_loadavg(const char *qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp);

static void sge_get_topology(const char *qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp);
static void sge_get_sockets(const char *qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp);
static void sge_get_cores(const char *qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp);
static void sge_get_hwthreads(const char *qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp);

/** @brief What the execution daemon reports to qmaster, and how
 *
 * One entry per kind of report - load, configuration, processors, jobs - each
 * with the function that fills it in. Terminated by a null entry.
 */
report_source execd_report_sources[] = {
   { NUM_REP_REPORT_LOAD, execd_add_load_report , 0 },
   { NUM_REP_REPORT_CONF, execd_add_conf_report , 0 },
   { NUM_REP_REPORT_PROCESSORS, execd_add_license_report , 0 },
   { NUM_REP_REPORT_JOB, execd_add_job_report , 0 },
   { 0, nullptr }
};

lUlong sge_execd_report_seqno = 0;   ///< Sequence number of the next report, so qmaster can spot a gap
uint64_t qmrestart_time = 0;         ///< When qmaster was last seen to restart; job reports are delayed after one
static bool delay_job_reports = false;
static bool send_all = true;
static lListElem *last_lr = nullptr;
static lList *lr_list = nullptr;

extern lList *jr_list;   ///< The job reports waiting to be sent; defined in job_report_execd.cc

static bool flush_lr = false;

/** @brief When qmaster was last seen to restart
 * @return the time, or 0 when no restart has been seen
 */
uint64_t sge_get_qmrestart_time() {
   return qmrestart_time;
}

/* record qmaster restart time, need for use in delayed_reporting */
/** @brief Record that qmaster has restarted
 *
 * Job reports are held back for a while afterwards, so that a qmaster still
 * rebuilding its state is not flooded.
 *
 * @param qmr the time of the restart
 */
void sge_set_qmrestart_time(uint64_t qmr) {
   qmrestart_time = qmr;
}

/** @brief Are job reports currently being held back?
 * @return true while they are delayed
 */
bool sge_get_delay_job_reports_flag() {
   return delay_job_reports;
}

/** @brief Hold back job reports, or stop doing so
 * @param new_val true to delay them
 */
void sge_set_delay_job_reports_flag(bool new_val) {
   delay_job_reports = new_val;
}

/** @brief Is a load report due to be sent at the next opportunity?
 * @return true when one is pending
 */
bool sge_get_flush_lr_flag() {
   return flush_lr;
}

/** @brief Ask for a load report to be sent at the next opportunity
 * @param new_val true to request one
 */
void sge_set_flush_lr_flag(bool new_val) {
   flush_lr = new_val;
}

/** @brief Fold the current load values into the report being assembled
 * @param seqno the report's sequence number
 */
void execd_merge_load_report(const uint32_t seqno) {
   DENTER(TOP_LAYER);

   if (last_lr == nullptr || seqno != ocs::tom::get<REP_seqno>(last_lr)) {
      DRETURN_VOID;
   }

   // last_lr and lr_list stay raw process-lifetime statics; last_lr is only set beside an existing lr_list
   const ocs::tom::List<ocs::tom::LR> known_lr(lr_list);
   for (const auto old_lr : ocs::tom::ConstList<ocs::tom::LR>(ocs::tom::get<REP_list>(last_lr))) {
      const char *hostname = ocs::tom::get<LR_host>(old_lr);
      const char *name = ocs::tom::get<LR_name>(old_lr);
      bool found = false;

      for (const auto lr : known_lr.find_all<LR_name>(name)) {
         if (sge_hostcmp(ocs::tom::get<LR_host>(lr), hostname) == 0) {
            found = true;
            if (ocs::tom::get<LR_is_static>(old_lr) == 2) {
               known_lr.erase(lr);
            } else {
               ocs::tom::set<LR_value>(lr, ocs::tom::get<LR_value>(old_lr));
            }
            break;
         }
      }
      if (!found) {
         known_lr.push_back(old_lr.clone());
      }
   }
   ocs::tom::ElementPtr<ocs::tom::REP>::from_typed(std::exchange(last_lr, nullptr)).reset();

   DRETURN_VOID;
}

/** @brief Discard the report being assembled
 *
 * Used when qmaster restarted: the half-built report belongs to a
 * conversation that no longer exists.
 */
void execd_trash_load_report() {
   send_all = true;
}

/**
 * @brief Add the load report to the reports to send, if it is due
 *
 * Only the load values that changed since the last report are sent, unless a full report is due.
 *
 * @param report_list the reports to send (`REP`)
 * @param now the current time
 * @param next_send when the next load report is due; updated when one is added
 * @return 0
 */
static int
execd_add_load_report(const ocs::tom::List<ocs::tom::REP> report_list, const uint64_t now, uint64_t *next_send) {
   DENTER(TOP_LAYER);

   const char* qualified_hostname = component_get_qualified_hostname();
   const char* binary_path = ocs::Bootstrap::get_binary_path();

   if (*next_send <= now || sge_get_flush_lr_flag()) {
      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());
      sge_set_flush_lr_flag(false);

      /*
      ** problem: add some more error handling here
      */
      /*
      ** 1. load report
      */
      auto report = ocs::tom::ElementPtr<ocs::tom::REP>::create();
      if (send_all) {
         ocs::tom::set<REP_type>(report, NUM_REP_FULL_REPORT_LOAD);
      } else {
         ocs::tom::set<REP_type>(report, NUM_REP_REPORT_LOAD);
      }
      ocs::tom::set<REP_version>(report, ocs::Version::get_version());
      ocs::tom::set<REP_seqno>(report, sge_execd_report_seqno);
      ocs::tom::set<REP_host>(report, qualified_hostname);

      auto tmp_lr_list = sge_build_load_report(qualified_hostname, binary_path);

      if (lr_list == nullptr) {
         lr_list = tmp_lr_list->clone("").release();
         ocs::tom::ElementPtr<ocs::tom::REP>::from_typed(std::exchange(last_lr, nullptr)).reset();
         tmp_lr_list->sort<ocs::tom::asc<LR_host>>();
         ocs::tom::set<REP_list>(report, std::move(tmp_lr_list));
      } else {
         for (const auto lr : ocs::tom::List<ocs::tom::LR>(lr_list)) {
            const char *hostname = ocs::tom::get<LR_host>(lr);
            const char *name = ocs::tom::get<LR_name>(lr);
            const char *value = ocs::tom::get<LR_value>(lr);
            bool found = false;

            for (const auto ep : tmp_lr_list->find_all<LR_name>(name)) {
               DPRINTF("handling %s in execd_add_load_report\n", name);
               if (sge_hostcmp(ocs::tom::get<LR_host>(ep), hostname) == 0) {
                  /* we found the same load value in the temp list */
                  found = true;

                  if (!send_all && sge_strnullcmp(ocs::tom::get<LR_value>(ep), value) == 0) {
                     /* value hasn't changed, remove it from list */
                     tmp_lr_list->erase(ep);
                  } else {
                     DPRINTF("value %s has changed from %s to %s\n", name, value ? value : "nullptr",
                             ocs::tom::get<LR_value>(ep));
                     /* Old value is no longer valid */
                     ocs::tom::set<LR_value>(lr, nullptr);
                  }
                  break;
               }
            }

            if (!found) {
               /* the load value is no longer reported, tag is as deleted and
                  add it to the report list */
               auto del_report = lr.clone();
               ocs::tom::set<LR_is_static>(del_report, 2);
               tmp_lr_list->push_back(std::move(del_report));
            }
         }

         tmp_lr_list->sort<ocs::tom::asc<LR_host>>();
         ocs::tom::set<REP_list>(report, std::move(tmp_lr_list));
         ocs::tom::ElementPtr<ocs::tom::REP>::from_typed(std::exchange(last_lr, nullptr)).reset();
         last_lr = report->clone().release();
      }

      report_list.push_back(std::move(report));
      send_all = false;
   }

   DRETURN(0);
}

/**
 * @brief Add the report of the configuration versions to the reports to send, if it is due
 *
 * @param report_list the reports to send (`REP`)
 * @param now the current time
 * @param next_send when the next configuration report is due; updated when one is added
 * @return 0
 */
static int
execd_add_conf_report(const ocs::tom::List<ocs::tom::REP> report_list, const uint64_t now, uint64_t *next_send) {
   DENTER(TOP_LAYER);

   const char* qualified_hostname = component_get_qualified_hostname();

   if (*next_send <= now) {

      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());

      /*
      ** 2. report about the configuration versions
      ** that the exec daemon has right now
      ** order of the reports is irrelevant
      */
      auto report = ocs::tom::ElementPtr<ocs::tom::REP>::create();
      ocs::tom::set<REP_type>(report, NUM_REP_REPORT_CONF);
      ocs::tom::set<REP_version>(report, ocs::Version::get_version());
      ocs::tom::set<REP_seqno>(report, sge_execd_report_seqno);
      ocs::tom::set<REP_host>(report, qualified_hostname);
      ocs::tom::set<REP_list>(report,
         ocs::tom::ConstList<ocs::tom::CONF>(Execd_Config_List).clone("execd config list copy"));
      report_list.push_back(std::move(report));
      DPRINTF("handling conf report in execd_add_conf_report\n");
   }
   DRETURN(0);
}

/**
 * @brief Add the report of the number of processors to the reports to send, once
 *
 * @param report_list the reports to send (`REP`)
 * @param now the current time
 * @param next_send 0 as long as the report was not added; set when it is
 * @return 0
 */
static int
execd_add_license_report(const ocs::tom::List<ocs::tom::REP> report_list, const uint64_t now, uint64_t *next_send) {
   DENTER(TOP_LAYER);
   if (*next_send == 0) {
      const char* qualified_hostname = component_get_qualified_hostname();

      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());

      /*
      ** 3. license report
      */
      auto report = ocs::tom::ElementPtr<ocs::tom::REP>::create();
      ocs::tom::set<REP_type>(report, NUM_REP_REPORT_PROCESSORS);
      ocs::tom::set<REP_version>(report, ocs::Version::get_version());
      ocs::tom::set<REP_seqno>(report, sge_execd_report_seqno);
      ocs::tom::set<REP_host>(report, qualified_hostname);
      {
         
         auto lp_lic = ocs::tom::ListPtr<ocs::tom::LIC>::create("license report list");
         auto ep_lic = ocs::tom::ElementPtr<ocs::tom::LIC>::create();
         /*
         ** no error handling here cause we can just send 1 as #processors
         */
         ocs::tom::set<LIC_processors>(ep_lic,  sge_nprocs());
         ocs::tom::set<LIC_arch>(ep_lic, sge_get_arch());
         lp_lic->push_back(std::move(ep_lic));
         ocs::tom::set<REP_list>(report, std::move(lp_lic));
      }
      report_list.push_back(std::move(report));
      DPRINTF("handling license report in execd_add_license_report\n");
   }
   DRETURN(0);
}

/**
 * @brief Add the job report to the reports to send, if it is due or reports are to be flushed
 *
 * @param report_list the reports to send (`REP`)
 * @param now the current time
 * @param next_send when the next complete job report is due; updated when one is added
 * @return 0
 */
static int
execd_add_job_report(const ocs::tom::List<ocs::tom::REP> report_list, const uint64_t now, uint64_t *next_send) {
   DENTER(TOP_LAYER);

   bool do_send = false;
   bool only_flush = false;
   static uint64_t last_send = 0;
   const char* qualified_hostname = component_get_qualified_hostname();

   /* without job reports, only the periodic report is sent: an empty complete job report tells
    * sge_qmaster that no job runs on this host any more */
   // jr_list belongs to job_report_execd.cc
   const ocs::tom::List<ocs::tom::JR> job_reports(jr_list);
   if (job_reports.empty() && *next_send > now) {
      DRETURN(0);
   }

   /* if report interval expired: send all reports */
   if (*next_send <= now) {
      *next_send = now + sge_gmt32_to_gmt64(mconf_get_load_report_time());
      do_send = true;
   } else if (sge_get_flush_jr_flag()) {
      /* if we shall flush reports: send only reports marked to flush */
      do_send = true;
      only_flush = true;
   }

   /* 
    * send only one report message per second 
    * we do send empty reports - they trigger rescheduling of a job
    * to the same host on which it executed before.
    */
   if (do_send && (last_send < now)) {
      /* remember last send time */
      last_send = now;

      /* create job report */
      auto job_report = ocs::tom::ElementPtr<ocs::tom::REP>::create();
      ocs::tom::set<REP_type>(job_report, NUM_REP_REPORT_JOB);
      ocs::tom::set<REP_version>(job_report, ocs::Version::get_version());
      ocs::tom::set<REP_seqno>(job_report, sge_execd_report_seqno);
      ocs::tom::set<REP_host>(job_report, qualified_hostname);

      /* create job report list */
      auto job_report_list = ocs::tom::ListPtr<ocs::tom::JR>::create("jr");

      /* copy reports (all or only to flush) 
       * We keep job reports where the job is started via JAPI (qsub -sync or
       * DRMAA). This is done only when we reconnect after a qmaster failover
       */
      bool complete = !only_flush;
      for (const auto jr : job_reports) {
         bool delayed = sge_get_delay_job_reports_flag() && ocs::tom::get<JR_delay_report>(jr);
         if ((!only_flush || ocs::tom::get<JR_flush>(jr)) && !ocs::tom::get<JR_no_send>(jr) && !delayed) {
            job_report_list->push_back(jr.clone());
            ocs::tom::set<JR_flush>(jr, false);
         } else if (delayed) {
            // the job is on this host, but it is missing in the report
            complete = false;
         } /* else not sending report for this job */
      }
      ocs::tom::set<REP_list>(job_report, std::move(job_report_list));

      /* a complete job report contains all jobs on this host, sge_qmaster uses it to find out
       * that a job does not run here any more (reschedule unknown list), see CS-3076
       * JR_no_send is only set for pe tasks, the job report of the master task is still sent */
      ocs::tom::set<REP_complete>(job_report, complete);
     
      /* append this new job report to the report list */
      report_list.push_back(std::move(job_report));

      /* now all is sent, reset flush_jr */
      sge_set_flush_jr_flag(false);
   }
   DRETURN(0);
}

/** @brief Collect every load value this host reports
 *
 * The built-in values plus whatever the configured load sensors produce.
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param binary_path where to find the load sensor binaries
 * @return the report, owned by the caller
 */
ocs::tom::ListPtr<ocs::tom::LR>
sge_build_load_report(const char *qualified_hostname, const char *binary_path) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::LR> lp;
   int nprocs = 1;
   double load;
   const void *iterator = nullptr;

   /* 
      adding load values to the load report 
      overwrites load values that are still
      in the load list; 
      so we first validate external load values
      then we get internal load values 
      overwriting the external values

   */

   /* build up internal report list */
   sge_switch2start_user();
   sge_get_loadavg(qualified_hostname, lp);
   sge_switch2admin_user();

   /* report topology reporting */
   sge_get_topology(qualified_hostname, lp);
   sge_get_sockets(qualified_hostname, lp);
   sge_get_cores(qualified_hostname, lp);
   sge_get_hwthreads(qualified_hostname, lp);

   /* get load report from external load sensor */
   sge_ls_get(qualified_hostname, binary_path, lp);

   /* make derived load values */
   /* retrieve num procs first - we need it for all other derived load values */
   ocs::tom::ConstElement<ocs::tom::LR> ep = ocs::tom::find_first<LR_name>(*lp, LOAD_ATTR_NUM_PROC, &iterator);
   while (ep != nullptr) {
      const char *value = ocs::tom::get<LR_value>(ep);

      if (sge_hostcmp(ocs::tom::get<LR_host>(ep), qualified_hostname) == 0) {
         if (value) {
            nprocs = std::max(1, atoi(value));
         }
         break;
      }   
      ep = ocs::tom::find_next<LR_name>(*lp, LOAD_ATTR_NUM_PROC, &iterator);
   }

   /* now make the derived load values */
   ocs::tom::ConstElement<ocs::tom::LR> ep_lr = ocs::tom::find_first<LR_host>(*lp, qualified_hostname, &iterator);
   while (ep_lr != nullptr) {
      const char *name = ocs::tom::get<LR_name>(ep_lr);
      const char *value = ocs::tom::get<LR_value>(ep_lr);

      if (strcmp(name, LOAD_ATTR_LOAD_AVG) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(lp, LOAD_ATTR_NP_LOAD_AVG, (load/nprocs), qualified_hostname, nullptr);
         }
      } else if (strcmp(name, LOAD_ATTR_LOAD_SHORT) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(lp, LOAD_ATTR_NP_LOAD_SHORT, (load/nprocs), 
                                          qualified_hostname, nullptr);
         }
      } else if (strcmp(name, LOAD_ATTR_LOAD_MEDIUM) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(lp, LOAD_ATTR_NP_LOAD_MEDIUM, (load/nprocs), 
                                          qualified_hostname, nullptr);
         }
      } else if (strcmp(name, LOAD_ATTR_LOAD_LONG) == 0) {
         if (value != nullptr) {
            load = strtod(value, nullptr);
            sge_add_double2load_report(lp, LOAD_ATTR_NP_LOAD_LONG, (load/nprocs), 
                                          qualified_hostname, nullptr);
         }
      }

      ep_lr = ocs::tom::find_next<LR_host>(*lp, qualified_hostname, &iterator);
   }

   /* qmaster expects a list sorted by host */
   lp->sort<ocs::tom::asc<LR_host>>();

   DRETURN(lp);
}

/**
 * @brief Appends the amount of sockets on a Linux platform
 *
 * Appends to the given list of load values the amount of sockets on
 * Linux platform only
 *
 * @param qualified_hostname Hostname
 * @param lpp List with load values
 *
 * @note static int -
 *
 * @note MT-NOTE: sge_get_sockets() is MT safe
 */
static void sge_get_sockets(const char* qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp) {
   DENTER(TOP_LAYER);
   int sockets = get_execd_amount_of_sockets();
   sge_add_int2load_report(lpp, LOAD_ATTR_SOCKETS, sockets, qualified_hostname);
   DRETURN_VOID;
}

/**
 * @brief Appends to the given list of load values the amount of cores
 *
 * @param qualified_hostname Hostname
 * @param lpp List with load values
 *
 * @note static int - 0 if everything was ok
 *
 * @note MT-NOTE: sge_get_cores() is MT safe
 */
static void sge_get_cores(const char* qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp) {
   DENTER(TOP_LAYER);
   int cores = get_execd_amount_of_cores();
   sge_add_int2load_report(lpp, LOAD_ATTR_CORES, cores, qualified_hostname);
   DRETURN_VOID;
}

/**
 * @brief Sets the amount of threads
 *
 * Appends to the given list of load values the amount of threads.
 *
 * @param qualified_hostname name of the host
 * @param lpp list with load values
 *
 * @note static int - 0 per default
 *
 * @note MT-NOTE: sge_get_hwthreads() is MT safe
 */

static void
sge_get_hwthreads(const char* qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp) {
   DENTER(TOP_LAYER);
   int hwthreads = get_execd_amount_of_threads();
   sge_add_int2load_report(lpp, LOAD_ATTR_THREADS, hwthreads, qualified_hostname);
   DRETURN_VOID;
}

/**
 * @brief Appends the topology string of this host to the given list of load values
 *
 * Nothing is added if HWLOC is disabled or the topology cannot be determined.
 *
 * @param qualified_hostname name of the host
 * @param lpp list with load values; created if it is empty
 */
static void sge_get_topology(const char* qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp) {
   DENTER(TOP_LAYER);
   std::string topology;
   bool ret = false;

   // do not report topology if HWLOC is disabled
   if (!mconf_get_enable_hwloc()) {
      DRETURN_VOID;
   }

   // get topology string
#if defined(OCS_HWLOC)
   ret = ocs::Topo::get_new_topology(topology, true, mconf_get_enable_hwloc());
#endif

   // add topology to load report
   if (ret) {
      sge_add_str2load_report(lpp, LOAD_ATTR_TOPOLOGY, topology.c_str(), qualified_hostname);
   }
   DRETURN_VOID;
}

/**
 * @brief Appends the load averages, the architecture, the number of processors and the memory values of this
 *        host to the given list of load values
 *
 * @param qualified_hostname name of the host
 * @param lpp list with load values; created if it is empty
 * @return 0, 1 if the memory values cannot be read
 */
static int sge_get_loadavg(const char *qualified_hostname, ocs::tom::ListPtr<ocs::tom::LR> &lpp) {
   DENTER(TOP_LAYER);

   double avg[3];
   int loads;
   int nprocs; 
#ifdef SGE_LOADMEM
   sge_mem_info_t mem_info;
#endif

   loads = sge_getloadavg(avg, 3);
   nprocs = sge_nprocs();
   if (loads == -1) {
      // log warning only every 2 hours
      static uint64_t next_log = 0;
      uint64_t now = sge_get_gmt64();

      if (now >= next_log) {
         WARNING(SFNMAX, MSG_SGETEXT_NO_LOAD);
         next_log = now + sge_gmt32_to_gmt64(7200);
      }
   } else if (loads == -2) {
      static bool logged_at_startup = false;
      if (!logged_at_startup) {
         logged_at_startup = true;
         WARNING(MSG_LS_USE_EXTERNAL_LS_S, sge_get_arch());
      }
   }

   /* build a list of load values */
   if (loads >= 0) {
      DPRINTF("---> %f %f %f - %d\n", avg[0], avg[1], avg[2], (int) (avg[2] * 100.0));

      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_AVG, avg[1], qualified_hostname, nullptr);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_SHORT, avg[0], qualified_hostname, nullptr);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_MEDIUM, avg[1], qualified_hostname, nullptr);
      sge_add_double2load_report(lpp, LOAD_ATTR_LOAD_LONG, avg[2], qualified_hostname, nullptr);
   }

   /* these are some static load values */
   sge_add_str2load_report(lpp, LOAD_ATTR_ARCH, sge_get_arch(), qualified_hostname);
   sge_add_int2load_report(lpp, LOAD_ATTR_NUM_PROC, nprocs, qualified_hostname);

#ifdef SGE_LOADMEM
   /* memory load report */
   memset(&mem_info, 0, sizeof(sge_mem_info_t));
   if (sge_loadmem(&mem_info)) {
      static int mem_fail = 0;
      if (!mem_fail) {
         ERROR(SFNMAX, MSG_LOAD_NOMEMINDICES);
         mem_fail =1;
      }
      DRETURN(1);
   }

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_FREE,        mem_info.mem_free, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_FREE,       mem_info.swap_free, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_FREE,    mem_info.mem_free  + mem_info.swap_free,
   qualified_hostname,"M");

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_TOTAL,       mem_info.mem_total, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_TOTAL,      mem_info.swap_total, qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_TOTAL,   mem_info.mem_total + mem_info.swap_total,
   qualified_hostname, "M");

   sge_add_double2load_report(lpp, LOAD_ATTR_MEM_USED,        mem_info.mem_total - mem_info.mem_free,
   qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_SWAP_USED,       mem_info.swap_total - mem_info.swap_free,
   qualified_hostname, "M");
   sge_add_double2load_report(lpp, LOAD_ATTR_VIRTUAL_USED,    (mem_info.mem_total + mem_info.swap_total)- 
                                          (mem_info.mem_free  + mem_info.swap_free), qualified_hostname, "M");

#if 0
   /* identical to "virtual_free" */
   if (!getenv("SGE_MAP_LOADVALUE")) {
      sge_add_double2load_report(lpp, "s_vmem",          mem_info.mem_total + mem_info.swap_total, qualified_hostname, "M");
      sge_add_double2load_report(lpp, "h_vmem",          mem_info.mem_total + mem_info.swap_total, qualified_hostname, "M");
   }
#endif
#endif /* SGE_LOADMEM */

#ifdef SGE_LOADCPU
   /* this code can cause nasty problems
      this is a hack to work around the problem */
   {
      double cpu_percentage;

      /* sge_getcpuload() must be called multiple 
         before it can return correct load values */
      static int first_time = 1;
      if (first_time) {
         sge_getcpuload(&cpu_percentage);
         first_time = 0;
      }

      if (sge_getcpuload(&cpu_percentage) != -1) {
         sge_add_double2load_report(lpp, "cpu", cpu_percentage, qualified_hostname, nullptr);
      } else {
         static uint64_t next_log2 = 0;

         uint64_t now = sge_get_gmt64();
         if (now >= next_log2) {
            WARNING(SFNMAX, MSG_SGETEXT_NO_LOAD);
            next_log2 = now + sge_gmt32_to_gmt64(7200);
         }
      }
   }
#endif /* SGE_LOADCPU */

   DRETURN(0);
}

/** @brief Refresh the usage figures of every job running on this host
 * @param qualified_hostname this host, as the cluster knows it
 */
void update_job_usage(const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   if (mconf_get_simulate_jobs()) {
      bool enable_mem_details = mconf_get_enable_mem_details();

      // jr_list belongs to job_report_execd.cc
      for (const auto jr : ocs::tom::List<ocs::tom::JR>(jr_list)) {
         add_usage(jr, USAGE_ATTR_CPU, nullptr, 0.1);
         add_usage(jr, USAGE_ATTR_MEM, nullptr, 0.1);
         add_usage(jr, USAGE_ATTR_IO, nullptr, 0.0);
         add_usage(jr, USAGE_ATTR_IOOPS, nullptr, 0.0);
         add_usage(jr, USAGE_ATTR_IOW, nullptr, 0.0);
         add_usage(jr, USAGE_ATTR_VMEM, nullptr, 256);
         add_usage(jr, USAGE_ATTR_MAXVMEM, nullptr, 256);
         add_usage(jr, USAGE_ATTR_RSS, nullptr, 234);
         add_usage(jr, USAGE_ATTR_MAXRSS, nullptr, 234);
         if (enable_mem_details) {
            add_usage(jr, USAGE_ATTR_PSS, nullptr, 200);
            add_usage(jr, USAGE_ATTR_MAXPSS, nullptr, 200);
            add_usage(jr, USAGE_ATTR_PMEM, nullptr, 166);
            add_usage(jr, USAGE_ATTR_SMEM, nullptr, 68);
         }
      }
      DRETURN_VOID;
   }

   ocs::tom::ListPtr<ocs::tom::JB> usage_list;
#ifdef COMPILE_DC
   if (!mconf_get_sharetree_reserved_usage()) {
      int ptf_error;

      ptf_error = ptf_get_usage(usage_list);
      if (ptf_error) {
         ERROR(MSG_LOAD_NOPTFUSAGE_S, ptf_errstr(ptf_error));
         /*
            use the old usage values in job report or none
            in case this is the first call to ptf_get_usage()
            since a new job was started
         */
         DRETURN_VOID;
      }
   }
#endif

   if (mconf_get_sharetree_reserved_usage()) {
      usage_list = get_reserved_usage(qualified_hostname);
   }

   if (!usage_list) {
      DRETURN_VOID;
   }

   if (usage_list->empty()) {
      /* could be an empty list head */
      DRETURN_VOID;
   }

#ifdef COMPILE_DC
#ifdef DEBUG_DC
   ptf_show_registered_jobs();
#endif
#endif

   /* replace existing usage in the job report with the new one */
   for (const auto usage : *usage_list) {
      uint32_t job_id = ocs::tom::get<JB_job_number>(usage);

      for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(usage)) {
         uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
         /* search matching job report */
         auto jr = get_job_report(job_id, ja_task_id, nullptr);
         if (!jr) {
            /* should not happen in theory */
            ERROR("removing unreferenced job " sge_u32 "." sge_u32 " without job report from ptf", job_id ,ja_task_id );
#ifdef COMPILE_DC
            ptf_unregister_registered_job(job_id ,ja_task_id);
#endif
            continue;
         }

         /* replace cpu/mem/io/... with newer values */
         DPRINTF("---> updating job report usage for job " sge_u32 "." sge_u32 "\n", job_id, ja_task_id);
         job_report_update_from_usage_list(jr, ocs::tom::get<JAT_usage_list>(ja_task));

         for (const auto pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
            const char *pe_task_id = ocs::tom::get<PET_id>(pe_task);

            /* search matching job report */
            jr = get_job_report(job_id, ja_task_id, pe_task_id);
            if (!jr) {
               /* should not happen in theory */
               ERROR("could not find job report for job " sge_u32"." sge_u32" " "task " SFN " contained in job usage from ptf", job_id, ja_task_id, pe_task_id);
#ifdef COMPILE_DC
#ifdef DEBUG_DC
               ptf_show_registered_jobs();
#endif
#endif
               continue;
            }

            /* replace cpu/mem/io/... with newer values */
            DPRINTF("---> updating job report usage for job " sge_u32 "." sge_u32" task \"%s\"\n",
                job_id, ja_task_id, pe_task_id);
            job_report_update_from_usage_list(jr, ocs::tom::get<PET_usage>(pe_task));
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Calculate the reserved memory
 *
 * @param queue the queue instance, read only, or an empty handle
 * @param nslots the number of slots
 * @param h_nm the name of the hard resource queue limit
 * @param s_nm the name of the soft resource queue limit
 * @return the reserved memory
 */
static double
calculate_reserved_memory(const ocs::tom::ConstElement<ocs::tom::QU> queue, const int nslots, const int h_nm,
                          const int s_nm) {
   DENTER(TOP_LAYER);

   double mem = 0.0;


   if (queue) {
      double lim, h_mem_lim, s_mem_lim;
      char err_str[128];

      parse_ulong_val(&h_mem_lim, nullptr, ocs::CEntry::Type::MEM,
                      queue.get<const char *>(h_nm),
                      err_str, sizeof(err_str)-1);

      parse_ulong_val(&s_mem_lim, nullptr, ocs::CEntry::Type::MEM,
                      queue.get<const char *>(s_nm),
                      err_str, sizeof(err_str)-1);

      lim = std::min(h_mem_lim, s_mem_lim);

      /* INFINITY is mapped to DBL_MAX -> use 0; we cannot account INFINITY! */
      if (lim == DBL_MAX) {
         lim = 0.0;
      }

      mem = lim * nslots;
   }

   DRETURN(mem);
}

/**
 * @brief Build the reserved online usage of an array task or of a PE task
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param ja_task the array task
 * @param pe_task the PE task, or an empty handle for the array task itself
 * @param job_id the job
 * @param ja_task_id the array task id
 * @param pe_task_id the PE task id, or nullptr
 * @param pe the parallel environment, or an empty handle
 * @param now the current time
 *
 * @return the usage list, owned by the caller, or an empty handle if the final usage was generated already
 */
static ocs::tom::ListPtr<ocs::tom::UA>
calculate_reserved_usage(const char *qualified_hostname, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                         const ocs::tom::ConstElement<ocs::tom::PET> pe_task, const uint32_t job_id,
                         const uint32_t ja_task_id, const char *pe_task_id,
                         const ocs::tom::ConstElement<ocs::tom::PE> pe, const uint64_t now) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::UA> ul;

   /* We only build new reserved online usage,
    * when the final (acct) usage has not yet been generated.
    * Otherwise, online usage might get higher than final usage!
    */
   // the job report belongs to job_report_execd.cc
   const auto jr = ocs::tom::ConstElement<ocs::tom::JR>(get_job_report(job_id, ja_task_id, pe_task_id));
   if (!ocs::tom::find_first_in<JR_usage, UA_name>(jr, USAGE_ATTR_CPU_ACCT)) {
      double cpu{}, mem{}, maxvmem{}, maxrss{};
      double wall_clock_time;
      build_reserved_usage(now, ja_task, pe_task, &wall_clock_time, &cpu, &mem, &maxvmem, &maxrss);

      double io{}, ioops{}, iow{};

#ifdef COMPILE_DC
      {
         /* use PDC actual I/O if available */
         const auto jul = ptf_get_job_usage(job_id, ja_task_id, pe_task_id);
         if (jul) {
            io = usage_list_get_double_usage(*jul, USAGE_ATTR_IO, 0.0);
            ioops = usage_list_get_double_usage(*jul, USAGE_ATTR_IOOPS, 0.0);
            iow = usage_list_get_double_usage(*jul, USAGE_ATTR_IOW, 0.0);
         }
      }
#endif

      /* create the reserved usage list */
      ul = ocs::tom::ListPtr<ocs::tom::UA>::create("usage_list");

      usage_list_set_double_usage(*ul, USAGE_ATTR_CPU, cpu);
      usage_list_set_double_usage(*ul, USAGE_ATTR_MEM, mem);
      usage_list_set_double_usage(*ul, USAGE_ATTR_IO, io);
      usage_list_set_double_usage(*ul, USAGE_ATTR_IOOPS, ioops);
      usage_list_set_double_usage(*ul, USAGE_ATTR_IOW, iow);

      /* for reserved usage, we assume that the job always
       * consumes the maximum allowed memory (by h_vmem/s_vmem)
       */
      if (maxvmem != 0) {
         usage_list_set_double_usage(*ul, USAGE_ATTR_VMEM, maxvmem);
         usage_list_set_double_usage(*ul, USAGE_ATTR_MAXVMEM, maxvmem);
      }
      if (maxrss != 0) {
         usage_list_set_double_usage(*ul, USAGE_ATTR_RSS, maxrss);
         usage_list_set_double_usage(*ul, USAGE_ATTR_MAXRSS, maxrss);
      }
   }

   DRETURN(ul);
}

/**
 * @brief Add the reserved online usage of an array task to the usage record of its job
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param ja_task the array task
 * @param job_id the job
 * @param ja_task_id the array task id
 * @param pe the parallel environment, or an empty handle
 * @param now the current time
 * @param new_job the usage record of the job, or an empty handle to create it
 *
 * @return the usage record of the job
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
calculate_reserved_usage_ja_task(const char *qualified_hostname, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                 const uint32_t job_id, const uint32_t ja_task_id,
                                 const ocs::tom::ConstElement<ocs::tom::PE> pe, const uint64_t now,
                                 ocs::tom::ElementPtr<ocs::tom::JB> new_job) {
   DENTER(TOP_LAYER);

   /* create data structures */
   if (!new_job) {
      new_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      ocs::tom::set<JB_job_number>(new_job, job_id);
   }

   const auto new_ja_task = ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(new_job, ja_task_id);

   ocs::tom::set<JAT_usage_list>(new_ja_task, calculate_reserved_usage(qualified_hostname, ja_task, nullptr,
                                                                       job_id, ja_task_id, nullptr, pe, now));

   DRETURN(new_job);
}

/**
 * @brief Add the reserved online usage of a PE task to the usage record of its job
 *
 * @param qualified_hostname this host, as the cluster knows it
 * @param ja_task the array task
 * @param pe_task the PE task
 * @param job_id the job
 * @param ja_task_id the array task id
 * @param pe_task_id the PE task id
 * @param pe the parallel environment, or an empty handle
 * @param now the current time
 * @param new_job the usage record of the job, or an empty handle to create it
 *
 * @return the usage record of the job
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
calculate_reserved_usage_pe_task(const char *qualified_hostname, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                 const ocs::tom::ConstElement<ocs::tom::PET> pe_task, const uint32_t job_id,
                                 const uint32_t ja_task_id, const char *pe_task_id,
                                 const ocs::tom::ConstElement<ocs::tom::PE> pe, const uint64_t now,
                                 ocs::tom::ElementPtr<ocs::tom::JB> new_job) {
   DENTER(TOP_LAYER);

   /* create data structures */
   if (!new_job) {
      new_job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      ocs::tom::set<JB_job_number>(new_job, job_id);
   }

   auto new_ja_task = ocs::tom::get<JB_ja_tasks>(new_job).find_first<JAT_task_number>(ja_task_id);

   if (!new_ja_task) {
      new_ja_task = ocs::tom::emplace_back_in<JB_ja_tasks, JAT_task_number>(new_job, ja_task_id);
   }

   const auto new_pe_task = ocs::tom::emplace_back_in<JAT_task_list, PET_id>(new_ja_task, pe_task_id);

   ocs::tom::set<PET_usage>(new_pe_task, calculate_reserved_usage(qualified_hostname, ja_task, pe_task,
                                                                  job_id, ja_task_id, pe_task_id, pe, now));

   DRETURN(new_job);
}

/**
 * @brief Calculate the reserved resource usage of every job on this host
 *
 * @param qualified_hostname this host, as the cluster knows it
 *
 * @return the usage, one job element per job holding only the job number and the array tasks, owned by the caller;
 *         an empty list if no job has usage
 */
static ocs::tom::ListPtr<ocs::tom::JB>
get_reserved_usage(const char *qualified_hostname) {
   DENTER(TOP_LAYER);

   uint64_t now = sge_get_gmt64();
   const auto what = ocs::tom::what<JB_job_number, JB_ja_tasks>();
   /* JG: TODO: why use JB etc.? We only need an object containing
    * job_id, ja_task_id, pe_task_id and a usage_list.
    * Same structure is delivered from PTF.
    */

   auto temp_job_usage_list = ocs::tom::ListPtr<ocs::tom::JB>::create("JobResUsageList");

   for (const auto job : ocs::DataStore::master_list<SGE_TYPE_JOB>()) {
      uint32_t job_id = ocs::tom::get<JB_job_number>(job);
      ocs::tom::ElementPtr<ocs::tom::JB> new_job;

      for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
         uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);

         /* we need the pe to be able to calculate the number of slots used
          * as multiplication factor for usage
          */
         const auto pe = ocs::tom::get<JAT_pe_object>(ja_task);

         /* If we have a pid for the ja_task: it's either a non-parallel job
          * or the master task of a parallel job.
          * Produce a usage record for it.
          */
         if (ocs::tom::get<JAT_pid>(ja_task) != 0) {
            new_job = calculate_reserved_usage_ja_task(qualified_hostname, ja_task, job_id, ja_task_id, pe, now,
                                                       std::move(new_job));
         }

         /* If we have pe tasks (tightly integrated): Produce a usage record
          * for each of them.
          * Do not report reserved usage, if accounting_summary is activated!
          */
         if (!(pe != nullptr && ocs::tom::get<PE_accounting_summary>(pe))) {
            for (const auto pe_task : ocs::tom::get<JAT_task_list>(ja_task)) {
               const char *pe_task_id = ocs::tom::get<PET_id>(pe_task);
               new_job = calculate_reserved_usage_pe_task(qualified_hostname, ja_task, pe_task, job_id, ja_task_id,
                                                          pe_task_id, pe, now, std::move(new_job));
            }
         }
      }

      if (new_job) {
         temp_job_usage_list->push_back(std::move(new_job));
      }
   }

   // the same reduced descriptor as lWhat("%T(%I %I)", ocs::tom::JB::descriptor.e, JB_job_number, JB_ja_tasks) gave
   auto job_usage_list = temp_job_usage_list->select(what, "PtfJobUsageList");

   DRETURN(job_usage_list);
}

/**
 * @brief Add the memory a job reserved in one queue to its usage
 *
 * Sums up the memory usage (the integral of the current memory over the
 * wallclock time) and the maximum vmem and rss, assuming the job uses all
 * memory its queue limits reserve.
 *
 * @param gdil_ep the granted destination identifier element of the queue
 * @param slots the number of slots granted in the queue
 * @param wallclock the wallclock time the job ran
 * @param mem the memory usage to add to, in GB seconds
 * @param maxvmem the maximum vmem to add to
 * @param maxrss the maximum rss to add to
 */
static void
build_reserved_mem_usage(const ocs::tom::ConstElement<ocs::tom::JG> gdil_ep, const int slots, const double wallclock,
                         double *mem, double *maxvmem, double *maxrss) {
   DENTER(TOP_LAYER);

   /* 
    * sum up memory usage (integral current memory * wallclock time)
    * and maxvmem (assume it is vmem)
    */
   const ocs::tom::ConstElement<ocs::tom::QU> queue(ocs::tom::get<JG_queue>(gdil_ep));
   double vmem = calculate_reserved_memory(queue, slots, QU_h_vmem, QU_s_vmem);
   *mem += vmem * wallclock / (1024*1024*1024);
   *maxvmem += vmem;
   *maxrss += calculate_reserved_memory(queue, slots, QU_h_rss, QU_s_rss);

   DRETURN_VOID;
}

/**
 * @brief Calculate reserved usage for job or pe task
 *
 * Computes reserved usage for a job (array task) or the task of a tightly
 * integrated parallel job.
 * The following values are computed and returned via call by reference:
 * - wallclock time (current time - start time)
 * - memory usage (integral of current memory usage times wallclock time)
 *   This can only be computed if the job requests memory (h_vmem/s_vmem).
 * - maxvmem (assume the job will consume as much memory as possible (as
 *   requested by h_vmem or s_vmem).
 *
 * @param now current time
 * @param ja_task job array task
 * @param pe_task parallel task, or an empty handle for job ja task
 *
 * @param[out] wallclock returns the wallclock time
 * @param[out] cpu returns the reserved cpu usage
 * @param[out] mem returns the reserved memory (integral vmem * wallclock)
 * @param[out] maxvmem returns the maximum virtual memory used
 * @param[out] maxrss returns the maximum resident memory used
 *
 * @note MT-NOTE: build_reserved_usage() is MT safe
 */
void
build_reserved_usage(const uint64_t now, const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                     const ocs::tom::ConstElement<ocs::tom::PET> pe_task, double *wallclock, double *cpu, double *mem,
                     double *maxvmem, double *maxrss) {
   DENTER(TOP_LAYER);

   uint64_t start_time;

   if (!ja_task || wallclock == nullptr || cpu == nullptr || mem == nullptr || maxvmem == nullptr) {
      DRETURN_VOID;
   }

   /* calculate wallclock time */ 
   if (!pe_task) {
      start_time = ocs::tom::get<JAT_start_time>(ja_task);
   } else {
      start_time = ocs::tom::get<PET_start_time>(pe_task);
   }
   if (start_time > 0 && start_time < now) {
      *wallclock = sge_gmt64_to_gmt32_double(now - start_time); // reported wallclock is in seconds as double
   } else {
      *wallclock = 0;
   }

   /* if wallclock == 0, something is wrong with start_time vs. end_time,
    * and we cannot report any usage
    */
   *cpu = 0.0;
   *mem = 0.0;
   *maxvmem = 0.0;
   *maxrss = 0.0;
   if (*wallclock != 0) {
      /* 
       * compute cpu, mem and maxvmem 
       * we must take into account that we might have multiple gdil elements
       * having different settings for h_vmem!
       * mem is the integral of h_vmem * slots * wallclock
       * this computation only works, if h_vmem is *not* INFINITY (DBL_MAX)
       *
       * we have to consider different cases:
       * - ordinary sequential job (only one slot == all gdil)
       * - loosely integrated parallel job (usage computation only for master task, but all gdil)
       * - tightly integrated parallel job: usage computation done
       *   - if we want to see the accounting summary:
       *       - for the master task (all gdil, similar to loose integration)
       *   - without accounting_summary
       *       - for the master task (1 slot)
       *       - for the individual pe tasks
       */
      if (pe_task) {
         /*
          * A pe task occupies one slot,
          * we get the virtual memory information from the queue
          * it is running in.
          */
         *cpu = *wallclock;
         const char *queue_name =
            ocs::tom::get<JG_qname>(ocs::tom::get<PET_granted_destin_identifier_list>(pe_task).front());
         const auto gdil_ep =
            ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task).find_first<JG_qname>(queue_name);
         build_reserved_mem_usage(gdil_ep, 1, *wallclock, mem, maxvmem, maxrss);
      } else {
         /* compute cpu */
         int slots = 0;
         int slots_total = 0;
         const auto gdil = ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task);
         const auto pe = ocs::tom::get<JAT_pe_object>(ja_task);

         /* sequential job, or loose integration, or tight integration with accounting_summary */
         if (pe == nullptr || !ocs::tom::get<PE_control_slaves>(pe) || ocs::tom::get<PE_accounting_summary>(pe)) {
            /* account for all gdil */
            const auto master_gdil_ep = gdil.front();
            for (const auto gdil_ep : gdil) {
               slots = ocs::tom::get<JG_slots>(gdil_ep);
               /* respect job_is_first_task, only once (for the master task gdil) */
               // @todo CS-547 we cannot differentiate master task only and master task + 1 slave task
               if (pe != nullptr && gdil_ep == master_gdil_ep && !ocs::tom::get<PE_job_is_first_task>(pe)) {
                  slots++;
               }
               slots_total += slots;
               build_reserved_mem_usage(gdil_ep, slots, *wallclock, mem, maxvmem, maxrss);
            }
         } else {
            /* tightly integrated without accounting_summary, this is the master task only */
            slots_total = 1;
            build_reserved_mem_usage(gdil.front(), 1, *wallclock, mem, maxvmem, maxrss);
         }

         /* cpu is wallclock time * total number of job slots */
         *cpu = *wallclock * slots_total;
      }
   }

   DRETURN_VOID;
}
