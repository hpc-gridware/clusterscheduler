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
 * @brief Submit users: the per user job counter behind `max_u_jobs`
 *
 * @see sge_suser.h
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include <cinttypes>
#include "msg_qmaster.h"
#include "sgeobj/sge_answer.h"

/**
 * @brief Add a new entry (uniq) entry into a list
 *
 * This function creates a new CULL element for the user "susername"
 * into the "suser_list". The newly created element will be returned.
 * If an element for this user already exists than this element will
 * be returned.
 *
 * The list is a master list, a process-lifetime global held as a raw
 * pointer; it is created if it does not exist yet.
 *
 * @param suser_list SU_Type list reference
 * @param answer_list AN_Type list; unused
 * @param suser_name username
 *
 * @return SU_Type element or an empty handle
 */
ocs::tom::Element<ocs::tom::SU> suser_list_add(lList **suser_list,
                                               [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                               const char *suser_name) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SU> ret;

   if (suser_list != nullptr) {
      ret = suser_list_find(ocs::tom::ConstList<ocs::tom::SU>::from_typed(*suser_list), suser_name);
      if (!ret) {
         ret = ocs::tom::emplace_back<SU_name>(suser_list, suser_name);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Find a user entry in a list
 *
 * This function tries to find the first entry for user "suser_name"
 * in the list "suser_list".
 *
 * @param suser_list SU_Type list, may be an empty handle
 * @param suser_name username
 *
 * @return SU_Type element or an empty handle
 */
ocs::tom::Element<ocs::tom::SU> suser_list_find(const ocs::tom::ConstList<ocs::tom::SU> suser_list,
                                                const char *suser_name) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SU> ret;

   if (suser_list && suser_name != nullptr) {
      ret = ocs::tom::find_first_rw<SU_name>(suser_list, suser_name);
   }
   DRETURN(ret);
}

/**
 * @brief suser_list_find() for an untyped caller
 *
 * @param suser_list SU_Type list, checked; may be nullptr
 * @param suser_name username
 *
 * @return as suser_list_find()
 */
ocs::tom::Element<ocs::tom::SU> suser_list_find(const lList *suser_list, const char *suser_name) {
   return suser_list_find(ocs::tom::ConstList<ocs::tom::SU>(suser_list), suser_name);
}

/**
 * @brief Increase the users job counter
 *
 * The job counter within "suser" will be increased by one
 *
 * @param suser SU_Type element, may be an empty handle
 */
void suser_increase_job_counter(const ocs::tom::Element<ocs::tom::SU> suser) {
   DENTER(TOP_LAYER);

   if (suser) {
      ocs::tom::add<SU_jobs>(suser, 1);
   }
   DRETURN_VOID;
}

/**
 * @brief Decrease the users job counter
 *
 * The job counter within "suser" will be decreased by one
 *
 * @param suser SU_Type element, may be an empty handle
 */
void suser_decrease_job_counter(const ocs::tom::Element<ocs::tom::SU> suser) {
   DENTER(TOP_LAYER);

   if (suser) {
      const uint32_t jobs = ocs::tom::get<SU_jobs>(suser);

      if (jobs == 0) {
         ERROR(MSG_SUSERCNTISALREADYZERO_S, ocs::tom::get<SU_name>(suser));
      } else {
         ocs::tom::add<SU_jobs>(suser, -1);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Return the users job counter
 *
 * Returns the current number of jobs registed for "suser"
 *
 * @param suser SU_Type element, may be an empty handle
 *
 * @return number of jobs, 0 for an empty handle
 */
uint32_t suser_get_job_counter(const ocs::tom::ConstElement<ocs::tom::SU> suser) {
   DENTER(TOP_LAYER);

   uint32_t ret = 0;

   if (suser) {
      ret = ocs::tom::get<SU_jobs>(suser);
   }
   DRETURN(ret);
}

/**
 * @brief Find the entry of a user, adding one if the list has none
 *
 * @param master_suser_list the submit users (SU_Type), not empty
 * @param submit_user the user, not nullptr
 * @return the entry
 */
static ocs::tom::Element<ocs::tom::SU>
ocs_suser_list_find_or_add(const ocs::tom::List<ocs::tom::SU> master_suser_list, const char *submit_user) {
   DENTER(TOP_LAYER);

   auto suser = suser_list_find(master_suser_list, submit_user);
   if (!suser) {
      suser = master_suser_list.emplace_back<SU_name>(submit_user);
   }
   DRETURN(suser);
}

/**
 * @brief Checks, if a job can be registered
 *
 * This function checks whether a new "job" would exceed the maxium
 * number of allowed jobs per user ("max_u_jobs"). JB_owner of "job"
 * is the username which will be used by this function to compare
 * the current number of registered jobs with "max_u_jobs". If the
 * limit would be exceeded than the function will return 1 otherwise 0.
 *
 * A user without an entry gets one with no jobs. Without a list the user
 * counts as one with no jobs.
 *
 * @param job JB_Type element
 * @param max_u_jobs maximum number of allowed jobs per user
 * @param master_suser_list the submit users to count in (SU_Type), may be an empty handle
 *
 * @return 1 => limit would be exceeded 0 => otherwise
 */
int suser_check_new_job(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t max_u_jobs,
                        const ocs::tom::List<ocs::tom::SU> master_suser_list) {
   DENTER(TOP_LAYER);

   int ret = 1;
   const char *submit_user = ocs::tom::get<JB_owner>(job);

   if (submit_user != nullptr) {
      uint32_t jobs = 0;
      if (master_suser_list) {
         jobs = suser_get_job_counter(ocs_suser_list_find_or_add(master_suser_list, submit_user));
      }
      if (max_u_jobs == 0 || max_u_jobs > jobs) {
         ret = 0;
      } else {
         ret = 1;
      }
   }
   DRETURN(ret);
}

/**
 * @brief suser_check_new_job() for an untyped caller
 *
 * @param job JB_Type element, checked
 * @param max_u_jobs maximum number of allowed jobs per user
 * @param master_suser_list the submit users to count in (SU_Type), checked; may be nullptr
 *
 * @return as suser_check_new_job()
 */
int suser_check_new_job(const lListElem *job, uint32_t max_u_jobs, lList *master_suser_list) {
   return suser_check_new_job(ocs::tom::ConstElement<ocs::tom::JB>(job), max_u_jobs,
                              ocs::tom::List<ocs::tom::SU>(master_suser_list));
}

/**
 * @brief Try to register a new job
 *
 * This function checks whether a new "job" would exceed the maximum
 * number of allowed jobs per user ("max_u_jobs"). JB_owner of "job"
 * is the username which will be used by this function to compare
 * the current number of registered jobs with "max_u_jobs". If the
 * limit would be exceeded than the function will return 1 otherwise
 * it will increase the jobcounter of the job owner and return 0.
 * In some situation it may be necessary to force the incrementation
 * of the jobcounter (reading jobs from spool area). This may be done
 * with "force_registration".
 *
 * @param job JB_Type element
 * @param max_u_jobs maximum number of allowed jobs per user
 * @param force_registration force job registration
 * @param master_suser_list the submit users to look in (SU_Type), may be an empty handle;
 *                          then nothing is counted
 *
 * @return 1 => limit would be exceeded 0 => otherwise
 *
 * @see `job_list_register_new_job()`
 */
int suser_register_new_job(const ocs::tom::ConstElement<ocs::tom::JB> job, const uint32_t max_u_jobs,
                           const int force_registration, const ocs::tom::List<ocs::tom::SU> master_suser_list) {
   DENTER(TOP_LAYER);

   int ret = 0;

   if (!force_registration) {
      ret = suser_check_new_job(job, max_u_jobs, master_suser_list);
   }
   if (ret == 0) {
      const char *submit_user = ocs::tom::get<JB_owner>(job);
      if (master_suser_list && submit_user != nullptr) {
         suser_increase_job_counter(ocs_suser_list_find_or_add(master_suser_list, submit_user));
      }
   }

   DRETURN(ret);
}

/**
 * @brief suser_register_new_job() for an untyped caller
 *
 * @param job JB_Type element, checked
 * @param max_u_jobs maximum number of allowed jobs per user
 * @param force_registration force job registration
 * @param master_suser_list the submit users to look in (SU_Type), checked; may be nullptr
 *
 * @return as suser_register_new_job()
 */
int suser_register_new_job(const lListElem *job, uint32_t max_u_jobs, int force_registration,
                           lList *master_suser_list) {
   return suser_register_new_job(ocs::tom::ConstElement<ocs::tom::JB>(job), max_u_jobs, force_registration,
                                 ocs::tom::List<ocs::tom::SU>(master_suser_list));
}

/**
 * @brief Number of jobs for a given user
 *
 * number of jobs for a given user
 *
 * @param job JB_Type element
 * @param master_suser_list the submit users to look in (SU_Type), may be an empty handle
 *
 * @return number of jobs in the system
 */
uint32_t suser_job_count(const ocs::tom::ConstElement<ocs::tom::JB> job,
                         const ocs::tom::ConstList<ocs::tom::SU> master_suser_list) {
   DENTER(TOP_LAYER);

   const char *submit_user = ocs::tom::get<JB_owner>(job);
   const uint32_t ret = suser_get_job_counter(suser_list_find(master_suser_list, submit_user));

   DRETURN(ret);
}

/**
 * @brief suser_job_count() for an untyped caller
 *
 * @param job JB_Type element, checked
 * @param master_suser_list the submit users to look in (SU_Type), checked; may be nullptr
 *
 * @return as suser_job_count()
 */
uint32_t suser_job_count(const lListElem *job, const lList *master_suser_list) {
   return suser_job_count(ocs::tom::ConstElement<ocs::tom::JB>(job),
                          ocs::tom::ConstList<ocs::tom::SU>(master_suser_list));
}

/**
 * @brief Unregister a job
 *
 * Decrease the jobcounter for the job owner of "job".
 *
 * @param job JB_Type element
 * @param master_suser_list the submit users to unregister from (SU_Type), may be an empty handle
 */
void suser_unregister_job(const ocs::tom::ConstElement<ocs::tom::JB> job,
                          const ocs::tom::ConstList<ocs::tom::SU> master_suser_list) {
   DENTER(TOP_LAYER);

   const char *submit_user = ocs::tom::get<JB_owner>(job);
   suser_decrease_job_counter(suser_list_find(master_suser_list, submit_user));

   DRETURN_VOID;
}

/**
 * @brief suser_unregister_job() for an untyped caller
 *
 * @param job JB_Type element, checked
 * @param master_suser_list the submit users to unregister from (SU_Type), checked; may be nullptr
 */
void suser_unregister_job(const lListElem *job, const lList *master_suser_list) {
   suser_unregister_job(ocs::tom::ConstElement<ocs::tom::JB>(job),
                        ocs::tom::ConstList<ocs::tom::SU>(master_suser_list));
}
