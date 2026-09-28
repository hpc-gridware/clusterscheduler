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

#include <cstdint>

#include "cull/cull.h"

#include "gdi/ocs_gdi_Target.h"

#include <string>

#include "sgeobj/ocs_Matcher.h"

#include <vector>

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
      SINGLE_OBJECT, ///< group A: one named object, rendered in full
      SINGLETON,     ///< group A: the one object of its type, no name given
      OBJECT_LIST,   ///< group N: several named objects, or all of them (CS-2819)
      SINGLE_OBJECT_BY_ID  ///< group N: one object whose key is a number, not a name (CS-2819)
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
      lList *hgroup_list_ = nullptr;             ///< the host groups, for the cluster queue read (CS-2818)
      lList *qref_pattern_list_ = nullptr;       ///< the queue references asked for (`QR_Type`, CS-2818)
      const qconf_object_desc *desc_ = nullptr;  ///< the row of the object type
      QConfObj object_ = QConfObj::CAL;          ///< which object type, valid once resolved
      QConfReadKind kind_ = QConfReadKind::NAME_LIST;   ///< what the switch produces
      int key_nm_ = NoName;                      ///< the field holding the name
      const char *requested_name_ = nullptr;     ///< what a sub-procedure was asked about (CS-2816)
      uint32_t requested_id_ = 0;                ///< the numeric key of a SINGLE_OBJECT_BY_ID read (CS-2819)

   public:
      /** @brief The objects that were fetched
       * @return them, sorted by name
       */
      [[nodiscard]] const lList *get_object_list() const { return object_list_; }

      /** @brief The field holding the name
       * @return its CULL id
       */
      [[nodiscard]] int get_key_nm() const { return key_nm_; }

      /** @brief The host groups a cluster queue is written in terms of (CS-2818)
       *
       * A queue's host list is the member list of its own `@@<queue>` group, and
       * a queue domain has to be resolved against the graph, so the cluster queue
       * read needs the groups next to the queues. Empty for every other read.
       *
       * @return them, or nullptr
       */
      [[nodiscard]] const lList *get_hgroup_list() const { return hgroup_list_; }

      /** @brief The complex entries, when the output format needed them
       *
       * Fetched for JSON only, because that is the only format whose writer
       * resolves a value's type. Empty otherwise.
       *
       * @return them, or nullptr
       */
      [[nodiscard]] const lList *get_centry_list() const { return centry_list_; }

      /** @brief One group of objects `qconf -sq` is to print (CS-2818)
       *
       * One per queue reference given, in the order they were given, because the
       * output is grouped that way. A reference either names queues, and then
       * their configurations are printed, or it names a host or a queue domain,
       * and then the matching queue instances are printed as one group - JSON
       * writes them as a single array.
       *
       * Resolving which is which happens in the model and not in the view: a
       * reference that matches nothing is an error, and ProcedureController's
       * process_request() is handed no answer list to report it in.
       */
      struct CQueueOutput {
         lList *cqueues = nullptr;      ///< the queues to print (`CQ_Type`), or nullptr
         lList *qinstances = nullptr;   ///< the instances to print (`QU_Type`), or nullptr
      };

      /** @brief What `qconf -sq` is to print, grouped by queue reference
       * @return the groups, in the order the references were given
       */
      [[nodiscard]] const std::vector<CQueueOutput> &get_cqueue_output() const { return cqueue_output_; }

      /** @brief What `-sds` found (CS-2819)
       *
       * The whole report as one piece of text, built while taking the snapshot
       * because that is where the answer list is: a queue whose configuration
       * cannot take effect is reported *and* sets a non-zero exit status, and the
       * status travels in an ANSWER_QUALITY_END answer, which show_answer() counts
       * without printing. Empty when every queue is sound.
       *
       * @return the report
       */
      [[nodiscard]] const std::string &get_sick_report() const { return sick_report_; }

      /** @brief The queue references the operation was asked about (CS-2818)
       *
       * Parsed here because the filter is built from them, and kept so that the
       * view iterates the same list instead of parsing the arguments again. Each
       * entry may be a wildcard pattern and may name a host or a queue domain.
       *
       * @return them (`QR_Type`), or nullptr for every other read
       */
      [[nodiscard]] const lList *get_qref_pattern_list() const { return qref_pattern_list_; }

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

      /** @brief The object the bespoke views were asked about (CS-2816)
       *
       * A sub-procedure names one object but is handed the whole list, because
       * the host group views resolve references and need the graph. The name is
       * kept here so that a view need not reach back into the parameter.
       *
       * @return the name, or nullptr for a table-driven read
       */
      [[nodiscard]] const char *get_requested_name() const { return requested_name_; }

      /** @brief What `-shgrp_why` found out (CS-2816)
       *
       * The route is decided here rather than in the view, for two reasons: the
       * host has to be resolved and the graph walked, which is deciding *what*
       * rather than *how*, and the answer list only exists at this point - a
       * host that is no member has always set the exit status, and that has to
       * travel back from qmaster somehow.
       */
      struct HGroupWhy {
         Matcher::Route route = Matcher::Route::NOT_A_MEMBER;   ///< how the host got in, if it did
         std::string host;       ///< the host as resolved, which is what the message names
         std::string found_in;   ///< the group the route led through, empty when there was none
         std::string matcher;    ///< the matcher that admitted it, empty unless route is MATCHER
      };

      /** @brief The answer `-shgrp_why` arrived at
       * @return it; meaningful only for that sub-procedure
       */
      [[nodiscard]] const HGroupWhy &get_hgroup_why() const { return hgroup_why_; }
#pragma endregion

#pragma region Data Retrieval

   protected:
      HGroupWhy hgroup_why_;   ///< filled by the -shgrp_why sub-procedure only
      std::vector<CQueueOutput> cqueue_output_;   ///< filled by the cluster queue sub-procedure only
      std::string sick_report_;                  ///< filled by the -sds sub-procedure only

   public:
      static const qconf_read_entry *resolve(const char *switch_name);
      static const char *switch_for(QConfObj object, QConfReadKind kind);

   protected:
      static lEnumeration *get_name_what(const lDescr *descr, int key_nm);
      static lCondition *get_name_where(const lDescr *descr, int key_nm);
      static lCondition *get_object_where(const lDescr *descr, int key_nm, const char *name);
      static lCondition *get_names_where(const lDescr *descr, int key_nm, const qconf_op &op);

      virtual bool fetch_centries(lList **answer_list) = 0;

      virtual bool fetch_hgroups(lList **answer_list) = 0;

      virtual bool fetch(lList **answer_list, gdi::Target target,
                         lCondition *where, lEnumeration *what) = 0;

   protected:
      bool make_list_snapshot(lList **answer_list, QConfObj object);
      bool make_hgroup_snapshot(lList **answer_list, QConfParameter &parameter);
      bool make_cqueue_snapshot(lList **answer_list, QConfParameter &parameter);
      bool make_sick_snapshot(lList **answer_list);

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
