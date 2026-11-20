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
 * @brief Array tasks: one task of a job array
 *
 * A pending task usually has no element at all - tasks live as id ranges
 * until they are about to run. This is the object the enrolled ones get.
 *
 * @see sge_ja_task.h
 */

#include <cstring>
#include <cstdlib>
#include <cctype>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_utility.h"

#include "msg_common.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_FPET.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"

#include "sge_ja_task.h"

#include "ocs_GrantedResources.h"

/**
 * @brief Find a certain PE Task
 *
 * Find a certain PE Task with "pe_task_id" in "ja_task"
 *
 * @param ja_task JAT_Type element, may be an empty handle
 * @param pe_task_id PE task id string (e.g. "1.speedy")
 *
 * @return the PET_Type element, an empty handle if there is none
 */
ocs::tom::Element<ocs::tom::PET> ja_task_search_pe_task(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                                        const char *pe_task_id) {
   if (ja_task) {
      const auto pe_tasks = ocs::tom::get<JAT_task_list>(ja_task);

      if (pe_tasks) {
         return ocs::tom::find_first_rw<PET_id>(pe_tasks, pe_task_id);
      }
   }

   return nullptr;
}

/**
 * @brief Print task id ranges into string
 *
 * The ids of all tasks contained in 'ja_task_list' will be printed
 * into 'range_string'.
 *
 * @param ja_task_list JAT_Type list, may be an empty handle
 * @param range_string dynamic string
 */
void ja_task_list_print_to_string(const ocs::tom::ConstList<ocs::tom::JAT> ja_task_list, dstring *range_string) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::RN> range_list;

   for (const auto ja_task : ja_task_list) {
      const uint32_t tid = ocs::tom::get<JAT_task_number>(ja_task);

      range_list_insert_id(range_list, nullptr, tid);
   }
   range_list_sort_uniq_compress(*range_list, nullptr, true);
   range_list_print_to_string(*range_list, range_string, false, false, false);
   DRETURN_VOID;
}

/**
 * @brief Split off the group of equally stated tasks the first task belongs to
 *
 * All tasks of the list - not only those at its front - that share the first
 * task's `JAT_status`, `JAT_state` and `JAT_hold` are moved into a new list. Callers use this to
 * render one qstat line per group rather than per task.
 *
 * The list is freed and the handle emptied when the last group is split off,
 * as lSplit() did.
 *
 * @param[in,out] ja_task_list the list to split (JAT_Type); the moved tasks are removed
 *                             from it, it is empty once the last group was split off
 * @return the tasks that were split off, an empty handle when the list was empty
 */
ocs::tom::ListPtr<ocs::tom::JAT> ja_task_list_split_group(ocs::tom::ListPtr<ocs::tom::JAT> &ja_task_list) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::JAT> ret_list;

   if (ja_task_list) {
      const auto list = *ja_task_list;
      const auto first_task = list.front();

      if (first_task) {
         const uint32_t status = ocs::tom::get<JAT_status>(first_task);
         const uint32_t state = ocs::tom::get<JAT_state>(first_task);
         const uint32_t hold = ocs::tom::get<JAT_hold>(first_task);

         // the tasks that differ stay, the group of equal ones is taken out
         ret_list = list.split_off(ocs::tom::field<JAT_status>() != status ||
                                   ocs::tom::field<JAT_state>() != state ||
                                   ocs::tom::field<JAT_hold>() != hold);
         if (list.empty()) {
            // lSplit() freed a source list it emptied
            ja_task_list.reset();
         }
      }
   }
   DRETURN(ret_list);
}

