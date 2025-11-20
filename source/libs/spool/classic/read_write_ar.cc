/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <filesystem>

#include "cull/cull_file.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_log.h"

#include "msg_spoollib_classic.h"
#include "read_write_ar.h"

namespace ocs::spool::classic {
   /**
    * @brief Reads an advance reservation from a spool file
    *
    * @param ar_id The ID of the advance reservation to read
    * @return Pointer to the advance reservation list element, or nullptr if reading failed
    */
   lListElem *ar_read_spool_file(u_long32 ar_id) {
      DENTER(TOP_LAYER);

      std::string filename = std::to_string(ar_id);
      lListElem *ar = lReadElemFromDisk(AR_DIR, filename.c_str(), AR_Type, SGE_OBJ_AR);

      DRETURN(ar);
   }

   /**
    * @brief Writes an advance reservation to a spool file
    *
    * @param ar Pointer to the advance reservation list element to write
    * @return true if writing succeeded, false otherwise
    */
   bool ar_write_spool_file(const lListElem *ar) {
      DENTER(TOP_LAYER);

      bool ret = ar != nullptr;

      if (ret) {
         std::string filename = std::to_string(lGetUlong(ar, AR_id));
         if (lWriteElemToDisk(ar, AR_DIR, filename.c_str(), SGE_OBJ_AR) != 0) {
            ret = false;
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Reads all advance reservations from disk and validates them
    *
    * @param ar_list Pointer to the list pointer where advance reservations will be stored
    * @param list_name Name for the created list
    * @return true if all advance reservations were successfully read and validated, false otherwise
    */
   bool ar_list_read_from_disk(lList **ar_list, const char *list_name) {
      DENTER(TOP_LAYER);

      bool ret = true;

      *ar_list = lCreateList(list_name, AR_Type);
      if (*ar_list == nullptr) {
         ret = false;
      }

      if (ret) {
         const lList *master_centry_list = *DataStore::get_master_list(SGE_TYPE_CENTRY);
         const lList *master_ckpt_list = *DataStore::get_master_list(SGE_TYPE_CKPT);
         const lList *master_cqueue_list = *DataStore::get_master_list(SGE_TYPE_CQUEUE);
         const lList *master_hgroup_list = *DataStore::get_master_list(SGE_TYPE_HGROUP);
         const lList *master_pe_list = *DataStore::get_master_list(SGE_TYPE_PE);
         const lList *master_userset_list = *DataStore::get_master_list(SGE_TYPE_USERSET);
         lList *answer_list = nullptr;

         try {
            std::filesystem::directory_iterator iter{AR_DIR};
            for (const auto &entry : iter) {
               lListElem *ar = lReadElemFromDisk(nullptr, entry.path().c_str(), AR_Type, SGE_OBJ_AR);
               if (ar != nullptr) {
                  if (ar_validate(ar, &answer_list, true, true, master_cqueue_list, master_hgroup_list,
                                  master_centry_list, master_ckpt_list, master_pe_list, master_userset_list)) {
                     lAppendElem(*ar_list, ar);
                                  } else {
                                     answer_list_output(&answer_list);
                                     lFreeElem(&ar);
                                     ret = false;
                                  }
               } else {
                  ret = false;
               }
            }
         } catch (std::filesystem::filesystem_error &e) {
            ERROR(MSG_SPOOLCLASSIC_CANNOTREADDIR_SS, AR_DIR, e.what());
            ret = false;
         }
      }

      DRETURN(ret);
   }
}
