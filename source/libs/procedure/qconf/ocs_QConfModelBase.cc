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
 * @brief Resolving a `qconf` read operation and fetching what it asks for
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_host.h"
#include "uti/sge_dstring.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/config.h"
#include <cstring>

#include "ocs_QConfModelBase.h"

#include "msg_common.h"
#include "msg_qconf.h"

/** @brief Which object type each read switch reports on
 *
 * Group B of the option inventory: the name lists. One row per switch, so that
 * the procedure decides what to read from the switch it was given rather than
 * from anything the client sends along.
 *
 */
static const ocs::qconf_read_entry qconf_read_table[] = {
   /* group B: the name lists */
   {"-scall",  QConfObj::CAL,  ocs::QConfReadKind::NAME_LIST},
   {"-sckptl", QConfObj::CK,   ocs::QConfReadKind::NAME_LIST},
   {"-scel",   QConfObj::CE,   ocs::QConfReadKind::NAME_LIST},
   {"-sel",    QConfObj::EH,   ocs::QConfReadKind::NAME_LIST},
   {"-shgrpl", QConfObj::HGRP, ocs::QConfReadKind::NAME_LIST},
   {"-spl",    QConfObj::PE,   ocs::QConfReadKind::NAME_LIST},
   {"-sprjl",  QConfObj::PR,   ocs::QConfReadKind::NAME_LIST},
   {"-sql",    QConfObj::CQ,   ocs::QConfReadKind::NAME_LIST},
   {"-srolel", QConfObj::RL,   ocs::QConfReadKind::NAME_LIST},
   {"-srqsl",  QConfObj::RQS,  ocs::QConfReadKind::NAME_LIST},
   {"-sul",    QConfObj::US,   ocs::QConfReadKind::NAME_LIST},
   {"-suserl", QConfObj::UU,   ocs::QConfReadKind::NAME_LIST},
   {"-sconfl", QConfObj::CONF, ocs::QConfReadKind::NAME_LIST},
   {"-sss",    QConfObj::EV,   ocs::QConfReadKind::NAME_LIST},

   /* group A: one named object, rendered in full */
   {"-scal",   QConfObj::CAL,  ocs::QConfReadKind::SINGLE_OBJECT},
   {"-sckpt",  QConfObj::CK,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-se",     QConfObj::EH,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-srole",  QConfObj::RL,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-sp",     QConfObj::PE,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-suser",  QConfObj::UU,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-sprj",   QConfObj::PR,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-sce",    QConfObj::CE,   ocs::QConfReadKind::SINGLE_OBJECT},

   /* group A: the singletons. A separate kind rather than a SINGLE_OBJECT row
    * with name_nm == NoName: that would work - get_object_where() returns
    * nullptr for NoName and arg(0) is null-safe - but a row whose switch takes
    * no name would be a trap for the next reader, and the empty case needs a
    * message that takes no argument (CS-2808). */
   {"-ssconf", QConfObj::SC,   ocs::QConfReadKind::SINGLETON},
   {"-sstree", QConfObj::STN,  ocs::QConfReadKind::SINGLETON},
};

static constexpr size_t qconf_read_table_size =
   sizeof(qconf_read_table) / sizeof(qconf_read_table[0]);

ocs::QConfModelBase::~QConfModelBase() {
   lFreeList(&object_list_);
   lFreeList(&centry_list_);
}

/**
 * @brief Find what a read switch reports on
 *
 * @param switch_name the switch as it was typed
 * @return its row, or nullptr when this procedure does not serve that switch
 */
const ocs::qconf_read_entry *
ocs::QConfModelBase::resolve(const char *switch_name) {
   if (switch_name == nullptr) {
      return nullptr;
   }
   for (size_t i = 0; i < qconf_read_table_size; i++) {
      if (strcmp(qconf_read_table[i].switch_name, switch_name) == 0) {
         return &qconf_read_table[i];
      }
   }
   return nullptr;
}