/**
 * @brief Remember finished parallel task
 *
 * To avoid duplicate handling of finished parallel tasks (which
 * could be triggered by sge_execd sending task end reports multiple
 * times until it receives an ack from qmaster), the ja_task object
 * (JAT_Type) contains a list of finished parallel tasks.
 * ja_task_add_finished_pe_task tries to add a new parallel task to
 * this list.
 * If an entry with the given pe_task_id already exists, the function
 * returns false, else true.
 *
 * @param ja_task the ja_task to check/modify (JAT_Type)
 * @param pe_task_id the pe_task_id to check/insert
 *
 * @return error state true  - if the pe_task_id did not yet exist and could be inserted, false - if the
 *         pe_task_id already existed.
 *
 * @see #ja_task_clear_finished_pe_tasks
 */
bool ja_task_add_finished_pe_task(const ocs::tom::Element<ocs::tom::JAT> ja_task, const char *pe_task_id) {
   DENTER(TOP_LAYER);

   if (ocs::tom::find_first_in<JAT_finished_task_list, FPET_id>(ja_task, pe_task_id)) {
      INFO("already handled exit of pe task " SFQ " in ja_task " sge_u32"\n", pe_task_id,
           ocs::tom::get<JAT_task_number>(ja_task));
      DRETURN(false);
   }

   ocs::tom::emplace_back_in<JAT_finished_task_list, FPET_id>(ja_task, pe_task_id);

   DRETURN(true);
}

/**
 * @brief Clear finished task list
 *
 * A ja_task contains a list of all finished parallel tasks (see also
 * sgeobj/ja_task/ja_task_add_finished_pe_task()).
 * In certain circumstances (e.g. if a ja_task is rescheduled), it is
 * necessary to clear this list.
 * ja_task_clear_finished_pe_tasks removes the complete sublist including
 * the contained task ids.
 *
 * @param ja_task the ja_task to modify (JAT_Type)
 *
 * @return true, if the list could be cleared, false, if no list of finished pe_tasks existed.
 *
 * @see #ja_task_add_finished_pe_task
 */
bool ja_task_clear_finished_pe_tasks(const ocs::tom::Element<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   /* get list of finished pe tasks */
   const auto pe_task_list = ocs::tom::get<JAT_finished_task_list>(ja_task);
   if (!pe_task_list) {
      DPRINTF("no finished pe task list to clear in ja_task " sge_u32 "\n", ocs::tom::get<JAT_task_number>(ja_task));
      DRETURN(false);
   }

   /* if we have such a list, delete it (lSetList will free the list) */
   ocs::tom::set<JAT_finished_task_list>(ja_task, nullptr);

   DPRINTF("cleared finished pe task list in ja_task " sge_u32 "\n", ocs::tom::get<JAT_task_number>(ja_task));

   DRETURN(true);
}


/**
 * @brief Parse array task ranges
 *
 * parses a job ids with or without task ranges following this pattern:
 * Digit = '0' | '1' | ... | '9' .
 * JobId = Digit { Digit } .
 * TaskIdRange = TaskId [ '-' TaskId [  ':' Digit ] ] .
 * JobTasks = JobId [ '.' TaskIdRange ] .
 * in case of a job name, the task range has to be specified extra. This
 * will be colleced in an extra list and handed in as the arrayDefList
 *
 * The new element used to be an out-parameter; it is returned now, together
 * with the state.
 *
 * @param id_list the target list, created if the handle is empty
 * @param str_jobtask job id with task range or job name
 * @param alpp answer list
 * @param include_names true: job names are allowed
 * @param arrayDefList in case of job names, a list of array taskes (RN_Type), may be an empty handle
 *
 * @return the state, -1 no valid JobTask-Identifier, 1 everything went fine, and the new ID_Type element
 *         parsed from str_jobtask, which is an empty handle for the state -1 or when it could not be created
 *
 * @note MT-NOTE: sge_parse_jobtasks() is MT safe
 */
