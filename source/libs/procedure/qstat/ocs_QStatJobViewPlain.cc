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

#include "ocs_QStatParameter.h"
#include "ocs_QStatJobViewPlain.h"
#include "ocs_client_job.h"
#include "msg_qstat.h"

void ocs::QStatJobViewPlain::report_jobs_and_reasons_with_job_request(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);

   /* print scheduler job information and global scheduler info */
   const lListElem *j_elem = nullptr;
   for_each_ep(j_elem, model.jlp) {
      u_long32 jid = lGetUlong(j_elem, JB_job_number);
      const lListElem *sme;
      const char *owner = lGetString(j_elem, JB_owner);
      bool show_job = job_is_visible(owner,  model.is_manager());
      if (!show_job) {
         DTRACE;
         continue;
      }

      os << "==============================================================\n";
      /* print job information */
      cull_show_job(os, j_elem, 0);

      /* print scheduling information */
      if ((sme = lFirst(model.ilp)) != nullptr) {
         int first_run = 1;

         if (sme) {
            const lListElem *mes;

            /* global schduling info */
            for_each_ep(mes, lGetList(sme, SME_global_message_list)) {
               if (first_run) {
                  os << MSG_SCHEDD_SCHEDULINGINFO << ":                 ";
                  first_run = 0;
               } else {
                  os << "                                 ";
               }
               os << lGetString(mes, MES_message) << "\n";
            }

            /* job scheduling info */
            for_each_ep(mes, lGetList(sme, SME_message_list)) {
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
   }

   DRETURN_VOID;
}

void
ocs::QStatJobViewPlain::report_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) {
   DENTER(TOP_LAYER);
   lList *mlp = nullptr;
   const lListElem* mes;
   int initialized = 0;
   u_long32 last_jid = 0;
   u_long32 last_mid = 0;
   char text[256], ltext[256];
   int ids_per_line = 0;
   int first_run = 1;
   int first_row = 1;
   const lListElem *jid_ulng = nullptr;

   lListElem *sme = lFirstRW(model.ilp);
   if (sme) {
      /* print global schduling info */
      first_run = 1;
      for_each_ep(mes, lGetList(sme, SME_global_message_list)) {
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
      lListElem *flt_msg, *flt_nxt_msg;
      const lListElem *ref_msg, *ref_jid;

      lList *new_list = lCreateList("filtered message list", MES_Type);

      flt_nxt_msg = lFirstRW(mlp);
      while ((flt_msg = flt_nxt_msg)) {
         lListElem *flt_jid, * flt_nxt_jid;
         int found_msg, found_jid;

         flt_nxt_msg = lNextRW(flt_msg);
         found_msg = 0;
         for_each_ep(ref_msg, new_list) {
            if (lGetUlong(ref_msg, MES_message_number) == lGetUlong(flt_msg, MES_message_number)) {
               flt_nxt_jid = lFirstRW(lGetList(flt_msg, MES_job_number_list));
               while ((flt_jid = flt_nxt_jid)) {
                  flt_nxt_jid = lNextRW(flt_jid);

                  found_jid = 0;
                  for_each_ep(ref_jid, lGetList(ref_msg, MES_job_number_list)) {
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

      text[0]=0;
      for_each_ep(mes, mlp) {
         lPSortList(lGetListRW(mes, MES_job_number_list), "I+", ULNG_value);

         for_each_ep(jid_ulng, lGetList(mes, MES_job_number_list)) {
            int skip = 0;
            int header = 0;

            u_long32 mid = lGetUlong(mes, MES_message_number);
            u_long32 jid = lGetUlong(jid_ulng, ULNG_value);

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