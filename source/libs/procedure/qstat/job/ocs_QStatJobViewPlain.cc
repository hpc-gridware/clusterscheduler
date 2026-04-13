/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include "uti/ocs_Pattern.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ulong.h"

#include "sched/sge_schedd_text.h"

#include "qstat/ocs_QStatParameter.h"
#include "qstat/job/ocs_QStatJobViewPlain.h"
#include "qstat/msg_qstat.h"

#include "ocs_client_job.h"

void ocs::QStatJobViewPlain::report_jobs_and_reasons_with_job_request(std::ostream &os, QStatParameter &parameter,
                                                                      QStatJobModel &model) {
   DENTER(TOP_LAYER);

   /* print scheduler job information and global scheduler info */
   for_each_ep_lv(j_elem, model.jlp) {
      const uint32_t jid = lGetUlong(j_elem, JB_job_number);
      const char *owner = lGetString(j_elem, JB_owner);
      if (const bool show_job = job_is_visible(owner, model.is_manager()); !show_job) {
         DTRACE;
         continue;
      }

      os << "==============================================================\n";
      /* print job information */
      cull_show_job(os, j_elem, 0);

      /* print scheduling information */
      if (const lListElem *sme = lFirst(model.ilp); sme != nullptr) {
         int first_run = 1;

         /* global scheduling info */
         for_each_ep_lv(mes, lGetList(sme, SME_global_message_list)) {
            if (first_run) {
               os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
               first_run = 0;
            } else {
               os << "                                 ";
            }
            os << lGetString(mes, MES_message) << "\n";
         }

         /* job scheduling info */
         for_each_ep_lv(mes, lGetList(sme, SME_message_list)) {
            const lListElem *mes_jid;

            for_each_ep(mes_jid, lGetList(mes, MES_job_number_list)) {
               if (lGetUlong(mes_jid, ULNG_value) == jid) {
                  if (first_run) {
                     os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
                     first_run = 0;
                  } else {
                     os << "                                 ";
                  }
                  os << lGetString(mes, MES_message) << "\n";
               }
            }
         }
      }
   }

   DRETURN_VOID;
}

void
ocs::QStatJobViewPlain::report_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   char text[256], ltext[256];

   if (lListElem *sme = lFirstRW(model.ilp); sme != nullptr) {
      lList *mlp = nullptr;
      uint32_t last_jid = 0;
      int initialized = 0;
      uint32_t last_mid = 0;
      int ids_per_line = 0;
      int first_run = 1;
      int first_row = 1;

      /* print global scheduling info */
      for_each_ep_lv(mes, lGetList(sme, SME_global_message_list)) {
         if (first_run) {
            os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
            first_run = 0;
         } else {
            os << "                                 ";
         }
         os << lGetString(mes, MES_message) << "\n";
      }
      if (!first_run) {
         os << "\n";
      }

      first_run = 1;

      mlp = lGetListRW(sme, SME_message_list);
      lPSortList(mlp, "I+", MES_message_number);

      /*
       * Remove all jids which have more than one entry for a MES_message_number
       * After this step the MES_messages are not correct anymore
       * We do not need this messages for the summary output
       */
      lListElem *flt_msg;

      lList *new_list = lCreateList("filtered message list", MES_Type);

      lListElem *flt_nxt_msg = lFirstRW(mlp);
      while ((flt_msg = flt_nxt_msg)) {
         lListElem *flt_jid;

         flt_nxt_msg = lNextRW(flt_msg);
         int found_msg = 0;
         for_each_ep_lv(ref_msg, new_list) {
            if (lGetUlong(ref_msg, MES_message_number) == lGetUlong(flt_msg, MES_message_number)) {
               lListElem *flt_nxt_jid = lFirstRW(lGetList(flt_msg, MES_job_number_list));
               while ((flt_jid = flt_nxt_jid)) {
                  flt_nxt_jid = lNextRW(flt_jid);

                  int found_jid = 0;
                  for_each_ep_lv(ref_jid, lGetList(ref_msg, MES_job_number_list)) {
                     if (lGetUlong(ref_jid, ULNG_value) ==
                         lGetUlong(flt_jid, ULNG_value)) {
                        lRemoveElem(lGetListRW(flt_msg, MES_job_number_list), &flt_jid);
                        found_jid = 1;
                        break;
                     }
                  }
                  if (!found_jid) {
                     lDechainElem(lGetListRW(flt_msg, MES_job_number_list), flt_jid);
                     lAppendElem(lGetListRW(ref_msg, MES_job_number_list), flt_jid);
                  }
               }
               found_msg = 1;
            }
         }
         if (!found_msg) {
            lDechainElem(mlp, flt_msg);
            lAppendElem(new_list, flt_msg);
         }
         lSetList(sme, SME_message_list, new_list);
         mlp = new_list;
      }

      text[0] = 0;
      for_each_ep_lv(mes, mlp) {
         lPSortList(lGetListRW(mes, MES_job_number_list), "I+", ULNG_value);

         for_each_ep_lv(jid_ulng, lGetList(mes, MES_job_number_list)) {
            int skip = 0;
            int header = 0;
            const uint32_t mid = lGetUlong(mes, MES_message_number);
            const uint32_t jid = lGetUlong(jid_ulng, ULNG_value);

            if (initialized) {
               if (last_mid == mid && last_jid == jid) {
                  skip = 1;
               } else if (last_mid != mid) {
                  header = 1;
               }
            } else {
               initialized = 1;
               header = 1;
            }

            if (strlen(text) >= MAX_LINE_LEN || ids_per_line >= MAX_IDS_PER_LINE || header) {
               os << text;
               text[0] = 0;
               ids_per_line = 0;
               first_row = 0;
            }

            if (header) {
               if (!first_run) {
                  os << "\n\n";
               } else {
                  first_run = 0;
               }
               os << sge_schedd_text(mid + SCHEDD_INFO_OFFSET) << "\n";
               first_row = 1;
            }

            if (!skip) {
               if (ids_per_line == 0) {
                  if (first_row) {
                     strcat(text, "\t");
                  } else {
                     strcat(text, ",\n\t");
                  }
               } else {
                  strcat(text, ",\t");
               }
               snprintf(ltext, sizeof(ltext), sge_u32, jid);
               strcat(text, ltext);
               ids_per_line++;
            }

            last_jid = jid;
            last_mid = mid;
         }
      }
      if (text[0] != 0) {
         os << text << "\n";
      }
   }

   DRETURN_VOID;
}