std::pair<int, ocs::tom::Element<ocs::tom::ID>>
sge_parse_jobtasks(ocs::tom::ListPtr<ocs::tom::ID> &id_list, const char *str_jobtask,
                   ocs::tom::ListPtr<ocs::tom::AN> *alpp, const bool include_names,
                   const ocs::tom::ConstList<ocs::tom::RN> arrayDefList) {
   DENTER(TOP_LAYER);

   char *token;
   char *job_str;
   ocs::tom::ListPtr<ocs::tom::RN> task_id_range_list;
   int ret = 1;
   ocs::tom::Element<ocs::tom::ID> idp = nullptr;

   job_str = strdup(str_jobtask);

   /* An empty job id string is a bad job id string! */
   if (strcmp(job_str, "") == 0) {
      ret = -1;
   }
      /*
      ** dup the input string for tokenizing
      */
   else if (isdigit(job_str[0])) {
      const double epsilon = 1.0E-12;
      char *end_ptr = nullptr;
      double dbl_value;
      uint32_t ulng_value;

      if ((token = strchr(job_str, '.')) != nullptr) {
         token[0] = '\0';
         token++;
         if (!range_list_parse_from_string(task_id_range_list, alpp, token,
                                           false, true, INF_NOT_ALLOWED) ||
             !task_id_range_list) {
            ret = -1;
         }
      }

      dbl_value = strtod(job_str, &end_ptr);
      ulng_value = dbl_value;

      if ((dbl_value < 1) || (dbl_value - ulng_value > epsilon) ||
          (end_ptr == nullptr) || (end_ptr[0] != '\0')) {
         ret = -1;
      }
   }

   if (arrayDefList) {
      if (!task_id_range_list) {
         task_id_range_list = arrayDefList.clone(arrayDefList.name());
      } else {
         task_id_range_list->splice(arrayDefList.clone(""));
      }
   }

   if (ret == 1) {
      if (!include_names && !isdigit(job_str[0]) && (strcmp(job_str, "\"*\"") != 0)) {
         ret = -1;
      } else {
         try {
            if (!id_list) {
               // the name and hashing ocs::tom::emplace_back() gave a list it created
               id_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
            }
            idp = id_list->emplace_back<ID_str>(job_str);
         } catch (const ocs::RuntimeException &) {
            // no memory: no element, as the soft path of ocs::tom::emplace_back() gave none
         }

         if (idp) {
            range_list_sort_uniq_compress(*task_id_range_list, alpp, true);
            ocs::tom::set<ID_ja_structure>(idp, std::move(task_id_range_list));
         }
      }
   }
   /*
   ** free the dupped string
   */
   sge_free(&job_str);
   DRETURN(std::make_pair(ret, idp));
}

/**
 * @brief Add a message to the message list of a task
 *
 * Adds a message in the "JAT_message_list"-message list of "this_elem"
 * "type" will be the message type. "message" is the text string stored
 * int the new element of the sublist.
 *
 * @param this_elem JAT_Type element
 * @param type message type id
 * @param message message
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: ja_task_message_add() is MT safe
 *
 * @see #ja_task_message_trash_all_of_type_X
 */
bool ja_task_message_add(const ocs::tom::Element<ocs::tom::JAT> this_elem, const uint32_t type, const char *message) {
   DENTER(TOP_LAYER);

   const bool ret = object_message_add<JAT_message_list>(this_elem, type, message);
   DRETURN(ret);
}


/**
 * @brief Trash messages of certain type
 *
 * Trash all messages from the sublist of JAT_message_list which are of
 * the given "type".
 *
 * @param this_elem JAT_Type element
 * @param type type id
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: ja_task_message_trash_all_of_type_X() is MT safe
 */
bool ja_task_message_trash_all_of_type_X(const ocs::tom::Element<ocs::tom::JAT> this_elem, const uint32_t type) {
   DENTER(TOP_LAYER);

   const bool ret = object_message_trash_all_of_type_X<JAT_message_list>(this_elem, type);
   DRETURN(ret);
}


/**
 * @brief Verify a ja_task object
 *
 * Verify correctness of a ja_task object.
 *
 * @param ja_task the ja_task to verify (JAT_Type)
 * @param answer_list answer list to pass back error messages
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: ja_task_verify() is MT safe
 *
 * @see #ja_task_verify_execd_job
 */
