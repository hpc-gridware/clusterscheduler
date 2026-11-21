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
 * @brief Implementation of the scheduler job information messages
 */
#include <cstdarg>
#include <utility>

#include "uti/sge_rmon_macros.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_schedd_conf.h"

#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_CCT.h"
#include "tom/ocs_List.h"

#include "schedd_message.h"
#include "schedd_monitor.h"
#include "sge_schedd_text.h"
#include "msg_schedd.h"
#include "sgeobj/sge_answer.h"

static ocs::tom::ListPtr<ocs::tom::ULNG>
schedd_mes_get_same_category_jids(lRef category, ocs::tom::ConstList<ocs::tom::JB> job_list, int ignore_category);

static lRef schedd_mes_get_category(uint32_t job_id, ocs::tom::ConstList<ocs::tom::JB> job_list);

/**
 * @brief Give every temporary message the ids of all jobs it applies to
 *
 * Each message of @p tmp_sme carries the id of one job only. It gets the ids
 * of all jobs of @p job_list that share the category of that job instead.
 *
 * @param tmp_sme the temporary message structure
 * @param job_list the jobs the ids are taken from
 * @param ignore_category if 1, every message gets the ids of all jobs of @p job_list
 */
static void schedd_mes_find_others(const ocs::tom::Element<ocs::tom::SME> tmp_sme,
                                   const ocs::tom::ConstList<ocs::tom::JB> job_list, const int ignore_category) {
   DENTER(TOP_LAYER);

   if (tmp_sme && job_list) {
      lRef category = nullptr;            /* Category pointer (void*) */
      ocs::tom::ConstList<ocs::tom::ULNG> jid_cat_list;

      /*
       * Here we have a list of message elements where each 
       * MES_job_number_list containes only one id.
       * We have to find the other jobs (jids) which have the same category.
       */
      for (auto message_elem : tmp_sme.get<SME_message_list>()) {
         auto jid_list = message_elem.get<MES_job_number_list>();
         auto jid = jid_list.front().get<ULNG_value>();
         auto jid_category = schedd_mes_get_category(jid, job_list);

         /*
          * Initilize jid_cat_list if not initialized
          * or if category differs from the last run
          */
         if (category != jid_category || ignore_category) {
            auto new_cat_list = schedd_mes_get_same_category_jids(jid_category, job_list, ignore_category);
            jid_cat_list = new_cat_list.get();
            category = jid_category;
            message_elem.set<MES_job_number_list>(std::move(new_cat_list));
         } else {
            message_elem.set<MES_job_number_list>(jid_cat_list.clone(""));
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief The category of a job
 * @param job_id the id of the job
 * @param job_list the jobs to look in
 * @return the category (`CT`) of the job, nullptr if the job is not in @p job_list
 */
static lRef schedd_mes_get_category(const uint32_t job_id, const ocs::tom::ConstList<ocs::tom::JB> job_list) {
   DENTER(TOP_LAYER);

     /* JB */
   lRef ret = nullptr;        /* Category pointer (void*) */

   auto job = job_list.find_first<JB_job_number>(job_id);
   if (job) {
      ret = job.get<JB_category>();
   }
   DRETURN(ret);
}

/**
 * @brief The ids of all jobs of a category
 * @param category the category (`CT`)
 * @param job_list the jobs
 * @param ignore_category if 1, the ids of all jobs of @p job_list
 * @return the job ids, the caller owns them; empty if there are none or @p category is nullptr
 */
static ocs::tom::ListPtr<ocs::tom::ULNG>
schedd_mes_get_same_category_jids(const lRef category, const ocs::tom::ConstList<ocs::tom::JB> job_list,
                                  const int ignore_category) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::ULNG> ret;

   if (job_list && category != nullptr) {
      for (auto job : job_list) {
         if (ignore_category || job.get<JB_category>() == category) {
            if (!ret) {
               ret = ocs::tom::ListPtr<ocs::tom::ULNG>::create("job ids");
            }
            ret->emplace_back<ULNG_value>(job.get<JB_job_number>());
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Initialize module variables
 */
void schedd_mes_initialize() {
   DENTER(TOP_LAYER);

   if (sconf_get_sme() == nullptr) {
      auto sme = ocs::tom::ElementPtr<ocs::tom::SME>::create();
      sme->set<SME_message_list>(ocs::tom::ListPtr<ocs::tom::MES>::create(""));
      sme->set<SME_global_message_list>(ocs::tom::ListPtr<ocs::tom::MES>::create(""));
      sconf_set_sme(std::move(sme));
   }

   /* prepare tmp_sme for collecting messages */
   if (sconf_get_tmp_sme() == nullptr) {
      auto tmp_sme = ocs::tom::ElementPtr<ocs::tom::SME>::create();
      tmp_sme->set<SME_message_list>(ocs::tom::ListPtr<ocs::tom::MES>::create(""));
      sconf_set_tmp_sme(std::move(tmp_sme));
   }

   sconf_set_mes_schedd_info(true);
   schedd_mes_set_logging(1);

   DRETURN_VOID;
}

/**
 * @brief Complete message elements and move them
 *
 * Each message contained in "tmp_sme" contains only
 * one job id. We have to find other jobs in "job_list" and
 * add the job ids to the list of ids contained in "tmp_sme"
 * message elements. After that we have to move all messages
 * contained in "tmp_sme" into "sme".
 * If "ignore_category" is 1 than the job category will be ignored.
 * This means that all ids of "job_list" will be added to all
 * messages contained in "tmp_sme".
 * If no category is passed in and ignore_category is false, the messages
 * are only generated for the current job, meaning, they are just copied.
 *
 * @param job_list the jobs
 * @param ignore_category if set to true, the messages with be generated for all jobs in the list
 * @param jid_category if not empty, the function uses the category to ensure, that every message is only added
 *                     per category once.
 */
void schedd_mes_commit(const ocs::tom::ConstList<ocs::tom::JB> job_list, const int ignore_category,
                       const ocs::tom::Element<ocs::tom::CT> jid_category) {
   const auto sme = sconf_get_sme();
   const auto tmp_sme = sconf_get_tmp_sme();

   if (sme && tmp_sme) {
      if (jid_category) {
         if (jid_category.get<CT_messages_added>()) {
            return;
         }
         jid_category.set<CT_messages_added>(true);
      }

      /*
       * Try to find other jobs which apply also for created message
       */
      if (jid_category || ignore_category == 1) {
         schedd_mes_find_others(tmp_sme, job_list, ignore_category);
      }

      /*
       * Tranfer all messages from tmp_sme to sme
       */
      auto tmp_sme_list =
         tmp_sme.exchange<SME_message_list>(ocs::tom::ListPtr<ocs::tom::MES>::create("job info messages"));
      sme.get<SME_message_list>().splice(std::move(tmp_sme_list));
   }
}

/**
 * @brief Free temporaryly generated messages
 *
 * Free temporaryly generated messages contained in "tmp_sme".
 */
void schedd_mes_rollback() {
   const auto tmp_sme = sconf_get_tmp_sme();

   if (tmp_sme) {
      tmp_sme.set<SME_message_list>(ocs::tom::ListPtr<ocs::tom::MES>::create("job info messages"));
   }
}

/**
 * @brief Get message structure
 *
 * Returns message structure which containes all messages.
 *
 * @param global_mes_count out: returns nr of global messages
 * @param job_mes_count out: returns nr of job messages
 *
 * @return the message structure; the caller owns it
 */
ocs::tom::ElementPtr<ocs::tom::SME> schedd_mes_obtain_package(int *global_mes_count, int *job_mes_count) {
   DENTER(TOP_LAYER);

   uint32_t schedd_job_info = sconf_get_schedd_job_info();
   const auto sme = sconf_get_sme();

   if (schedd_job_info == SCHEDD_JOB_INFO_FALSE) {
      /*
       * Temporaryly we enable schedd_job_info to add one
       * message which says that schedd_job_info is disabled. 
       */
      sconf_enable_schedd_job_info();
      schedd_mes_add_global(nullptr, false, SCHEDD_INFO_TURNEDOFF);
      sconf_disable_schedd_job_info();
   } else if (schedd_job_info == SCHEDD_JOB_INFO_JOB_LIST) {
      schedd_mes_add_global(nullptr, false, SCHEDD_INFO_JOBLIST);
   } else if (sme.get<SME_message_list>().empty() && sme.get<SME_global_message_list>().empty()) {
      schedd_mes_add_global(nullptr, false, SCHEDD_INFO_NOMESSAGE);
   }

   if (global_mes_count != nullptr) {
      *global_mes_count = static_cast<int>(sme.get<SME_global_message_list>().size());
   }

   if (job_mes_count != nullptr) {
      *job_mes_count = static_cast<int>(sme.get<SME_message_list>().size());
   }

   // the caller takes the messages over, the temporary ones are freed
   auto ret = ocs::tom::ElementPtr<ocs::tom::SME>::from_typed(sconf_get_sme());
   sconf_set_sme(nullptr);
   auto tmp_sme = ocs::tom::ElementPtr<ocs::tom::SME>::from_typed(sconf_get_tmp_sme());
   sconf_set_tmp_sme(nullptr);
   tmp_sme.reset();

   sconf_set_mes_schedd_info(false);
   schedd_mes_set_logging(0);

   DRETURN(ret);
}

/**
 * @brief Add one entry into the message structure
 *
 * During the time the scheduler trys to dispatch jobs it might
 * call this function to add messages into a temporary structure.
 * This function might be called several times. Each call
 * will add one element which containes one message describing
 * a reason, why a job can't be dispatched and the concerned jid.
 * When it is clear if the job could be dispatched or not, one of
 * following functions has to be called:
 *    schedd_mes_commit()
 *    schedd_mes_rollback()
 *
 * @param[in,out] monitor_alpp     answer list for `qalter -w v`, or nullptr
 * @param[in]     monitor_next_run whether the message also goes into the
 *                                 scheduler run log
 * @param[in]     job_id           id of the job the message is about, 0 for
 *                                 none
 * @param[in]     message_number   message id from `sge_schedd_text.h`
 * @param[in]     ...              arguments of the format string that
 *                                 sge_schedd_text() returns for
 *                                 `message_number`
 *
 * @note MT-NOTE: schedd_mes_add() is MT safe
 *
 * @see #schedd_mes_commit, #schedd_mes_rollback
 */
void schedd_mes_add(ocs::tom::ListPtr<ocs::tom::AN> *monitor_alpp, bool monitor_next_run, uint32_t job_id,
                    uint32_t message_number, ...) {
   DENTER(TOP_LAYER);

   uint32_t schedd_job_info = sconf_get_schedd_job_info();

   if (monitor_alpp != nullptr || monitor_next_run || (job_id != 0 && schedd_job_info != SCHEDD_JOB_INFO_FALSE)) {
      va_list args;
      const char *fmt;

      char msg[MAXMSGLEN];
      dstring msg_ds;
      const char *msg_str;

      fmt = sge_schedd_text(message_number);
      va_start(args, message_number);
      sge_dstring_init(&msg_ds, msg, sizeof(msg));
      msg_str = sge_dstring_vsprintf(&msg_ds, fmt, args);
      va_end(args);

      if (monitor_alpp || monitor_next_run) {
         char msg_log[MAXMSGLEN];
         dstring msg_log_ds;
         const char *msg_log_str;

         sge_dstring_init(&msg_log_ds, msg_log, sizeof(msg_log));

         if (job_id) {
            msg_log_str = sge_dstring_sprintf(&msg_log_ds, "Job " sge_u32" %s", job_id, msg_str);
         } else {
            msg_log_str = sge_dstring_sprintf(&msg_log_ds, "Your job %s", msg_str);
         }
         schedd_log(msg_log_str, monitor_alpp, monitor_next_run);
      }

      if (!monitor_alpp) {
         if (job_id && (schedd_job_info != SCHEDD_JOB_INFO_FALSE)) {
            if (sconf_get_mes_schedd_info()) {
               const auto tmp_sme = sconf_get_tmp_sme();

               if (schedd_job_info == SCHEDD_JOB_INFO_JOB_LIST) {
                  if (!sconf_is_id_in_schedd_job_info_range(job_id)) {
                     DPRINTF("Job " sge_u32 " not in scheddconf.schedd_job_info_list\n", job_id);
                     DRETURN_VOID;
                  }
               }

               auto mes = ocs::tom::ElementPtr<ocs::tom::MES>::create();
               mes->set<MES_job_number_list>(ocs::tom::ListPtr<ocs::tom::ULNG>::create("job ids"));
               mes->set<MES_message_number>(message_number);
               mes->set<MES_message>(msg_str);
               mes->emplace_back_in<MES_job_number_list, ULNG_value>(job_id);
               tmp_sme.get<SME_message_list>().push_back(std::move(mes));
            }
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Like schedd_mes_add(), but joins messages with the same id
 *
 * Jobs that fail for the same reason end up in one message carrying the list
 * of their job ids, instead of one message per job. Only the temporary
 * message list is used, not the global one.
 *
 * @param[in] monitor_next_run whether the message also goes into the
 *                             scheduler run log
 * @param[in] job_number       id of the job the message is about
 * @param[in] message_number   message id from `sge_schedd_text.h`
 * @param[in] ...              arguments of the format string that
 *                             sge_schedd_text() returns for `message_number`
 *
 * @note MT-NOTE: schedd_mes_add_join() is MT safe
 */
void schedd_mes_add_join(bool monitor_next_run, uint32_t job_number, uint32_t message_number, ...) {
   DENTER(TOP_LAYER);

   uint32_t schedd_job_info;
   int do_logging;

   schedd_job_info = sconf_get_schedd_job_info();
   do_logging = schedd_mes_get_logging();

   if (do_logging || (job_number && (schedd_job_info != SCHEDD_JOB_INFO_FALSE))) {
      va_list args;
      const char *fmt;

      char msg[MAXMSGLEN];
      dstring msg_ds;
      const char *msg_str;

      char msg_log[MAXMSGLEN];
      dstring msg_log_ds;
      const char *msg_log_str;

      sge_dstring_init(&msg_ds, msg, sizeof(msg));
      sge_dstring_init(&msg_log_ds, msg_log, sizeof(msg_log));

      fmt = sge_schedd_text(message_number);
      va_start(args, message_number);
      msg_str = sge_dstring_vsprintf(&msg_ds, fmt, args);
      va_end(args);

      if (job_number && (schedd_job_info != SCHEDD_JOB_INFO_FALSE)) {
         if (sconf_get_mes_schedd_info()) {
            const auto tmp_sme = sconf_get_tmp_sme();

            if (schedd_job_info == SCHEDD_JOB_INFO_JOB_LIST) {
               if (!sconf_is_id_in_schedd_job_info_range(job_number)) {
                  DPRINTF("Job " sge_u32 " not in scheddconf.schedd_job_info_list\n", job_number);
                  DRETURN_VOID;
               }
            }

            auto mes_list = tmp_sme.get<SME_message_list>();
            auto mes_mes = mes_list.find_first<MES_message_number>(message_number);
            if (mes_mes == nullptr) {
               auto mes = ocs::tom::ElementPtr<ocs::tom::MES>::create();
               mes->set<MES_job_number_list>(ocs::tom::ListPtr<ocs::tom::ULNG>::create("job ids"));
               mes->set<MES_message_number>(message_number);
               mes->set<MES_message>(msg_str);
               mes_mes = mes_list.push_back(std::move(mes));
            }

            mes_mes.emplace_back_in<MES_job_number_list, ULNG_value>(job_number);
         }

         if (do_logging) {
            msg_log_str = sge_dstring_sprintf(&msg_log_ds, "Job " sge_u32" %s", job_number, msg_str);
            schedd_log(msg_log_str, nullptr, monitor_next_run);
         }
      } else {
         if (do_logging) {
            if (job_number) {
               msg_log_str = sge_dstring_sprintf(&msg_log_ds, "Job " sge_u32" %s", job_number, msg_str);
            } else {
               msg_log_str = sge_dstring_sprintf(&msg_log_ds, "Your job %s", msg_str);
            }
            schedd_log(msg_log_str, nullptr, monitor_next_run);
         }
      }
   }

   DRETURN_VOID;
}


/**
 * @brief Add a global message
 *
 * Add a global message into a message structure.
 *
 * @param[in,out] monitor_alpp     answer list for `qalter -w v`, or nullptr
 * @param[in]     monitor_next_run whether the message also goes into the
 *                                 scheduler run log
 * @param[in]     message_number   message id from `sge_schedd_text.h`
 * @param[in]     ...              arguments of the format string that
 *                                 sge_schedd_text() returns for
 *                                 `message_number`
 *
 * @note MT-NOTE: schedd_mes_add_global() is MT safe
 */
void schedd_mes_add_global(ocs::tom::ListPtr<ocs::tom::AN> *monitor_alpp, bool monitor_next_run,
                           uint32_t message_number, ...) {
   DENTER(TOP_LAYER);

   if (monitor_alpp != nullptr || monitor_next_run ||
       sconf_get_schedd_job_info() != SCHEDD_JOB_INFO_FALSE) {
      va_list args;
      const char *fmt;

      char msg[MAXMSGLEN];
      dstring msg_ds;
      const char *msg_str;

      /* Create error message */
      fmt = sge_schedd_text(message_number);
      va_start(args,message_number);

      sge_dstring_init(&msg_ds, msg, sizeof(msg));
      msg_str = sge_dstring_vsprintf(&msg_ds, fmt, args);

      if (!monitor_alpp && sconf_get_schedd_job_info() != SCHEDD_JOB_INFO_FALSE) {
         const auto sme = sconf_get_sme();

         if (sme) {
            auto mes = ocs::tom::ElementPtr<ocs::tom::MES>::create();
            mes->set<MES_message_number>(message_number);
            mes->set<MES_message>(msg_str);
            sme.get<SME_global_message_list>().push_back(std::move(mes));
         }
      }

      /* Write entry into log file */
      schedd_log(msg_str, monitor_alpp, monitor_next_run);
   }

   DRETURN_VOID;
}


/**
 * @brief Gets all messages for the current job
 *
 * returns a list of all messages for the current job
 *
 * @return the message list of the current job, read only, or an empty handle
 */
ocs::tom::ConstList<ocs::tom::MES> schedd_mes_get_tmp_list() {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::MES> ret;
   const ocs::tom::ConstElement<ocs::tom::SME> tmp_sme = sconf_get_tmp_sme();

   if (tmp_sme) {
      ret = tmp_sme.get<SME_message_list>();
   }
   DRETURN(ret);
}

/**
 * @brief Sets the messages for a current job
 *
 * Takes a mesage list, changes the job number to the current job and stores
 * the list.
 *
 * @param category the category cache, which stores the list in `CCT_job_messages`; the list is taken out
 * @param job_number job number
 */
void schedd_mes_set_tmp_list(const ocs::tom::Element<ocs::tom::CCT> category, const uint32_t job_number) {
   DENTER(TOP_LAYER);
   const auto tmp_sme = sconf_get_tmp_sme();
   auto tmp_list = category.exchange<CCT_job_messages>(ocs::tom::ListPtr<ocs::tom::MES>());

   for (auto tmp_elem : tmp_list.get()) {
      tmp_elem.emplace_back_in<MES_job_number_list, ULNG_value>(job_number);
   }

   if (tmp_sme && tmp_list) {
      tmp_list->splice(tmp_sme.exchange<SME_message_list>(ocs::tom::ListPtr<ocs::tom::MES>()));
      tmp_sme.set<SME_message_list>(std::move(tmp_list));
   }

   DRETURN_VOID;
}