/**
 * @brief The switch that reads one object type in one way
 *
 * The reverse of resolve(), for the call sites that know the object type rather
 * than the switch - the option blocks of group A, which still decide for
 * themselves what a failure means and therefore call into the procedure by
 * type.
 *
 * @param object the object type
 * @param kind   a name list or one object
 * @return the switch, or nullptr when this procedure does not serve that pair
 */
const char *
ocs::QConfModelBase::switch_for(QConfObj object, QConfReadKind kind) {
   for (size_t i = 0; i < qconf_read_table_size; i++) {
      if (qconf_read_table[i].object == object && qconf_read_table[i].kind == kind) {
         return qconf_read_table[i].switch_name;
      }
   }
   return nullptr;
}

/**
 * @brief Which fields a name list needs
 *
 * Only the key. A name list has no business transporting whole objects, and
 * under output reduction it is also the one field there is nothing to hide in.
 *
 * @param descr  the CULL descriptor of the object type
 * @param key_nm the field holding the name
 * @return the enumeration, which the caller frees
 */
lEnumeration *
ocs::QConfModelBase::get_name_what(const lDescr *descr, int key_nm) {
   return lWhat("%T(%I)", descr, key_nm);
}

/**
 * @brief Which elements a name list leaves out
 *
 * The pseudo entries that are not objects a user created: the template and
 * global execution hosts, the global configuration, and every event client
 * that is not the scheduler.
 *
 * @param descr  the CULL descriptor of the object type
 * @param key_nm the field holding the name
 * @return the condition, or nullptr when everything is listed; the caller frees
 */
lCondition *
ocs::QConfModelBase::get_name_where(const lDescr *descr, int key_nm) {
   switch (key_nm) {
      case EH_name:
         return lWhere("%T(!(%Ic=%s || %Ic=%s))", descr, key_nm, SGE_TEMPLATE_NAME,
                       key_nm, SGE_GLOBAL_NAME);
      case CONF_name:
         return lWhere("%T(!(%I c= %s))", descr, key_nm, SGE_GLOBAL_NAME);
      case EV_host:
         return lWhere("%T(%I==%u))", descr, EV_id, EV_ID_SCHEDD);
      default:
         return nullptr;
   }
}

/**
 * @brief Which element a single-object show asks for
 *
 * A type without a name field holds exactly one object, so there is nothing to
 * filter on. A host name is matched with the host comparison, which folds the
 * domain away - `qconf -se host` finds `host.example.com`.
 *
 * @param descr  the CULL descriptor of the object type
 * @param key_nm the field holding the name, NoName for a singleton
 * @param name   the name that was asked for
 * @return the condition, or nullptr for a singleton; the caller frees
 */
lCondition *
ocs::QConfModelBase::get_object_where(const lDescr *descr, int key_nm, const char *name) {
   if (key_nm == NoName) {
      return nullptr;
   }
   const int pos = lGetPosInDescr(descr, key_nm);
   const bool by_host = (lGetPosType(descr, pos) == lHostT);
   return by_host ? lWhere("%T( %Ih=%s )", descr, key_nm, name)
                  : lWhere("%T( %I==%s )", descr, key_nm, name);
}

/**
 * @brief Fetch what the operation asked for
 *
 * An empty list is an error for plain output and not for JSON, which renders
 * the empty array - the distinction the hand written engine made, kept as it
 * was. The message travels back in the answer list rather than going to stderr,
 * because inside qmaster stderr is the daemon's log and not the caller's
 * terminal.
 *
 * @param answer_list for error reporting
 * @param parameter   the operation and how it renders
 * @return true when there is something to show
 */
/**
 * @brief Fetch every object of one type, for a view that needs the whole list
 *
 * The bespoke views are not "one object by name": the host group views resolve
 * references through the graph, and `-sc` aligns its columns across all complex
 * entries before it renders the first one. Both want the list, so both take this
 * snapshot.
 *
 * The kind is set to NAME_LIST because that is what was fetched. It does not say
 * how it is rendered - the sub-procedure's own controller decides that, which is
 * the whole point of having one.
 *
 * @param answer_list for error reporting
 * @param object      the object type
 * @return true when the list was fetched
 */
