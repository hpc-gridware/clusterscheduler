/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2025-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Reading and writing the spool file of an advance reservation
 */

#include <filesystem>
#include <utility>

#include "cull/cull_file.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_log.h"

#include "msg_spoollib_classic.h"
#include "read_write_ar.h"

namespace ocs::spool::classic {
   /**
    * @brief Reads an advance reservation from a spool file
    *
    * The file holds an advance reservation (AR_Type) as lWriteElemToDisk()
    * wrote it, so it is taken without a check.
    *
    * @param ar_id The ID of the advance reservation to read
    * @return the advance reservation, or an empty handle if reading failed
    */
   ocs::tom::ElementPtr<ocs::tom::AR> ar_read_spool_file(const uint32_t ar_id) {
      DENTER(TOP_LAYER);

      std::string filename = std::to_string(ar_id);
      auto ar = ocs::tom::ElementPtr<ocs::tom::AR>::from_typed(lReadElemFromDisk(AR_DIR, filename.c_str(), AR_Type,
                                                                                 SGE_OBJ_AR));

      DRETURN(ar);
   }

   /**
    * @brief Writes an advance reservation to a spool file
    *
    * @param ar the advance reservation to write, may be an empty handle
    * @return true if writing succeeded, false otherwise
    */
   bool ar_write_spool_file(const ocs::tom::ConstElement<ocs::tom::AR> ar) {
      DENTER(TOP_LAYER);

      bool ret = static_cast<bool>(ar);

      if (ret) {
         std::string filename = std::to_string(ar.get<AR_id>());
         if (lWriteElemToDisk(ar, AR_DIR, filename.c_str(), SGE_OBJ_AR) != 0) {
            ret = false;
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Reads all advance reservations from disk and validates them
    *
    * The list stays untyped: it is created through the pointer if it does not
    * exist yet. The files hold advance reservations (AR_Type) as
    * lWriteElemToDisk() wrote them, so they are taken without a check.
    *
    * @param ar_list Pointer to the list pointer where advance reservations will be stored (AR_Type)
    * @param list_name Name for the created list
    * @return true if all advance reservations were successfully read and validated, false otherwise
    */
   bool ar_list_read_from_disk(lList **ar_list, const char *list_name) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (*ar_list == nullptr) {
         // the caller's list is untyped (generic spool_read_list()), so the new list is handed over raw
         *ar_list = ocs::tom::ListPtr<ocs::tom::AR>::create(list_name).release();
      }

      if (ret) {
         const auto master_centry_list =
            ocs::tom::ConstList<ocs::tom::CE>(*DataStore::get_master_list(SGE_TYPE_CENTRY));
         const auto master_ckpt_list = ocs::tom::ConstList<ocs::tom::CK>(*DataStore::get_master_list(SGE_TYPE_CKPT));
         const auto master_cqueue_list =
            ocs::tom::ConstList<ocs::tom::CQ>(*DataStore::get_master_list(SGE_TYPE_CQUEUE));
         const auto master_hgroup_list =
            ocs::tom::ConstList<ocs::tom::HGRP>(*DataStore::get_master_list(SGE_TYPE_HGROUP));
         const auto master_pe_list = ocs::tom::ConstList<ocs::tom::PE>(*DataStore::get_master_list(SGE_TYPE_PE));
         const auto master_userset_list =
            ocs::tom::ConstList<ocs::tom::US>(*DataStore::get_master_list(SGE_TYPE_USERSET));
         lList *answer_list = nullptr;

         const auto ars = ocs::tom::List<ocs::tom::AR>(*ar_list);

         try {
            std::filesystem::directory_iterator iter{AR_DIR};
            for (const auto &entry : iter) {
               auto ar = ocs::tom::ElementPtr<ocs::tom::AR>::from_typed(lReadElemFromDisk(nullptr, entry.path().c_str(),
                                                                                          AR_Type, SGE_OBJ_AR));
               if (ar) {
                  if (ar_validate(ar.get(), &answer_list, true, true, master_cqueue_list, master_hgroup_list,
                                  master_centry_list, master_ckpt_list, master_pe_list, master_userset_list)) {
                     ars.push_back(std::move(ar));
                  } else {
                     answer_list_output(&answer_list);
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