bool ja_task_verify(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   const bool ret = object_verify_ulong_not_null<JAT_task_number>(ja_task, answer_list);

   /*
    * TODO: JAT_status,
    * TODO: JAT_start_time,
    * TODO: JAT_end_time,
    * TODO: JAT_hold,
    * TODO: JAT_granted_pe,
    * TODO: JAT_job_restarted,
    * TODO: JAT_master_queue,
    * TODO: JAT_state,
    * TODO: JAT_pvm_ckpt_pid,

    * TODO: JAT_pending_signal,
    * TODO: JAT_pending_signal_delivery_time,
    * TODO: JAT_pid,
    * TODO: JAT_osjobid,
    * TODO: JAT_usage_list,

    * TODO: JAT_scaled_usage_list,
    * TODO: JAT_reported_usage_list,
    * TODO: JAT_fshare,
    * TODO: JAT_tix,
    * TODO: JAT_oticket,

    * TODO: JAT_fticket,
    * TODO: JAT_sticket,
    * TODO: JAT_share,
    * TODO: JAT_suitable,
    * TODO: JAT_task_list,
    * TODO: JAT_finished_task_list,

    * TODO: JAT_previous_usage_list,

    * TODO: JAT_pe_object,
    * TODO: JAT_next_pe_task_id,
    * TODO: JAT_stop_initiate_time,
    * TODO: JAT_prio,
    * TODO: JAT_ntix,

    * TODO: JAT_message_list
    */

   DRETURN(ret);
}

/**
 * @brief Verify a ja_task object for execd
 *
 * Verify a ja_task object that has been sent to execd.
 *
 * @param ja_task the ja_task object to verify (JAT_Type)
 * @param answer_list answer list to pass back error messages
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: ja_task_verify_execd_job() is MT safe
 *
 * @see #ja_task_verify
 */
bool ja_task_verify_execd_job(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = ja_task_verify(ja_task, answer_list);

   if (ret) {
      ret = ja_task_verify_granted_destin_identifier_list(ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task),
                                                          answer_list);
   }

   DRETURN(ret);
}

/**
 * @brief Verify granted destination identifier list
 *
 * Verify correctness of a granted destination identifier list being part
 * of a scheduled ja_task.
 *
 * @param gdil the list to verify (JG_Type); an empty handle is an error
 * @param answer_list answer list to pass back error messages
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: ja_task_verify_granted_destin_identifier_list() is MT safe
 *
 * @see #ja_task_verify_granted_destin_identifier
 */