bool
ocs::QConfModelBase::make_list_snapshot(lList **answer_list, QConfObj object) {
   DENTER(TOP_LAYER);

   object_ = object;
   kind_ = QConfReadKind::NAME_LIST;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   lEnumeration *what = lWhat("%T(ALL)", desc_->descr);
   const bool ok = fetch(answer_list, desc_->target, nullptr, what);
   lFreeWhat(&what);

   DRETURN(ok);
}

/**
 * @brief Fetch the whole host group list for one of the bespoke views (CS-2816)
 *
 * `-shgrp_tree`, `-shgrp_resolved` and `-shgrp_why` resolve references through
 * the host group graph, so they need every group even when the switch names one.
 * `-shgrp` could be served with a single object, and the hand written code did
 * exactly that - but taking the same snapshot for all four keeps one fetch
 * instead of two, and inside qmaster it is an lSelect() over the master list
 * either way. Host groups are a handful; the client's extra bytes are cheaper
 * than a second code path.
 *
 * The group the caller asked about is kept in requested_name_; whether it exists
 * is the view's business, because the four switches word that differently.
 *
 * @param answer_list for error reporting
 * @param parameter   the invocation, whose operation carries the name
 * @return true when the list was fetched
 */
bool
ocs::QConfModelBase::make_hgroup_snapshot(lList **answer_list, QConfParameter &parameter) {
   DENTER(TOP_LAYER);

   requested_name_ = parameter.get_operation().arg(0);
   if (!make_list_snapshot(answer_list, QConfObj::HGRP)) {
      DRETURN(false);
   }

   /* The controller is handed no answer list, so a missing group is reported
    * here - the same place the table-driven read reports read.not_found_msg. */
   const lListElem *hgroup =
      (requested_name_ != nullptr) ? lGetElemHost(object_list_, key_nm_, requested_name_) : nullptr;
   if (requested_name_ != nullptr && hgroup == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_HGROUP_NOTEXIST_S, requested_name_);
      DRETURN(false);
   }

   if (parameter.get_sub_procedure_name() == QConfParameter::HGROUP_WHY) {
      /* The host is resolved the way every other host argument of this client is,
       * so that a short name and the name the group stores compare equal. A name
       * that does not resolve is not an error: the question is answerable for it,
       * and the answer is that it is not a member.
       *
       * The resolution now happens wherever the procedure runs. In qmaster that
       * is the daemon's resolver, which is the authority for host names in this
       * product - but it is a change, and the -ectx comparison of a short name
       * is what shows whether the two agree. */
      lListElem *probe = lCreateElem(HR_Type);
      lSetHost(probe, HR_name, parameter.get_operation().arg(1));
      sge_resolve_host(probe, HR_name);

      dstring found_in = DSTRING_INIT;
      dstring matcher = DSTRING_INIT;
      hgroup_why_.route = hgroup_why(hgroup, lGetHost(probe, HR_name), object_list_,
                                     &found_in, &matcher);
      hgroup_why_.host = lGetHost(probe, HR_name);

      /* a cleared dstring hands back an empty string rather than nothing once it
       * has held a value, so "no group to report" is emptiness and not nullptr */
      const char *in = sge_dstring_get_string(&found_in);
      hgroup_why_.found_in = (in != nullptr) ? in : "";
      const char *m = sge_dstring_get_string(&matcher);
      hgroup_why_.matcher = (m != nullptr) ? m : "";

      sge_dstring_free(&found_in);
      sge_dstring_free(&matcher);
      lFreeElem(&probe);

      if (hgroup_why_.route == Matcher::Route::NOT_A_MEMBER) {
         /* A host that is no member has always set the exit status while the
          * message went to stdout, not stderr. show_answer() returns non-zero
          * for any status other than STATUS_OK and prints the last element only
          * when its quality is not ANSWER_QUALITY_END - so an END-quality answer
          * carries the exit status across the procedure boundary and prints
          * nothing, which is exactly the old behaviour. */
         answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_END, "%s", "");
      }
   }

   DRETURN(true);
}

