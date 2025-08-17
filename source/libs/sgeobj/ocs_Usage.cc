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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <math.h>

#ifndef NO_SGE_COMPILE_DEBUG
#   define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_Usage.h"
#include "ocs_UserProject.h"
#include "sge_ja_task.h"
#include "sge_job.h"
#include "sge_pe_task.h"
#include "sge_schedd_conf.h"
#include "sge_usage.h"
#include "sge_userprj.h"

#include "cull/sge_eejob_SGEJ_L.h"

/** @brief Calculate decay rate and constant based on half-life
 *
 * This function calculates the decay rate and decay constant based on
 * the provided half-life in minutes. The decay rate is calculated using
 * the formula: -log(0.5) / (halftime * 60). The decay constant is then
 * calculated as 1 - (decay_rate * sge_usage_interval).
 *
 * @param halftime The half-life in minutes.
 * @param decay_rate Pointer to store the calculated decay rate.
 * @param decay_constant Pointer to store the calculated decay constant.
 */
void
ocs::Usage::calculate_decay_constant(const double halftime, double *decay_rate, double *decay_constant) {
   if (halftime < 0) {
      *decay_rate = 1.0;
      *decay_constant = 0;
   } else if (halftime == 0) {
      *decay_rate = 0;
      *decay_constant = 1.0;
   } else {
      *decay_rate = -log(0.5) / (halftime * 60);
      *decay_constant = 1 - (*decay_rate * sge_usage_interval);
   }
}

/** @brief Calculate the default decay constant based on half-life
 *
 * This function calculates the default decay constant based on the
 * provided half-life in hours. It converts the half-life to seconds
 * and then calls calculate_decay_constant to compute the decay rate
 * and constant.
 *
 * @param halftime The half-life in hours.
 */
void
ocs::Usage::calculate_default_decay_constant(const int halftime) {
   double sge_decay_rate = 0.0;
   double sge_decay_constant = 0.0;

   calculate_decay_constant(halftime * 60.0, &sge_decay_rate, &sge_decay_constant);
   sconf_set_decay_constant(sge_decay_constant);
}

void ocs::Usage::add_decay_element(lList **decay_list, double value, const char *name) {
   double decay_rate, decay_constant;
   calculate_decay_constant(value, &decay_rate, &decay_constant);
   lListElem *u = lAddElemStr(decay_list, UA_name, name, UA_Type);
   lSetDouble(u, UA_value, decay_constant);
}

/** @brief Calculate decay constants for half-life decay list
 *
 * This function calculates the decay list based on the half-life
 * and decay constants provided in the configuration.
 *
 * @return A list of decay elements with their corresponding decay constants.
 */
lList *
ocs::Usage::get_decay_list() {
   lList *decay_list = nullptr;
   lList *halflife_decay_list = sconf_get_halflife_decay_list();

   if (halflife_decay_list != nullptr) {
      const lListElem *ep = nullptr;

      for_each_rw(ep, halflife_decay_list) {
         add_decay_element(&decay_list, lGetDouble(ep, UA_value), lGetString(ep, UA_name));
      }
   } else {
      // @todo: what is the purpose of this?
      add_decay_element(&decay_list, -1, "finished_jobs");
   }
   lFreeList(&halflife_decay_list);
   return decay_list;
}

/** @brief Decay usage for the passed usage list
 *
 * This function decays the usage values in the given usage list based on
 * the decay list and the specified interval. If a decay value is not found
 * in the decay list, a default decay constant is used.
 *
 * @param usage_list The list of usage elements to be decayed.
 * @param decay_list The list of decay elements to use for decay calculations.
 * @param interval The time interval over which to apply the decay.
 */
