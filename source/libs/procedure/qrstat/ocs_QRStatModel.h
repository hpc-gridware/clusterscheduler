#pragma once
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

#include "cull/cull.h"

#include "qrstat/ocs_QRStatParameter.h"

namespace ocs {
   class QRStatModel {
   public:
      lEnumeration *what_AR_Type = nullptr;
      lCondition *where_AR_Type = nullptr;
      lList *ar_list = nullptr;

   private:
      void free_data();

      void qrstat_filter_add_core_attributes(QRStatParameter& parameter);
      void qrstat_filter_add_ar_attributes(QRStatParameter& parameter);
      void qrstat_filter_add_xml_attributes(QRStatParameter& parameter);
      void qrstat_filter_add_explain_attributes(QRStatParameter& parameter);
      void qrstat_filter_add_ar_where(QRStatParameter& parameter);
      void qrstat_filter_add_u_where(QRStatParameter& parameter);

      bool fetch_data(lList **answer_list, QRStatParameter& parameter);
   public:
      QRStatModel() = default;
      virtual ~QRStatModel() { free_data(); }

      bool make_snapshot(lList **answer_list, QRStatParameter &parameter);
   };
}