bool
ocs::QConfModelBase::make_snapshot(lList **answer_list, QConfParameter &parameter) {
   DENTER(TOP_LAYER);

   if (parameter.get_output_format() == ProcedureParameter::OutputFormat::XML) {
      /* qconf has no XML output: -fmt takes plain and json. The traits struct
       * of the server dispatch still needs an XML view, so the request is
       * refused here, where there is an answer list to say it in - by the time
       * a view is chosen there is no way left to report. */
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "qconf has no XML output; -fmt takes plain or json");
      DRETURN(false);
   }

   /* CS-2816: a sub-procedure renders a shape the descriptor table cannot
    * express, so it brings its own snapshot. The empty sub-procedure name is the
    * table-driven read of groups A and B and falls through. */
   if (QConfParameter::is_hgroup_sub_procedure(parameter.get_sub_procedure_name())) {
      DRETURN(make_hgroup_snapshot(answer_list, parameter));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::CENTRY_LIST) {
      DRETURN(make_list_snapshot(answer_list, QConfObj::CE));
   }

   const qconf_read_entry *entry = resolve(parameter.get_operation().token.c_str());
   if (entry == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_ANSWER_INVALIDOPTIONARGX_S,
                              parameter.get_operation().token.c_str());
      DRETURN(false);
   }

   object_ = entry->object;
   kind_ = entry->kind;
   desc_ = &qconf_desc(entry->object);
   key_nm_ = desc_->name_nm;

   lEnumeration *what;
   lCondition *where;
   if (kind_ == QConfReadKind::NAME_LIST) {
      what = get_name_what(desc_->descr, key_nm_);
      where = get_name_where(desc_->descr, key_nm_);
   } else {
      what = lWhat("%T(ALL)", desc_->descr);
      where = get_object_where(desc_->descr, key_nm_, parameter.get_operation().arg(0));
   }
   const bool ok = fetch(answer_list, desc_->target, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);
   if (!ok) {
      DRETURN(false);
   }

   if (kind_ == QConfReadKind::NAME_LIST) {
      lPSortList(object_list_, "%I+", key_nm_);

      /* An empty list is an error for plain output and not for JSON, which
       * renders the empty array - the distinction the hand written engine made,
       * kept as it was. The message travels back in the answer list rather than
       * going to stderr, because inside qmaster stderr is the daemon's log and
       * not the caller's terminal. */
      if (lGetNumberOfElem(object_list_) == 0 &&
          parameter.get_output_format() != ProcedureParameter::OutputFormat::JSON) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_QCONF_NOXDEFINED_S, desc_->read.list_name);
         DRETURN(false);
      }
   } else {
      /* lGetNumberOfElem() would flag a null list as a CULL error, so the list
       * itself is checked first - as the hand written engine did. The wording of
       * the "not found" message is per object type, its shape is not. */
      if (object_list_ == nullptr || lGetNumberOfElem(object_list_) == 0) {
         if (kind_ == QConfReadKind::SINGLETON) {
            /* no name was given, so the message takes no argument */
            if (desc_->read.empty_msg != nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                       "%s", desc_->read.empty_msg);
            }
         } else if (desc_->read.not_found_msg != nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                    desc_->read.not_found_msg,
                                    parameter.get_operation().arg(0));
         }
         DRETURN(false);
      }

      /* The prepare hook fills the typed JSON values, and needs the complex
       * entries to do it. Fetching them is the model's job - the client asks
       * qmaster, qmaster reads its own list - so the hook runs here and the view
       * only renders. For plain output the hook did nothing anyway, which is why
       * nothing is fetched then. */
      if (desc_->read.prepare != nullptr &&
          parameter.get_output_format() == ProcedureParameter::OutputFormat::JSON) {
         if (!fetch_centries(answer_list)) {
            DRETURN(false);
         }
         desc_->read.prepare(lFirstRW(object_list_), centry_list_);
      }
   }

   DRETURN(true);
}
