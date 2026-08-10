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

#include <cinttypes>
#include "msg_qmaster.h"

/**
 * @brief Add a new entry (uniq) entry into a list
 *
 * This function creates a new CULL element for the user "susername"
 * into the "suser_list". The newly created element will be returned.
 * If an element for this user already exists than this element will
 * be returned.
 *
 * @param suser_list SU_Type list
 * @param answer_list AN_Type list
 * @param suser_name username
 *
 * @return SU_Type element or nullptr
 */
lListElem *suser_list_add(lList **suser_list, lList **answer_list,
                          const char *suser_name)
{
   lListElem *ret = nullptr;

   if (suser_list != nullptr) {
      ret = suser_list_find(*suser_list, suser_name);
      if (ret == nullptr) {
         ret = lAddElemStr(suser_list, SU_name, suser_name, SU_Type);
      }
   }
   return ret;
}

/**
 * @brief Find a user entry in a list
 *
 * This function tries to find the first entry for user "suser_name"
 * in the list "suser_list".
 *
 * @param suser_list SU_Type list
 * @param suser_name username
 *
 * @return SU_Type element pointer or nullptr
 */
lListElem *suser_list_find(const lList *suser_list, const char *suser_name)
{
   lListElem *ret = nullptr;

   if (suser_list != nullptr && suser_name != nullptr) {
      ret = lGetElemStrRW(suser_list, SU_name, suser_name);
   }
   return ret;
}

/**
 * @brief Increase the users job counter
 *
 * The job counter within "suser" will be increased by one
 *
 * @param suser SU_Type list
 */
void suser_increase_job_counter(lListElem *suser)
{
   if (suser != nullptr) {
      lAddUlong(suser, SU_jobs, 1);
   }
}

/**
 * @brief Decrease the users job counter
 *
 * The job counter within "suser" will be decreased by one
 *
 * @param suser SU_Type list
 */
void suser_decrease_job_counter(lListElem *suser)
{
   DENTER(TOP_LAYER);

   if (suser != nullptr) {
      uint32_t jobs = lGetUlong(suser, SU_jobs);
    
      if (jobs == 0) {
         ERROR(MSG_SUSERCNTISALREADYZERO_S, lGetString(suser, SU_name));
      } else {
         lAddUlong(suser, SU_jobs, -1);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Return the users job counter
 *
 * Returns the current number of jobs registed for "suser"
 *
 * @param suser SU_Type element
 *
 * @return number of jobs
 */
uint32_t suser_get_job_counter(lListElem *suser)
{
   uint32_t ret = 0;

   if (suser != nullptr) {
      ret = lGetUlong(suser, SU_jobs);
   }
   return ret;
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
 * @param job JB_Type element
 * @param max_u_jobs maximum number of allowed jobs per user
 * @param master_suser_list the submit users to count in
 *
 * @return 1 => limit would be exceeded 0 => otherwise
 */
int suser_check_new_job(const lListElem *job, uint32_t max_u_jobs, lList *master_suser_list)
{
   const char *submit_user = nullptr;
   lListElem *suser = nullptr;
   int ret = 1;

   DENTER(TOP_LAYER);
   submit_user = lGetString(job, JB_owner);
   suser = suser_list_add(&master_suser_list, nullptr, submit_user);
   if (suser != nullptr) {
      if (max_u_jobs == 0 || max_u_jobs > suser_get_job_counter(suser))
         ret = 0;
      else
         ret = 1;
   }      
   DRETURN(ret);
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
 * @param master_suser_list the submit users to look in
 * @param max_u_jobs maximum number of allowed jobs per user
 * @param force_registration force job registration
 *
 * @return 1 => limit would be exceeded 0 => otherwise
 *
 * @see `job_list_register_new_job()`
 */
int suser_register_new_job(const lListElem *job, uint32_t max_u_jobs,
                           int force_registration, lList *master_suser_list)
{
   const char *submit_user = nullptr;
   lListElem *suser = nullptr;
   int ret = 0;

   DENTER(TOP_LAYER);

   if (!force_registration) {
      ret = suser_check_new_job(job, max_u_jobs, master_suser_list);
   }
   if (ret == 0) {
      submit_user = lGetString(job, JB_owner);
      suser = suser_list_add(&master_suser_list, nullptr, submit_user);
      suser_increase_job_counter(suser);
   }

   DRETURN(ret);
}

/**
 * @brief Number of jobs for a given user
 *
 * number of jobs for a given user
 *
 * @param job JB_Type element
 * @param master_suser_list the submit users to unregister from
 *
 * @return number of jobs in the system
 */
uint32_t suser_job_count(const lListElem *job, const lList *master_suser_list)
{
   const char *submit_user = nullptr;
   lListElem *suser = nullptr;
   uint32_t ret = 0;

   DENTER(TOP_LAYER);
   submit_user = lGetString(job, JB_owner);  
   suser = suser_list_find(master_suser_list, submit_user);
   if (suser != nullptr) {
      ret = suser_get_job_counter(suser);
   }
   DRETURN(ret);
}

/**
 * @brief Unregister a job
 *
 * Decrease the jobcounter for the job owner of "job".
 *
 * @param job JB_Type element
 * @param master_suser_list the submit users to unregister from
 */
void suser_unregister_job(const lListElem *job, const lList *master_suser_list)
{
   const char *submit_user = nullptr;
   lListElem *suser = nullptr;

   DENTER(TOP_LAYER);
   submit_user = lGetString(job, JB_owner);  
   suser = suser_list_find(master_suser_list, submit_user);
   if (suser != nullptr) {
      suser_decrease_job_counter(suser);
   }
   DRETURN_VOID;
}
