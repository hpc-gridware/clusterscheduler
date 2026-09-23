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

/** @file
 * @brief The data one `qconf` read operation needs
 */

#include "cull/cull.h"

#include "gdi/ocs_gdi_Target.h"

#include "ocs_QConfObject.h"
#include "ocs_QConfParameter.h"

namespace ocs {
   /** @brief What a `qconf` read operation is about, resolved from its switch
    *
    * The switch alone says everything: `-sql` is "the names of every cluster
    * queue". Resolving it here rather than taking the object type from the
    * client keeps the procedure in charge of what it reads, which is what
    * matters once RBAC decides what a caller may see.
    */
   /** @brief What a read switch produces */
   enum class QConfReadKind {
      NAME_LIST,     ///< group B: every name of the type, one per line
      SINGLE_OBJECT  ///< group A: one named object, rendered in full
   };

   struct qconf_read_entry {
      const char *switch_name;   ///< the switch, e.g. `-sql`
      QConfObj object;           ///< the object type it reports on
      QConfReadKind kind;        ///< a name list or one object
   };

   /** @brief The objects one `qconf` read operation asked for
    *
    * Fetching is the only thing that differs between the two execution
    * contexts: the client asks qmaster over GDI, qmaster reads its own master
    * lists. Everything above that - resolving the switch, building the filter,
    * sorting - is the same, and lives here.
    *
    * @ingroup libprocedure
    */
   class QConfModelBase {
#pragma region Data

   protected:
      lList *object_list_ = nullptr;             ///< what the operation asked for
      lList *centry_list_ = nullptr;             ///< the complex entries, fetched for JSON only
      const qconf_object_desc *desc_ = nullptr;  ///< the row of the object type
      QConfObj object_ = QConfObj::CAL;          ///< which object type, valid once resolved
      QConfReadKind kind_ = QConfReadKind::NAME_LIST;   ///< what the switch produces
      int key_nm_ = NoName;                      ///< the field holding the name

   public:
      /** @brief The objects that were fetched
       * @return them, sorted by name
       */
      [[nodiscard]] const lList *get_object_list() const { return object_list_; }

      /** @brief The field holding the name
       * @return its CULL id
       */
      [[nodiscard]] int get_key_nm() const { return key_nm_; }

      /** @brief The descriptor of the object type
       * @return its row, or nullptr before make_snapshot()
       */
      [[nodiscard]] const qconf_object_desc *get_desc() const { return desc_; }

      /** @brief Which object type the operation reports on
       * @return it; only meaningful once make_snapshot() has resolved the switch
       */
      [[nodiscard]] QConfObj get_object() const { return object_; }

      /** @brief Whether the operation produces a name list or one object
       * @return the kind
       */
      [[nodiscard]] QConfReadKind get_kind() const { return kind_; }
#pragma endregion

#pragma region Data Retrieval

   public:
      static const qconf_read_entry *resolve(const char *switch_name);
      static const char *switch_for(QConfObj object, QConfReadKind kind);

   protected:
      static lEnumeration *get_name_what(const lDescr *descr, int key_nm);
      static lCondition *get_name_where(const lDescr *descr, int key_nm);
      static lCondition *get_object_where(const lDescr *descr, int key_nm, const char *name);

      virtual bool fetch_centries(lList **answer_list) = 0;

      virtual bool fetch(lList **answer_list, gdi::Target target,
                         lCondition *where, lEnumeration *what) = 0;

   public:
      virtual bool make_snapshot(lList **answer_list, QConfParameter &parameter);
#pragma endregion

#pragma region Constructors/Destructors

   public:
      QConfModelBase() = default;
      virtual ~QConfModelBase();
#pragma endregion
   };
}