bool ja_task_verify_granted_destin_identifier_list(const ocs::tom::ConstList<ocs::tom::JG> gdil,
                                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (!gdil) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_GDIL);
      ret = false;
   }

   if (ret) {
      for (const auto ep : gdil) {
         ret = ja_task_verify_granted_destin_identifier(ep, answer_list);
         if (!ret) {
            break;
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Verify a granted destination identifier
 *
 * Verify a single element of a granted destination identifier list
 * (a granted queue with a certain number of slots).
 *
 * @param ep the element to verify (JG_Type); an empty handle is an error
 * @param answer_list answer list to pass back error messages
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: ja_task_verify_granted_destin_identifier() is MT safe
 *
 * @see #ja_task_verify_granted_destin_identifier_list
 */
bool ja_task_verify_granted_destin_identifier(const ocs::tom::ConstElement<ocs::tom::JG> ep,
                                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (!ep) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      ret = false;
   }

   if (ret) {
      ret = qinstance_verify_full_name(answer_list, ocs::tom::get<JG_qname>(ep));
   }

   if (ret) {
      ret = verify_host_name(answer_list, ocs::tom::get<JG_qhostname>(ep));
   }

   if (ret) {
      ret = object_verify_ulong_not_null<JG_slots>(ep, answer_list);
   }

   if (ret) {
      // JG_queue names no sub-type, it holds a QU_Type object
      const ocs::tom::ConstElement<ocs::tom::QU> queue(ocs::tom::get<JG_queue>(ep));

      if (queue) {
         ret = qinstance_verify(queue, answer_list);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Is this a tightly integrated job?
 *
 * Figures out, if a running ja task belongs to a tightly integrated
 * parallel job.
 *
 * @param ja_task the ja task to test (JAT_Type), may be an empty handle
 * @param master_pe_list the parallel environments, to look up the task's PE (PE_Type)
 *
 * @return true if it is a tightly integrated parallel job, else false
 *
 * @note MT-NOTE: ja_task_is_tightly_integrated() is MT safe, the caller must
 *       hold a read lock on the SGE_TYPE_PE list.
 */
bool ja_task_is_tightly_integrated(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                   const ocs::tom::ConstList<ocs::tom::PE> master_pe_list) {
   bool ret = false;

   if (ja_task) {
      const char *pe_name = ocs::tom::get<JAT_granted_pe>(ja_task);
      if (pe_name != nullptr) {
         const auto pe = ocs::tom::ConstElement<ocs::tom::PE>::from_typed(pe_list_locate(master_pe_list, pe_name));
         if (pe) {
            if (ocs::tom::get<PE_control_slaves>(pe)) {
               ret = true;
            }
         }
      }
   }

   return ret;
}


/**
 * @brief Debit or undebit the RSMAP ids of one granted resource on a host
 *
 * @param granted_resource the granted resource (GRU_Type) whose resource map ids are booked
 * @param host the host to book on (EH_Type)
 * @param slots positive to debit, negative to undebit
 * @param[out] just_check if != nullptr do not book, only report whether it would fit
 * @return the number of modifications done
 */
static int
ja_task_debit_host_rsmap(const ocs::tom::ConstElement<ocs::tom::GRU> granted_resource,
                         const ocs::tom::Element<ocs::tom::EH> host, const int slots, bool *just_check) {
   int mods = 0;
   const char *ce_name = ocs::tom::get<GRU_name>(granted_resource);
   for (const auto resl : ocs::tom::get<GRU_resource_map_list>(granted_resource)) {
      mods += host_debit_rsmap(host, ce_name, resl, slots, just_check);
      if (just_check != nullptr && !*just_check) {
         break;
      }
   }

   return mods;
}

/**
 * @brief debit / undebit RSMAP ids of a newly scheduled ja_task
 *
 * @param granted_resources_list the resources granted to the task (GRU_Type), may be an empty handle
 * @param host  from this host (EH_Type)
 * @param slots this is the amount of slots to debit
 * @param just_check if != nullptr then do not do booking but just check if the job would fit on the resources and
 *        return the result here
 * @return the number of modifications done
 */
int ja_task_debit_host_rsmaps(const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list,
                              const ocs::tom::Element<ocs::tom::EH> host, const int slots, bool *just_check) {
   int mods = 0;
   const char *host_name = ocs::tom::get<EH_name>(host);
   const void *iterator = nullptr;

   for (auto granted_resource = ocs::tom::find_first<GRU_host>(granted_resources_list, host_name, &iterator);
        granted_resource;
        granted_resource = ocs::tom::find_next<GRU_host>(granted_resources_list, host_name, &iterator)) {

      // only debit RSMAPs here
      if (static_cast<ocs::GrantedResources::Type>(ocs::tom::get<GRU_type>(granted_resource)) !=
          ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
         continue;
      }

      mods += ja_task_debit_host_rsmap(granted_resource, host, slots, just_check);
      if (just_check != nullptr && !*just_check) {
         break;
      }
   }

   return mods;
}

/**
 * @brief Debit or undebit the core bindings of one granted resource on a host
 *
 * @param granted_resource the granted resource (GRU_Type) whose bindings are booked
 * @param host the host to book on (EH_Type)
 * @param slots positive to debit, negative to undebit
 * @param[out] just_check if != nullptr do not book, only report whether it would fit
 * @return the number of modifications done
 */
static int
ja_task_debit_host_binding(const ocs::tom::ConstElement<ocs::tom::GRU> granted_resource,
                           const ocs::tom::Element<ocs::tom::EH> host, const int slots, bool *just_check) {
   DENTER(TOP_LAYER);
   int mods = 0;
   const char *ce_name = ocs::tom::get<GRU_name>(granted_resource);
   for (const auto binding_st_elem : ocs::tom::get<GRU_binding_inuse>(granted_resource)) {
      mods += host_debit_binding(host, ce_name, binding_st_elem, slots, just_check);
      if (just_check != nullptr && !*just_check) {
         break;
      }
   }
   DRETURN(mods);
}

/**
 * @brief Debit or undebit the cores a newly scheduled task was bound to
 *
 * The counterpart of #ja_task_debit_host_rsmaps for core bindings: what is
 * booked is a topology string rather than a count.
 *
 * @param granted_resources_list the resources granted to the task (GRU_Type), may be an empty handle
 * @param host the host to book on (EH_Type)
 * @param slots positive to debit, negative to undebit
 * @param[out] just_check if != nullptr do not book, only report whether it would fit
 * @return the number of modifications done
 */
int ja_task_debit_host_bindings(const ocs::tom::ConstList<ocs::tom::GRU> granted_resources_list,
                                const ocs::tom::Element<ocs::tom::EH> host, const int slots, bool *just_check) {
   int mods = 0;
   const char *host_name = ocs::tom::get<EH_name>(host);
   const void *iterator = nullptr;
   for (auto granted_resource = ocs::tom::find_first<GRU_host>(granted_resources_list, host_name, &iterator);
        granted_resource;
        granted_resource = ocs::tom::find_next<GRU_host>(granted_resources_list, host_name, &iterator)) {

      // only debit binding information here
      if (static_cast<ocs::GrantedResources::Type>(ocs::tom::get<GRU_type>(granted_resource)) !=
          ocs::GrantedResources::Type::GRU_BINDING_TYPE) {
         continue;
      }

      mods += ja_task_debit_host_binding(granted_resource, host, slots, just_check);
      if (just_check != nullptr && !*just_check) {
         break;
      }
   }

   return mods;
}


/**
 * @brief Checks if a job array task is in running state
 *
 * This function determines whether a job array task is currently running by
 * checking its status flags. A task is considered running if it has either
 * the JTRANSFERING or JRUNNING status bit set.
 *
 * JTRANSFERING indicates the task is being transferred to an execution host,
 * while JRUNNING indicates the task is actively executing.
 *
 * @param[in] ja_task The job array task element (JAT_Type) to check
 *
 * @return true if the task is transferring or running, false otherwise
 *
 * @note Thread-safe: This function only reads from the passed parameter
 */
bool ja_task_is_running(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   bool is_running = false;
   if (ocs::tom::get<JAT_status>(ja_task) & JTRANSFERING ||
       ocs::tom::get<JAT_status>(ja_task) & JRUNNING) {
      is_running = true;
   }
   return is_running;
}


/**
 * @brief Record who deleted a job array task
 *
 * Stored in `JAT_deleted_by`, so it survives spooling and is available when the
 * accounting record is written. An already existing value is overwritten.
 *
 * @param[in,out] ja_task the task to mark (JAT_Type)
 * @param deleted_by the name to record
 */
void ja_task_set_deleted_by(const ocs::tom::Element<ocs::tom::JAT> ja_task, const char *deleted_by) {
   ocs::tom::set<JAT_deleted_by>(ja_task, deleted_by);
}


/**
 * @brief Who deleted this job array task?
 *
 * @param ja_task the task to ask about (JAT_Type)
 * @return the recorded name, or nullptr when the task was not deleted through
 *         this mechanism
 */
const char *ja_task_get_deleted_by(const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   return ocs::tom::get<JAT_deleted_by>(ja_task);
}