void
ocs::Usage::decay_usage(const lList *usage_list, const lList *decay_list, const double interval) {
   if (usage_list) {
      lListElem *usage = nullptr;

      for_each_rw (usage, usage_list) {
         double decay;

         if (const lListElem *decay_elem;
             decay_list != nullptr && (decay_elem = lGetElemStr(decay_list, UA_name, lGetPosString(usage, UA_name_POS))) != nullptr) {
            decay = pow(lGetPosDouble(decay_elem, UA_value_POS), interval / sge_usage_interval);
         } else {
            decay = pow(sconf_get_decay_constant(), interval / sge_usage_interval);
         }
         lSetPosDouble(usage, UA_value_POS, lGetPosDouble(usage, UA_value_POS) * decay);
      }
   }
}


/*--------------------------------------------------------------------
 * decay_and_sum_usage - accumulates and decays usage in the correct
 * user and project objects for the specified job
 *--------------------------------------------------------------------*/

void
ocs::Usage::decay_and_sum_usage(lListElem *job, lListElem *ja_task, lListElem *node, lListElem *user, lListElem *project,
                    lList *decay_list, u_long seqno, u_long64 curr_time) {
   lList *job_usage_list=nullptr,
         *old_usage_list=nullptr,
         *user_usage_list=nullptr,
         *project_usage_list=nullptr,
         *user_long_term_usage_list=nullptr,
         *project_long_term_usage_list=nullptr;
   lListElem *userprj = nullptr,
             *petask;
   int obj_debited_job_usage = PR_debited_job_usage;

   if (!node && !user && !project) {
      return;
   }

   if (user) {
      userprj = user;
      obj_debited_job_usage = UU_debited_job_usage;
   } else if (project) {
      userprj = project;
      obj_debited_job_usage = PR_debited_job_usage;
   }

   /*-------------------------------------------------------------
    * Decay the usage for the associated user and project
    *-------------------------------------------------------------*/

   if (user) {
      ocs::UserProject::decay_userprj_usage(user, true, decay_list, seqno, curr_time);
   }

   if (project) {
      ocs::UserProject::decay_userprj_usage(project, false, decay_list, seqno, curr_time);
   }

   /*-------------------------------------------------------------
    * Note: Since SGE will update job.usage directly, we
    * maintain the job usage the last time we collected it from
    * the job.  The difference between the new usage and the old
    * usage is what needs to be added to the user or project node.
    * This old usage is maintained in the user or project node
    * depending on the type of share tree.
    *-------------------------------------------------------------*/

   if (ja_task != nullptr) {
      job_usage_list = lCopyList("", lGetList(ja_task, JAT_scaled_usage_list));

      /* sum sub-task usage into job_usage_list */
      if (job_usage_list) {
         for_each_rw(petask, lGetList(ja_task, JAT_task_list)) {
            lListElem *dst, *src;
            for_each_rw(src, lGetList(petask, PET_scaled_usage)) {
               if ((dst=lGetElemStrRW(job_usage_list, UA_name, lGetString(src, UA_name)))) {
                  lSetDouble(dst, UA_value, lGetDouble(dst, UA_value) + lGetDouble(src, UA_value));
               } else {
                  lAppendElem(job_usage_list, lCopyElem(src));
               }
            }
         }
      }
   }

   if (userprj) {
      const lListElem *upu;
      const lList *upu_list = lGetList(userprj, obj_debited_job_usage);
      if (upu_list) {
         if ((upu = lGetElemUlong(upu_list, UPU_job_number, lGetUlong(job, JB_job_number)))) {
            if ((old_usage_list = lGetListRW(upu, UPU_old_usage_list))) {
               old_usage_list = lCopyList("", old_usage_list);
            }
         }
      }
   }

   if (!old_usage_list) {
      old_usage_list = build_usage_list("old_usage_list", nullptr);
   }

   if (user) {

      /* if there is a user & project, usage is kept in the project sub-list */

      if (project) {
         lList *upp_list = lGetListRW(user, UU_project);
         lListElem *upp;
         const char *project_name = lGetString(project, PR_name);

         if (!upp_list) {
            upp_list = lCreateList("", UPP_Type);
            lSetList(user, UU_project, upp_list);
         }
         if (!((upp = lGetElemStrRW(upp_list, UPP_name, project_name))))
            upp = lAddElemStr(&upp_list, UPP_name, project_name, UPP_Type);
         user_long_term_usage_list = lGetListRW(upp, UPP_long_term_usage);
         if (!user_long_term_usage_list) {
            user_long_term_usage_list = build_usage_list("upp_long_term_usage_list", nullptr);
            lSetList(upp, UPP_long_term_usage, user_long_term_usage_list);
         }
         user_usage_list = lGetListRW(upp, UPP_usage);
         if (!user_usage_list) {
            user_usage_list = build_usage_list("upp_usage_list", nullptr);
            lSetList(upp, UPP_usage, user_usage_list);
         }

      } else {
         user_long_term_usage_list = lGetListRW(user, UU_long_term_usage);
         if (!user_long_term_usage_list) {
            user_long_term_usage_list = build_usage_list("user_long_term_usage_list", nullptr);
            lSetList(user, UU_long_term_usage, user_long_term_usage_list);
         }
         user_usage_list = lGetListRW(user, UU_usage);
         if (!user_usage_list) {
            user_usage_list = build_usage_list("user_usage_list", nullptr);
            lSetList(user, UU_usage, user_usage_list);
         }
      }
   }

   if (project) {
      project_long_term_usage_list = lGetListRW(project, PR_long_term_usage);
      if (!project_long_term_usage_list) {
         project_long_term_usage_list = build_usage_list("project_long_term_usage_list", nullptr);
         lSetList(project, PR_long_term_usage, project_long_term_usage_list);
      }
      project_usage_list = lGetListRW(project, PR_usage);
      if (!project_usage_list) {
         project_usage_list = build_usage_list("project_usage_list", nullptr);
         lSetList(project, PR_usage, project_usage_list);
      }
   }

   if (job_usage_list) {
      lListElem *job_usage;

      /*-------------------------------------------------------------
       * Add to node usage for each usage type
       *-------------------------------------------------------------*/

      for_each_rw(job_usage, job_usage_list) {

         lListElem *old_usage=nullptr,
                   *user_usage=nullptr, *project_usage=nullptr,
                   *user_long_term_usage=nullptr,
                   *project_long_term_usage=nullptr;
         const char *usage_name = lGetString(job_usage, UA_name);

         /*---------------------------------------------------------
          * Locate the corresponding usage element for the job
          * usage type in the node usage, old job usage, user usage,
          * and project usage.  If it does not exist, create a new
          * corresponding usage element.
          *---------------------------------------------------------*/

         if (old_usage_list) {
            old_usage = get_usage(old_usage_list, usage_name);
            if (!old_usage) {
               old_usage = create_usage_elem(usage_name);
               lAppendElem(old_usage_list, old_usage);
            }
         }

         if (user_usage_list) {
            user_usage = get_usage(user_usage_list, usage_name);
            if (!user_usage) {
               user_usage = create_usage_elem(usage_name);
               lAppendElem(user_usage_list, user_usage);
            }
         }

         if (user_long_term_usage_list) {
            user_long_term_usage = get_usage(user_long_term_usage_list,
                                                       usage_name);
            if (!user_long_term_usage) {
               user_long_term_usage = create_usage_elem(usage_name);
               lAppendElem(user_long_term_usage_list, user_long_term_usage);
            }
         }

         if (project_usage_list) {
            project_usage = get_usage(project_usage_list, usage_name);
            if (!project_usage) {
               project_usage = create_usage_elem(usage_name);
               lAppendElem(project_usage_list, project_usage);
            }
         }

         if (project_long_term_usage_list) {
            project_long_term_usage =
                  get_usage(project_long_term_usage_list, usage_name);
            if (!project_long_term_usage) {
               project_long_term_usage = create_usage_elem(usage_name);
               lAppendElem(project_long_term_usage_list,
			   project_long_term_usage);
            }
         }

         if (job_usage && old_usage) {

            double usage_value;

            usage_value = MAX(lGetDouble(job_usage, UA_value) -
                              lGetDouble(old_usage, UA_value), 0);

            /*---------------------------------------------------
             * Add usage to decayed user usage
             *---------------------------------------------------*/

            if (user_usage)
                lSetDouble(user_usage, UA_value,
                      lGetDouble(user_usage, UA_value) +
                      usage_value);

            /*---------------------------------------------------
             * Add usage to long term user usage
             *---------------------------------------------------*/

            if (user_long_term_usage)
                lSetDouble(user_long_term_usage, UA_value,
                      lGetDouble(user_long_term_usage, UA_value) +
                      usage_value);

            /*---------------------------------------------------
             * Add usage to decayed project usage
             *---------------------------------------------------*/

            if (project_usage)
                lSetDouble(project_usage, UA_value,
                      lGetDouble(project_usage, UA_value) +
                      usage_value);

            /*---------------------------------------------------
             * Add usage to long term project usage
             *---------------------------------------------------*/

            if (project_long_term_usage)
                lSetDouble(project_long_term_usage, UA_value,
                      lGetDouble(project_long_term_usage, UA_value) +
                      usage_value);


         }

      }

   }

   /*-------------------------------------------------------------
    * save off current job usage in debitted job usage list
    *-------------------------------------------------------------*/

   lFreeList(&old_usage_list);

   if (job_usage_list) {
      if (userprj) {
         lListElem *upu;
         u_long jobnum = lGetUlong(job, JB_job_number);
         lList *upu_list = lGetListRW(userprj, obj_debited_job_usage);
         if (!upu_list) {
            upu_list = lCreateList("", UPU_Type);
            lSetList(userprj, obj_debited_job_usage, upu_list);
         }
         if ((upu = lGetElemUlongRW(upu_list, UPU_job_number, jobnum))) {
            lSetList(upu, UPU_old_usage_list, lCopyList(lGetListName(job_usage_list), job_usage_list));
         } else {
            upu = lCreateElem(UPU_Type);
            lSetUlong(upu, UPU_job_number, jobnum);
            lSetList(upu, UPU_old_usage_list, lCopyList(lGetListName(job_usage_list), job_usage_list));
            lAppendElem(upu_list, upu);
         }
      }
      lFreeList(&job_usage_list);
   }

}


