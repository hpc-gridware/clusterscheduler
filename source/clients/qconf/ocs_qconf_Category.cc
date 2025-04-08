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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/ocs_Category.h"
#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_Client.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "ocs_qconf_Category.h"
#include "msg_qconf.h"

bool ocs::CategoryQconf::show_list(lList **answer_list) {
   DENTER(TOP_LAYER);

   lList *cat_list = get_via_gdi(answer_list);
   if (cat_list != nullptr) {
      spool_flatfile_align_list(answer_list, (const lList *) cat_list, CAT_fields, 3);

      const char *filename = spool_flatfile_write_list(answer_list, cat_list, CAT_fields, &qconf_cat_list_sfi,
                                                       SP_DEST_STDOUT, SP_FORM_ASCII, nullptr, false);
      sge_free(&filename);
      lFreeList(&cat_list);

   }

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   DRETURN(true);
}

bool ocs::CategoryQconf::show(lList **answer_list, u_long64 id) {
   DENTER(TOP_LAYER);

   lListElem *centry = get_via_gdi(answer_list, id);
   if (centry != nullptr) {
      const char *filename = spool_flatfile_write_object(answer_list, centry, false, CAT_fields, &qconf_cat_sfi, SP_DEST_STDOUT,
                                                         SP_FORM_ASCII, nullptr, false);
      sge_free(&filename);
      lFreeElem(&centry);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR, MSG_CAT_DOESNOTEXIST_U64, id);
      DRETURN(false);
   }

   DRETURN(true);
}

lListElem *
ocs::CategoryQconf::get_via_gdi(lList **answer_list, u_long64 id) {
   DENTER(TOP_LAYER);

   // Get the list via GDI
   lList *cat_list = nullptr;
   lEnumeration *what = lWhat("%T(ALL)", CT_Type);
   lCondition *where = lWhere("%T(%I==%u)", CT_Type, CT_id, id);
   lList *gdi_answer_list = gdi::Client::sge_gdi(gdi::Target::TargetValue::SGE_CAT_LIST, gdi::Command::SGE_GDI_GET,
                                                 gdi::SubCommand::SGE_GDI_SUB_NONE, &cat_list, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   // Return the answer list if there was an error
   lListElem *ret = nullptr;
   if (answer_list_has_error(&gdi_answer_list)) {
      answer_list_replace(answer_list, &gdi_answer_list);
   }
   lFreeList(&gdi_answer_list);

   // Dechain the first element
   if (cat_list != nullptr) {
      ret = lDechainElem(cat_list, lFirstRW(cat_list));
      lFreeList(&cat_list);
   }

   DRETURN(ret);
}

lList *
ocs::CategoryQconf::get_via_gdi(lList **answer_list) {
   DENTER(TOP_LAYER);

   // Get the list via GDI
   lList *ret = nullptr;
   lEnumeration *what = lWhat("%T(ALL)", CT_Type);
   lList *gdi_answer_list = gdi::Client::sge_gdi(gdi::Target::TargetValue::SGE_CAT_LIST, gdi::Command::SGE_GDI_GET,
                                                 gdi::SubCommand::SGE_GDI_SUB_NONE, &ret, nullptr, what);
   lFreeWhat(&what);

   // Return the answer list if there was an error
   if (answer_list_has_error(&gdi_answer_list)) {
      answer_list_replace(answer_list, &gdi_answer_list);
   }
   lFreeList(&gdi_answer_list);

   // Sort the list
   if (ret != nullptr) {
      lSortOrder *order = lParseSortOrderVarArg(lGetListDescr(ret), "%I+", CT_id);
      lSortList(ret, order);
      lFreeSortOrder(&order);
   }

   DRETURN(ret);
}
