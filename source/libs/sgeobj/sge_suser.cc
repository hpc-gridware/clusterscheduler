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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"

#include "basis_types.h"
#include "msg_qmaster.h"

/****** sgeobj/suser/suser_list_add() *****************************************
*  NAME
*     suser_list_add() -- Add a new entry (uniq) entry into a list 
*
*  SYNOPSIS
*     lListElem* suser_list_add(lList **suser_list, lList **answer_list, 
*                               const char *suser_name) 
*
*  FUNCTION
*     This function creates a new CULL element for the user "susername" 
*     into the "suser_list". The newly created element will be returned.
*     If an element for this user already exists than this element will
*     be returned.
*
*  INPUTS
*     lList **suser_list     - SU_Type list 
*     lList **answer_list    - AN_Type list 
*     const char *suser_name - username 
*
*  RESULT
*     lListElem* - SU_Type element or nullptr
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
******************************************************************************/
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

/****** sgeobj/suser/suser_list_find() ****************************************
*  NAME
*     suser_list_find() -- find a user entry in a list 
*
*  SYNOPSIS
*     lListElem* suser_list_find(lList *suser_list, 
*                                const char *suser_name) 
*
*  FUNCTION
*     This function tries to find the first entry for user "suser_name" 
*     in the list "suser_list".
*
*  INPUTS
*     lList *suser_list      - SU_Type list 
*     const char *suser_name - username 
*
*  RESULT
*     lListElem* - SU_Type element pointer or nullptr
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
******************************************************************************/
lListElem *suser_list_find(const lList *suser_list, const char *suser_name)
{
   lListElem *ret = nullptr;

   if (suser_list != nullptr && suser_name != nullptr) {
      ret = lGetElemStrRW(suser_list, SU_name, suser_name);
   }
   return ret;
}

/****** sgeobj/suser/suser_increase_job_counter() *****************************
*  NAME
*     suser_increase_job_counter() -- increase the users job counter 
*
*  SYNOPSIS
*     void suser_increase_job_counter(lListElem *suser) 
*
*  FUNCTION
*     The job counter within "suser" will be increased by one 
*
*  INPUTS
*     lListElem *suser - SU_Type list 
*
*  RESULT
*     void - NONE
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
******************************************************************************/
void suser_increase_job_counter(lListElem *suser)
{
   if (suser != nullptr) {
      lAddUlong(suser, SU_jobs, 1);
   }
}

/****** sgeobj/suser/suser_decrease_job_counter() *****************************
*  NAME
*     suser_decrease_job_counter() -- decrease the users job counter 
*
*  SYNOPSIS
*     void suser_decrease_job_counter(lListElem *suser) 
*
*  FUNCTION
*     The job counter within "suser" will be decreased by one 
*
*  INPUTS
*     lListElem *suser - SU_Type list 
*
*  RESULT
*     void - NONE
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
******************************************************************************/
void suser_decrease_job_counter(lListElem *suser)
{
   DENTER(TOP_LAYER);

   if (suser != nullptr) {
      u_long32 jobs = lGetUlong(suser, SU_jobs);
    
      if (jobs == 0) {
         ERROR(MSG_SUSERCNTISALREADYZERO_S, lGetString(suser, SU_name));
      } else {
         lAddUlong(suser, SU_jobs, -1);
      }
   }
   DRETURN_VOID;
}

/****** sgeobj/suser/suser_get_job_counter() **********************************
*  NAME
*     suser_get_job_counter() -- return the users job counter 
*
*  SYNOPSIS
*     u_long32 suser_get_job_counter(lListElem *suser) 
*
*  FUNCTION
*     Returns the current number of jobs registed for "suser" 
*
*  INPUTS
*     lListElem *suser - SU_Type element 
*
*  RESULT
*     u_long32 - number of jobs 
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
******************************************************************************/
u_long32 suser_get_job_counter(lListElem *suser)
{
   u_long32 ret = 0;

   if (suser != nullptr) {
      ret = lGetUlong(suser, SU_jobs);
   }
   return ret;
}

/****** sgeobj/suser/suser_check_new_job() ************************************
*  NAME
*     suser_check_new_job() -- checks, if a job can be registered
*
*  SYNOPSIS
*     int suser_check_new_job(const lListElem *job, u_long32 max_u_jobs, 
*                                int force_registration) 
*
*  FUNCTION
*     This function checks whether a new "job" would exceed the maxium
*     number of allowed jobs per user ("max_u_jobs"). JB_owner of "job" 
*     is the username which will be used by this function to compare
*     the current number of registered jobs with "max_u_jobs". If the
*     limit would be exceeded than the function will return 1 otherwise 0.
*
*  INPUTS
*     const lListElem *job   - JB_Type element 
*     u_long32 max_u_jobs    - maximum number of allowed jobs per user 
*     int force_registration - force job registration 
*
*  RESULT
*     int - 1 => limit would be exceeded
*           0 => otherwise
******************************************************************************/
int suser_check_new_job(const lListElem *job, u_long32 max_u_jobs, lList *master_suser_list)
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

/****** sgeobj/suser/suser_register_new_job() *********************************
*  NAME
*     suser_register_new_job() -- try to register a new job 
*
*  SYNOPSIS
*     int suser_register_new_job(const lListElem *job, 
*                                u_long32 max_u_jobs, 
*                                int force_registration) 
*
*  FUNCTION
*     This function checks whether a new "job" would exceed the maximum
*     number of allowed jobs per user ("max_u_jobs"). JB_owner of "job" 
*     is the username which will be used by this function to compare
*     the current number of registered jobs with "max_u_jobs". If the
*     limit would be exceeded than the function will return 1 otherwise
*     it will increase the jobcounter of the job owner and return 0.
*     In some situation it may be necessary to force the incrementation
*     of the jobcounter (reading jobs from spool area). This may be done
*     with "force_registration".
*
*  INPUTS
*     const lListElem *job   - JB_Type element 
*     u_long32 max_u_jobs    - maximum number of allowed jobs per user 
*     int force_registration - force job registration 
*
*  RESULT
*     int - 1 => limit would be exceeded
*           0 => otherwise
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
*     qmaster/job/job_list_register_new_job()
******************************************************************************/
int suser_register_new_job(const lListElem *job, u_long32 max_u_jobs,
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

/****** sgeobj/suser/suser_get_job_count() ************************************
*  NAME
*     suser_job_count() - number of jobs for a given user
*
*  SYNOPSIS
*     void suser_job_count(const lListElem *job) 
*
*  FUNCTION
*     number of jobs for a given user
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     number of jobs in the system
******************************************************************************/
u_long32 suser_job_count(const lListElem *job, const lList *master_suser_list)
{
   const char *submit_user = nullptr;
   lListElem *suser = nullptr;
   u_long32 ret = 0;

   DENTER(TOP_LAYER);
   submit_user = lGetString(job, JB_owner);  
   suser = suser_list_find(master_suser_list, submit_user);
   if (suser != nullptr) {
      ret = suser_get_job_counter(suser);
   }
   DRETURN(ret);
}

/****** sgeobj/suser/suser_unregister_job() ***********************************
*  NAME
*     suser_unregister_job() -- unregister a job 
*
*  SYNOPSIS
*     void suser_unregister_job(const lListElem *job) 
*
*  FUNCTION
*     Decrease the jobcounter for the job owner of "job".
*
*  INPUTS
*     const lListElem *job - JB_Type element 
*
*  RESULT
*     void - NONE
*
*  SEE ALSO
*     sgeobj/suser/SU_Type
******************************************************************************/
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