/*--------------------------------------------------------------------
 * build_usage_list - create a new usage list from an existing list
 *--------------------------------------------------------------------*/

lList *
ocs::Usage::build_usage_list(const char *name, lList *old_usage_list)
{
   lList *usage_list = nullptr;
   lListElem *usage;

   if (old_usage_list) {

      /*-------------------------------------------------------------
       * Copy the old list and zero out the usage values
       *-------------------------------------------------------------*/

      usage_list = lCopyList(name, old_usage_list);
      for_each_rw(usage, usage_list)
         lSetDouble(usage, UA_value, 0);

   } else {

      /*
       * the UA_value fields are implicitly set to 0 at creation
       * time of a new element with lCreateElem or lAddElemStr
       */

      lAddElemStr(&usage_list, UA_name, USAGE_ATTR_CPU, UA_Type);
      lAddElemStr(&usage_list, UA_name, USAGE_ATTR_MEM, UA_Type);
      lAddElemStr(&usage_list, UA_name, USAGE_ATTR_IO, UA_Type);
   }

   return usage_list;
}

/*--------------------------------------------------------------------
 * get_usage - return usage entry based on name
 *--------------------------------------------------------------------*/
lListElem *
ocs::Usage::get_usage(lList *usage_list, const char *name) {
   return lGetElemStrRW(usage_list, UA_name, name);
}


/*--------------------------------------------------------------------
 * create_usage_elem - create a new usage element
 *--------------------------------------------------------------------*/
lListElem *
ocs::Usage::create_usage_elem( const char *name ) {
   lListElem *usage = lCreateElem(UA_Type);
   lSetString(usage, UA_name, name);
   lSetDouble(usage, UA_value, 0);
   return usage;
}




