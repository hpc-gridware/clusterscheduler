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
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_eval_expression.h"
#include "uti/ocs_Pattern.h"
#include "sgeobj/config.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_userset.h"
#include "uti/sge.h"
#include <array>
#include <cstring>
#include <fnmatch.h>
#include <utility>

#include "ocs_QConfModelBase.h"

#include "msg_common.h"
/* MSG_CQUEUE_NOQMATCHING_S lives in the clients' shared catalogue, and
 * libs/sgeobj/sge_cqueue.cc and sge_qref.cc already reach for it from the
 * library side for the same reason - the message is about queue references,
 * not about a client */
#include "msg_clients_common.h"
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
   /* the configuration is keyed by host name and prefixes its object with a
    * "#<host>:" comment, see read.name_header (CS-2820) */
   {"-sconf",  QConfObj::CONF, ocs::QConfReadKind::SINGLE_OBJECT},

   /* group A, read by a modify switch (CS-2823). These three types read one
    * object for the editor like every other one, but they have no show switch
    * that does: "-shgrp" and "-sq" are sub-procedures with several output
    * shapes each, and "-su" renders a list of named objects. The row says what
    * the modify switch reads, which is what this table is for, rather than
    * teaching qconf_show_object() a fallback. */
   {"-mhgrp",  QConfObj::HGRP, ocs::QConfReadKind::SINGLE_OBJECT},
   {"-mq",     QConfObj::CQ,   ocs::QConfReadKind::SINGLE_OBJECT},
   {"-mu",     QConfObj::US,   ocs::QConfReadKind::SINGLE_OBJECT},

   /* group N: several named objects, or every one of them when no name is given.
    * -su demands at least one name, -srqs takes none and then shows all, and the
    * option table is what enforces the difference (CS-2819). */
   {"-srqs",   QConfObj::RQS,  ocs::QConfReadKind::OBJECT_LIST},
   {"-su",     QConfObj::US,   ocs::QConfReadKind::OBJECT_LIST},

   /* group N: the one key that is a number. A row of its own rather than a
    * SINGLE_OBJECT with a numeric field, because the filter has to compare as a
    * number - a string compare on CT_id would not match anything (CS-2819). */
   {"-scat",   QConfObj::CAT,  ocs::QConfReadKind::SINGLE_OBJECT_BY_ID},

   /* group J: the four reserved member lists. The switch reports the sublist of
    * one named object, not the objects of a type, so the source differs from a
    * name list while the output does not (CS-2820). */
   {"-sh",     QConfObj::AH,   ocs::QConfReadKind::MEMBER_LIST},
   {"-ss",     QConfObj::SH,   ocs::QConfReadKind::MEMBER_LIST},
   {"-sm",     QConfObj::MGR,  ocs::QConfReadKind::MEMBER_LIST},
   {"-so",     QConfObj::OP,   ocs::QConfReadKind::MEMBER_LIST},

   /* group S: the exports (CS-2826). One row per "-S<obj>" switch, so that the
    * export reads through the same table every other read goes through - the
    * fetch is qmaster's now, and the switch is what says which type it fetches.
    *
    * OBJECT_LIST for all of them, because that is the one kind that means both
    * halves of an export: the names that were given, or every object of the type
    * when none was. So "qconf -Scal dir/" and "qconf -Scal mycal" are one read
    * with and without an argument, which is also why the single export of an
    * object the bulk run leaves out still works - the exclusion applies to the
    * run over everything, see the export exclusion below.
    *
    * The export form itself is not in this table: it is a flag on the request
    * (QConfParameter::get_for_export()), because it changes how the objects are
    * rendered and not which ones are read.
    */
   {"-Scal",   QConfObj::CAL,  ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Sckpt",  QConfObj::CK,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Sce",    QConfObj::CE,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Se",     QConfObj::EH,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Shgrp",  QConfObj::HGRP, ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Sp",     QConfObj::PE,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Sprj",   QConfObj::PR,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Sq",     QConfObj::CQ,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Srole",  QConfObj::RL,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Srqs",   QConfObj::RQS,  ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Su",     QConfObj::US,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Suser",  QConfObj::UU,   ocs::QConfReadKind::OBJECT_LIST, true},
   {"-Sconf",  QConfObj::CONF, ocs::QConfReadKind::OBJECT_LIST, true},

   /* The two singletons export the one object they have, so they read it the way
    * "-sstree" and "-ssconf" do. A directory argument is refused in the client,
    * where the file system is. */
   {"-Sstree", QConfObj::STN,  ocs::QConfReadKind::SINGLETON, true},
   {"-Ssconf", QConfObj::SC,   ocs::QConfReadKind::SINGLETON, true},

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
   lFreeList(&hgroup_list_);
   lFreeList(&qref_pattern_list_);
   for (CQueueOutput &group : cqueue_output_) {
      lFreeList(&group.cqueues);
      lFreeList(&group.qinstances);
   }
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

/** @brief The filter for a read that names several objects (CS-2819)
 *
 * `-su a,b c` and `-srqs a,b` mean the same thing, so the arguments are split on
 * commas and spaces and joined with OR. No argument at all means every object of
 * the type, which is a null filter - `-srqs` uses that, `-su` cannot, because its
 * option table entry demands a name.
 *
 * @param descr  the CULL descriptor of the type
 * @param key_nm the field holding the name
 * @param op     the operation, carrying the arguments
 * @return the condition, or nullptr for "everything"
 */
lCondition *
ocs::QConfModelBase::get_names_where(const lDescr *descr, int key_nm, const qconf_op &op) {
   if (key_nm == NoName || op.arg_count() == 0) {
      return nullptr;
   }

   lList *name_list = nullptr;
   for (size_t i = 0; i < op.arg_count(); i++) {
      lString2List(op.arg(i), &name_list, ST_Type, ST_name, ", ");
   }

   lCondition *where = nullptr;
   for_each_ep_lv(name_elem, name_list) {
      lCondition *one = get_object_where(descr, key_nm, lGetString(name_elem, ST_name));
      where = (where == nullptr) ? one : lOrWhere(where, one);
   }
   lFreeList(&name_list);

   return where;
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
 * @brief Drop from an export over everything what a restore cannot create
 *
 * Two kinds of object are in a type's list and have no business being a file in
 * a backup: the entries that are not objects - the exec host list's `template`
 * and `global`, which carry defaults rather than a host (CS-2826) - and the ones
 * the product refuses to have written: `@exec_hosts`, which no GDI write may touch
 * at all, and `@@<queue>`, whose add is refused and whose member list the queue's
 * own export file already carries (CS-2906).
 *
 * Which ones those are is the object table's to say, and it says it with a
 * predicate rather than with names: what `@@` means belongs to
 * hgroup_is_queue_group() and is not to be spelled out a second time here.
 *
 * The filter runs after the fetch and not as a condition on it, because that is
 * the only place a predicate can be asked. The list it works on is this read's
 * own copy either way - in qmaster lSelect() has already copied the master list.
 *
 * @param list   the fetched objects, filtered in place
 * @param desc   the row of the object type
 * @param key_nm the field holding the name
 * @return none
 */
static void
drop_from_export(lList *list, const qconf_object_desc &desc, int key_nm) {
   if (desc.export_exclude == nullptr || key_nm == NoName) {
      return;
   }
   lListElem *ep = lFirstRW(list);
   while (ep != nullptr) {
      lListElem *next = lNextRW(ep);
      if (desc.export_exclude(qconf_get_name(ep, key_nm))) {
         lRemoveElem(list, &ep);
      }
      ep = next;
   }
}

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

   /* An empty result has to be an empty list, not no list at all. lSelect() hands
    * back nullptr when nothing matched, while the same read over GDI produces an
    * empty list - and the flatfile writer tells the two apart: it writes the table
    * header for an empty list and nothing at all for nullptr. `qconf -scatl` on a
    * cluster without categories printed that header before the read moved, so the
    * two execution contexts disagreed until this line (CS-2819). */
   if (ok && object_list_ == nullptr) {
      object_list_ = lCreateList("", desc_->descr);
   }

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

/** @brief Resolve the types of a cluster queue's complex overrides for JSON
 *
 * `complex_values`, `load_thresholds` and `suspend_thresholds` may be overridden
 * per host or host group. Each override's value is a `CE_Type` list, and
 * resolving its types from the complex entries lets the JSON writer emit typed
 * numbers - `np_load_avg` as a double rather than as a string.
 *
 * The complex entries are passed in. In the client this function fetched them
 * itself, once per object shown, which CS-2799 had already taken out of the
 * other consumer of them; inside qmaster there is nothing to fetch.
 *
 * Mutates only the copy about to be written.
 *
 * @param cqueue      the queue about to be shown, may be nullptr
 * @param centry_list the complex entries, may be nullptr
 */
static void
cqueue_json_type_complex_overrides(lListElem *cqueue, const lList *centry_list) {
   if (cqueue == nullptr || centry_list == nullptr) {
      return;
   }
   static const int override_fields[] = {
      CQ_consumable_config_list, CQ_load_thresholds, CQ_suspend_thresholds
   };
   for (const int field : override_fields) {
      lList *override_list = lGetListRW(cqueue, field);
      for (lListElem *entry = lFirstRW(override_list); entry != nullptr; entry = lNextRW(entry)) {
         lList *ce_list = lGetListRW(entry, ACELIST_value);
         if (ce_list != nullptr && lGetNumberOfElem(ce_list) > 0) {
            lList *fill_alp = nullptr;
            centry_list_fill_request(ce_list, &fill_alp, centry_list, true, true, true);
            lFreeList(&fill_alp);
         }
      }
   }
}

/** @brief The rendered value of a list attribute, or `NONE` when it is empty
 *
 * Every list valued attribute of `-sobjl` is matched against its rendered form,
 * and an empty list renders as the literal `NONE` rather than as nothing - which
 * is why `-sobjl exechost user_lists '*'` answers for every host although none of
 * them has a user list.
 *
 * @param value the dstring the caller filled
 * @return its contents, or `NONE` when it stayed empty
 */
static const char *
sobjl_value_or_none(dstring *value) {
   const char *text = sge_dstring_get_string(value);
   if (text == nullptr) {
      sge_dstring_copy_string(value, "NONE");
      text = sge_dstring_get_string(value);
   }
   return text;
}

/** @brief Name the execution hosts `-sobjl exechost` matched (CS-2819)
 *
 * Seven attributes, in a fixed order, and a host is named at most once: the first
 * attribute whose name matches the pattern and whose value matches decides, and
 * the remaining ones are not looked at. An attribute outside this set matches
 * nothing at all, which is why `-sobjl exechost projects '*'` answers nothing
 * although `qconf -se` prints a projects line - the set is the switch's own, not
 * the object's.
 *
 * @param answer_list used to return error messages
 * @param attr the attribute name pattern
 * @param val the value pattern
 * @return true on success; false with @p answer_list filled when the hosts could not be read
 */
bool
ocs::QConfModelBase::collect_exechost_matches(lList **answer_list, const char *attr, const char *val) {
   DENTER(TOP_LAYER);

   object_ = QConfObj::EH;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;
   kind_ = QConfReadKind::NAME_LIST;

   lEnumeration *what = lWhat("%T(ALL)", desc_->descr);
   const bool ok = fetch(answer_list, desc_->target, nullptr, what);
   lFreeWhat(&what);
   if (!ok) {
      DRETURN(false);
   }

   for_each_ep_lv(elem, object_list_) {
      const char *hostname = lGetHost(elem, EH_name);
      bool already_handled = false;

      if (fnmatch(attr, SGE_ATTR_HOSTNAME, 0) == 0 && fnmatch(val, hostname, 0) == 0) {
         filtered_names_.emplace_back(hostname);
         already_handled = true;
      }

      if (!already_handled && fnmatch(attr, SGE_ATTR_LOAD_SCALING, 0) == 0) {
         dstring value = DSTRING_INIT;
         for (const lListElem *ep = lFirst(lGetList(elem, EH_scaling_list)); ep != nullptr; ep = lNext(ep)) {
            sge_dstring_sprintf_append(&value, "%s=%.10g", lGetString(ep, HS_name), lGetDouble(ep, HS_value));
            if (lNext(ep) != nullptr) {
               sge_dstring_append(&value, ",");
            }
         }
         if (fnmatch(val, sobjl_value_or_none(&value), 0) == 0) {
            filtered_names_.emplace_back(hostname);
            already_handled = true;
         }
         sge_dstring_free(&value);
      }

      if (!already_handled && fnmatch(attr, SGE_ATTR_COMPLEX_VALUES, 0) == 0) {
         dstring value = DSTRING_INIT;
         for (const lListElem *ep = lFirst(lGetList(elem, EH_consumable_config_list)); ep != nullptr; ep = lNext(ep)) {
            sge_dstring_sprintf_append(&value, "%s=", lGetString(ep, CE_name));
            if (lGetString(ep, CE_stringval) != nullptr) {
               sge_dstring_append(&value, lGetString(ep, CE_stringval));
            } else {
               /* the hand written switch read this double field with lGetString
                * and printed it as %f, which is a CULL type error and undefined
                * behaviour; the branch is only reached when a complex value has
                * no string form, which is why it survived (CS-2819) */
               sge_dstring_sprintf_append(&value, "%f", lGetDouble(ep, CE_doubleval));
            }
            if (lNext(ep) != nullptr) {
               sge_dstring_append(&value, ",");
            }
         }
         if (fnmatch(val, sobjl_value_or_none(&value), 0) == 0) {
            filtered_names_.emplace_back(hostname);
            already_handled = true;
         }
         sge_dstring_free(&value);
      }

      if (!already_handled && fnmatch(attr, SGE_ATTR_LOAD_VALUES, 0) == 0) {
         dstring value = DSTRING_INIT;
         for (const lListElem *ep = lFirst(lGetList(elem, EH_load_list)); ep != nullptr; ep = lNext(ep)) {
            sge_dstring_sprintf_append(&value, "%s=%s", lGetString(ep, HL_name), lGetString(ep, HL_value));
            if (lNext(ep) != nullptr) {
               sge_dstring_append(&value, ",");
            }
         }
         if (fnmatch(val, sobjl_value_or_none(&value), 0) == 0) {
            filtered_names_.emplace_back(hostname);
            already_handled = true;
         }
         sge_dstring_free(&value);
      }

      if (!already_handled && fnmatch(attr, SGE_ATTR_PROCESSORS, 0) == 0) {
         dstring value = DSTRING_INIT;
         sge_dstring_sprintf(&value, "%d", static_cast<int>(lGetUlong(elem, EH_processors)));
         if (fnmatch(val, sge_dstring_get_string(&value), 0) == 0) {
            filtered_names_.emplace_back(hostname);
            already_handled = true;
         }
         sge_dstring_free(&value);
      }

      for (const auto &[attr_name, field] : {std::pair{SGE_ATTR_USER_LISTS, EH_acl},
                                             std::pair{SGE_ATTR_XUSER_LISTS, EH_xacl}}) {
         if (already_handled || fnmatch(attr, attr_name, 0) != 0) {
            continue;
         }
         dstring value = DSTRING_INIT;
         for (const lListElem *ep = lFirst(lGetList(elem, field)); ep != nullptr; ep = lNext(ep)) {
            sge_dstring_append(&value, lGetString(ep, US_name));
            if (lNext(ep) != nullptr) {
               sge_dstring_append(&value, " ");
            }
         }
         if (fnmatch(val, sobjl_value_or_none(&value), 0) == 0) {
            filtered_names_.emplace_back(hostname);
            already_handled = true;
         }
         sge_dstring_free(&value);
      }
   }

   DRETURN(true);
}

/** @brief Name the queues, domains or instances `-sobjl queue*` matched (CS-2819)
 *
 * The three queue specifications share one pass over the cluster queues and differ
 * only in which of the three kinds of an attribute entry is named: the queue wide
 * value (`queue`), a value overridden for a host group (`queue_domain`) or one
 * overridden for a single host (`queue_instance`).
 *
 * Two details of the hand written switch are kept deliberately, see CS-2832:
 * an entry that matches ends the queue even when its kind is not the requested
 * one - so `-sobjl queue_instance slots '*'` answers nothing, the queue wide
 * value having matched first - and the rendered value accumulates over the
 * entries instead of being rebuilt per entry, so the second entry of a queue is
 * matched against the concatenation of both.
 *
 * @param answer_list used to return error messages
 * @param spec `queue`, `queue_domain` or `queue_instance`
 * @param attr the attribute name pattern
 * @param val the value pattern
 * @return true on success; false with @p answer_list filled when the queues could not be read
 */
bool
ocs::QConfModelBase::collect_cqueue_matches(lList **answer_list, const char *spec, const char *attr, const char *val) {
   DENTER(TOP_LAYER);

   const bool handle_cqueue = strcmp(spec, "queue") == 0;
   const bool handle_domain = strcmp(spec, "queue_domain") == 0;
   const bool handle_qinstance = strcmp(spec, "queue_instance") == 0;

   object_ = QConfObj::CQ;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;
   kind_ = QConfReadKind::NAME_LIST;

   if (!fetch_hgroups(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *what = lWhat("%T(ALL)", desc_->descr);
   const bool ok = fetch(answer_list, desc_->target, nullptr, what);
   lFreeWhat(&what);
   if (!ok) {
      DRETURN(false);
   }

   /* CS-2677: the host list lives in the queue's own "@@<queue>" group since then,
    * so it has to be joined back on before "hostlist" can be matched against what
    * qconf -sq would print */
   cqueue_list_fill_hostlist(object_list_, hgroup_list_);

   for_each_ep_lv(cqueue, object_list_) {
      const char *cqueue_name = lGetString(cqueue, CQ_name);
      bool already_handled = false;

      if (fnmatch(attr, SGE_ATTR_QNAME, 0) == 0 && handle_cqueue &&
          fnmatch(val, cqueue_name, 0) == 0) {
         filtered_names_.emplace_back(cqueue_name);
         already_handled = true;
      }

      if (!already_handled && fnmatch(attr, SGE_ATTR_HOST_LIST, 0) == 0) {
         dstring value = DSTRING_INIT;
         if (const lList *hostref_list = lGetList(cqueue, CQ_hostlist); hostref_list != nullptr) {
            href_list_append_to_dstring(hostref_list, &value);
         } else {
            sge_dstring_copy_string(&value, "NONE");
         }
         if (handle_cqueue && fnmatch(val, sge_dstring_get_string(&value), 0) == 0) {
            filtered_names_.emplace_back(cqueue_name);
            already_handled = true;
         }
         sge_dstring_free(&value);
      }

      for (int index = 0; !already_handled && cqueue_attribute_array[index].cqueue_attr != NoName; index++) {
         if (fnmatch(attr, cqueue_attribute_array[index].name, 0) != 0) {
            continue;
         }
         /* one dstring for the whole attribute, not one per entry - the
          * accumulation this produces is the behaviour of CS-2832 */
         dstring value = DSTRING_INIT;
         const lList *attribute_list = lGetList(cqueue, cqueue_attribute_array[index].cqueue_attr);

         for_each_ep_lv(attribute, attribute_list) {
            const lDescr *descr = lGetListDescr(attribute_list);
            const char *host_hgroup = lGetHost(attribute, cqueue_attribute_array[index].href_attr);
            const bool is_cqueue = strcmp(host_hgroup, HOSTREF_DEFAULT) == 0;
            const bool is_domain = !is_cqueue && ocs::is_hgroup_name(host_hgroup);
            const bool is_qinstance = !is_domain && !is_cqueue;

            /* rendered through a one element copy whose host reference is the
             * default, so the entry renders as a plain value without its
             * "[<host>=...]" brackets */
            lList *tmp_attribute_list = lCreateList("", descr);
            lListElem *tmp_attribute = lCopyElem(attribute);
            lAppendElem(tmp_attribute_list, tmp_attribute);
            lSetHost(tmp_attribute, cqueue_attribute_array[index].href_attr, HOSTREF_DEFAULT);
            attr_list_append_to_dstring(tmp_attribute_list, &value, descr,
                                        cqueue_attribute_array[index].href_attr,
                                        cqueue_attribute_array[index].value_attr);
            lFreeList(&tmp_attribute_list);

            if (fnmatch(val, sge_dstring_get_string(&value), 0) == 0) {
               if (handle_cqueue && is_cqueue) {
                  filtered_names_.emplace_back(cqueue_name);
               } else if ((handle_domain && is_domain) || (handle_qinstance && is_qinstance)) {
                  filtered_names_.emplace_back(std::string{cqueue_name} + "@" + host_hgroup);
               }
               /* set even when nothing was written, see the note above */
               already_handled = true;
               break;
            }
         }
         sge_dstring_free(&value);
      }
   }

   DRETURN(true);
}

/** @brief Answer `qconf -sobjl <spec> <attr_pattern> <value_pattern>` (CS-2819)
 *
 * The switch names every object of one kind that has an attribute whose *name*
 * matches the first pattern and whose *value* matches the second. Four kinds,
 * and the last two are about overrides rather than about the object itself:
 *
 * - `queue`          the cluster queues, matched against their own values
 * - `exechost`       the execution hosts, including `global` and `template`
 * - `queue_instance` a queue whose attribute is overridden for a host
 * - `queue_domain`   a queue whose attribute is overridden for a host group
 *
 * What comes back are names, not objects, which is why the result is a vector of
 * strings rather than a CULL list: a queue instance is named `queue@host` and a
 * domain `queue@@group`, neither of which is the name of anything the daemon
 * holds.
 *
 * @param answer_list used to return error messages
 * @param parameter the operation, whose three arguments are spec, attribute and value
 * @return true unless the objects could not be read or the specification is unknown
 */
bool
ocs::QConfModelBase::make_sobjl_snapshot(lList **answer_list, QConfParameter &parameter) {
   DENTER(TOP_LAYER);

   const char *spec = parameter.get_operation().arg(0);
   const char *attr = parameter.get_operation().arg(1);
   const char *val = parameter.get_operation().arg(2);
   if (spec == nullptr || attr == nullptr || val == nullptr) {
      /* unreachable from qconf, whose option table demands exactly three
       * arguments - a guard against a procedure call built elsewhere */
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              "%s", MSG_PARSE_NOOPTIONARGUMENT);
      DRETURN(false);
   }

   if (strcmp(spec, "exechost") == 0) {
      DRETURN(collect_exechost_matches(answer_list, attr, val));
   }
   if (strcmp(spec, "queue") == 0 || strcmp(spec, "queue_domain") == 0 ||
       strcmp(spec, "queue_instance") == 0) {
      DRETURN(collect_cqueue_matches(answer_list, spec, attr, val));
   }

   /* An unknown specification is not an error: none of the four handlers matches,
    * so the hand written switch wrote nothing and exited zero, and so does this.
    * The same goes for an unknown attribute and for a value matching nothing -
    * every answer of this switch that finds nothing is simply empty. */
   DRETURN(true);
}

/** @brief Fetch the qmaster thread pools for `qconf -stl` (CS-2820)
 *
 * The only read of this model whose data is not a configuration object: the
 * pools are the running daemon's own state. Both contexts therefore go through
 * fetch_thread_pools() rather than through fetch(), which reads the data store.
 *
 * An empty answer is left as an empty list and not reported: the hand written
 * switch printed nothing at all in that case, header included, and said nothing
 * about it.
 *
 * @param answer_list used to return error messages
 * @return true on success
 */
bool
ocs::QConfModelBase::make_threadpool_snapshot(lList **answer_list) {
   DENTER(TOP_LAYER);

   /* there is no descriptor row for the thread pools, and nothing downstream
    * consults these on a sub-procedure path - they are set so that no field is
    * left over from a previous call, the way make_eventclient_snapshot() does */
   object_ = QConfObj::EV;
   kind_ = QConfReadKind::NAME_LIST;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   if (!fetch_thread_pools(answer_list)) {
      DRETURN(false);
   }
   if (object_list_ == nullptr) {
      object_list_ = lCreateList("", ST_Type);
   }

   DRETURN(true);
}

/** @brief Fetch the share tree, and check the node paths that were asked for (CS-2820)
 *
 * Serves `-sst`, `-bonsai`, `-sstnode` and `-rsstnode`. The tree is a singleton,
 * so the whole object is read and the root is its first element.
 *
 * For the two node path switches the comma separated argument is split here and
 * every path is checked against the tree, because a path that names no node has
 * to be reported and the view is handed no answer list. The report is an ERROR
 * answer and the function still returns **true**: the paths that do exist have
 * to be rendered all the same, which is what the hand written switch did. Only a
 * read that failed outright returns false.
 *
 * A cluster without a share tree keeps its old answer: the "no share tree"
 * message and nothing else.
 *
 * @param answer_list used to return error messages
 * @param parameter the invocation, consulted for the node paths
 * @return true unless the share tree could not be read
 */
bool
ocs::QConfModelBase::make_sharetree_snapshot(lList **answer_list, QConfParameter &parameter) {
   DENTER(TOP_LAYER);

   object_ = QConfObj::STN;
   /* not consulted on a sub-procedure path, set for the reason given in
    * make_eventclient_snapshot() */
   kind_ = QConfReadKind::SINGLE_OBJECT;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   lEnumeration *what = lWhat("%T(ALL)", desc_->descr);
   const bool ok = fetch(answer_list, desc_->target, nullptr, what);
   lFreeWhat(&what);
   if (!ok) {
      DRETURN(false);
   }

   lListElem *root = lFirstRW(object_list_);
   if (root == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              "%s", MSG_TREE_NOSHARETREE);
      DRETURN(false);
   }

   if (parameter.get_sub_procedure_name() != QConfParameter::SHARETREE_NODES) {
      DRETURN(true);
   }

   const char *paths = parameter.get_operation().arg(0);
   if (paths == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              "%s", MSG_PARSE_NOOPTIONARGUMENT);
      DRETURN(false);
   }

   /* the same split the hand written switch used, so an empty element between two
    * commas is skipped rather than reported as a missing node */
   lList *path_list = nullptr;
   lString2List(paths, &path_list, STN_Type, STN_name, ", ");
   for_each_ep_lv(path_elem, path_list) {
      const char *path = lGetString(path_elem, STN_name);
      if (path == nullptr) {
         continue;
      }
      sharetree_paths_.emplace_back(path);

      /* the same two spellings of the root the renderer accepts */
      const bool is_root = (strcmp(path, "/") == 0) || (strcasecmp(path, "Root") == 0);
      if (!is_root) {
         ancestors_t ancestors{};
         if (search_named_node_path(root, path, &ancestors) == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                    MSG_TREE_UNABLETOLACATEXINSHARETREE_S, path);
         }
         free_ancestors(&ancestors);
      }
   }
   lFreeList(&path_list);

   DRETURN(true);
}

/** @brief Fetch the execution hosts `qconf -sep` reports on (CS-2820)
 *
 * The same reduced field set and the same filter the hand written switch used:
 * only the name, the processor count and the load values are needed, and the
 * pseudo hosts `global` and `template` are left out - they have no processors and
 * the SUM row would be wrong if they were counted.
 *
 * An empty cluster is an error here rather than an empty table, because that is
 * what the switch did: it wrote the diagnostic to stderr and no table at all.
 *
 * @param answer_list used to return error messages
 * @return true on success; false with @p answer_list filled when nothing could be read
 */
bool
ocs::QConfModelBase::make_processor_snapshot(lList **answer_list) {
   DENTER(TOP_LAYER);

   object_ = QConfObj::EH;
   /* not consulted on a sub-procedure path, set for the reason given in
    * make_eventclient_snapshot() */
   kind_ = QConfReadKind::NAME_LIST;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   lEnumeration *what = lWhat("%T(%I %I %I)", EH_Type, EH_name, EH_processors, EH_load_list);
   lCondition *where = lWhere("%T(!(%Ic=%s || %Ic=%s))", EH_Type, EH_name,
                              SGE_TEMPLATE_NAME, EH_name, SGE_GLOBAL_NAME);
   const bool ok = fetch(answer_list, desc_->target, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);
   if (!ok) {
      DRETURN(false);
   }

   if (lGetNumberOfElem(object_list_) == 0) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              "%s", MSG_QCONF_NOEXECUTIONHOSTSDEFINED);
      DRETURN(false);
   }
   lPSortList(object_list_, "%I+", EH_name);

   DRETURN(true);
}

/** @brief The snapshot of `qconf -secl` (CS-2820)
 *
 * Fetches the registered event clients with the three fields the switch shows
 * and sorts them by id, which is the order the hand written switch produced.
 *
 * The reduced enumeration is deliberate and safe here, unlike the one that
 * aborted in CS-2818: the only reader of these elements is
 * QConfEventClientView, and it touches exactly these three fields. In server
 * context the read does not go through lSelect() at all - QConfModelServer
 * answers an EV read from the event master, which builds the list on demand.
 *
 * An empty list is an error - the rule every migrated read follows, and the one
 * behaviour this move changed on purpose: the hand written switch wrote the
 * message to stderr and still exited 0, so a cluster without event clients was
 * indistinguishable from a successful read. Since CS-2929 the rule no longer
 * reads the output format: the error is reported and the read returns normally,
 * so the empty output is rendered in whatever form was asked for and the exit
 * code says the same thing in both.
 *
 * @param answer_list carries the reason when the read fails
 * @param parameter   the invocation
 * @return true when the clients were fetched
 */
bool
ocs::QConfModelBase::make_eventclient_snapshot(lList **answer_list) {
   DENTER(TOP_LAYER);

   object_ = QConfObj::EV;
   /* the kind is not consulted on a sub-procedure path - this controller renders
    * rather than QConfController - but it is set so that nothing downstream sees
    * an unset field, the way make_sick_snapshot() does */
   kind_ = QConfReadKind::NAME_LIST;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   lEnumeration *what = lWhat("%T(%I %I %I)", EV_Type, EV_id, EV_name, EV_host);
   const bool ok = fetch(answer_list, desc_->target, nullptr, what);
   lFreeWhat(&what);
   if (!ok) {
      DRETURN(false);
   }

   /* an empty result has to be an empty list, not no list at all - see the note
    * in make_list_snapshot() */
   if (object_list_ == nullptr) {
      object_list_ = lCreateList("", EV_Type);
   }
   lPSortList(object_list_, "%I+", EV_id);

   /* reported, not failed - see the note on the same rule in make_snapshot() */
   if (lGetNumberOfElem(object_list_) == 0) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              "%s", MSG_QCONF_NOEVENTCLIENTSREGISTERED);
   }

   DRETURN(true);
}

/** @brief Run the sick check over every cluster queue (CS-2819)
 *
 * `qconf -sds` reports queues whose configuration is accepted but cannot take
 * effect - most often an attribute overridden for a host or host group the queue
 * does not actually contain, so the override reads as if it applied.
 *
 * The report is built here rather than in the view for the same reason the
 * cluster queue references are resolved here: this is where the answer list is,
 * and a report that is not empty has always set a non-zero exit status as well as
 * being printed. That status travels in an ANSWER_QUALITY_END answer, which
 * show_answer() counts without printing - the mechanism -shgrp_why introduced in
 * CS-2816. Output and a status together is exactly what this switch means, unlike
 * a reference that matched nothing, which is an error and prints nothing.
 *
 * @param answer_list used to return error messages
 * @return true unless the queues could not be read
 */
bool
ocs::QConfModelBase::make_sick_snapshot(lList **answer_list) {
   DENTER(TOP_LAYER);

   object_ = QConfObj::CQ;
   kind_ = QConfReadKind::NAME_LIST;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   if (!fetch_hgroups(answer_list)) {
      DRETURN(false);
   }

   /* every attribute, for the reason spelled out in make_cqueue_snapshot(): a
    * reduced descriptor and lSelect() do not go together */
   lEnumeration *what = lWhat("%T(ALL)", desc_->descr);
   const bool ok = fetch(answer_list, desc_->target, nullptr, what);
   lFreeWhat(&what);
   if (!ok) {
      DRETURN(false);
   }

   /* CS-2677: join the host lists back on, from the groups fetched above. Without
    * it every queue's host list is empty - since CS-2677 it lives in the queue's
    * own "@@<queue>" host group - and cqueue_sick() then reports every attribute
    * overridden for a host as unused, which is the whole of its output. */
   cqueue_list_fill_hostlist(object_list_, hgroup_list_);

   dstring ds = DSTRING_INIT;
   for (lListElem *cqueue = lFirstRW(object_list_); cqueue != nullptr; cqueue = lNextRW(cqueue)) {
      cqueue_sick(cqueue, answer_list, hgroup_list_, &ds);
   }
   const char *text = sge_dstring_get_string(&ds);
   sick_report_ = (text != nullptr) ? text : "";
   sge_dstring_free(&ds);

   if (!sick_report_.empty()) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_END, "%s", "");
   }

   DRETURN(true);
}

/** @brief Take the snapshot the cluster queue read needs (CS-2818)
 *
 * Three things beyond a plain list read. The queue references are parsed here
 * because the filter is built from them and because the view iterates the same
 * list afterwards. The enumeration is reduced the way the client always reduced
 * it - a pattern naming a host or a domain needs the queue instances, one naming
 * only a queue does not - so that moving the read into the daemon does not turn
 * every `qconf -sq` into a copy of every queue instance in the cluster. And the
 * host groups are fetched next to the queues, because since CS-2677 a queue's
 * host list is the member list of its own `@@<queue>` group and the queue cannot
 * be rendered without them.
 *
 * Fetching the groups first and the queues second is deliberate: in the client
 * both went into one multi request so that the two could not disagree, and the
 * order here keeps the groups the older of the two, which is the safe direction
 * - a group that has just appeared leaves a queue unresolved, a group that has
 * just gone would leave a queue pointing at nothing.
 *
 * @param answer_list used to return error messages
 * @param parameter the operation, whose arguments are the queue references
 * @return true on success; false with @p answer_list filled otherwise
 */
bool
ocs::QConfModelBase::make_cqueue_snapshot(lList **answer_list, QConfParameter &parameter) {
   DENTER(TOP_LAYER);

   object_ = QConfObj::CQ;
   kind_ = QConfReadKind::NAME_LIST;
   desc_ = &qconf_desc(object_);
   key_nm_ = desc_->name_nm;

   /* CS-2799 took the complex entries out of the render loop once already, for
    * qconf_json_fill_complex(). The queue rendering has a second consumer of
    * them, the typed complex overrides, which fetched them itself per object -
    * so they are fetched once here, and only for JSON, which is the only format
    * either of the two does anything for. */
   if (parameter.get_output_format() == ProcedureParameter::OutputFormat::JSON) {
      if (!fetch_centries(answer_list)) {
         DRETURN(false);
      }
   }

   qref_pattern_list_ = lCreateList("qref_pattern_list", QR_Type);
   for (size_t i = 0; i < parameter.get_operation().arg_count(); i++) {
      lString2List(parameter.get_operation().arg(i), &qref_pattern_list_, QR_Type, QR_name, ", ");
   }

   /* "-sq" without an argument prints a queue template, not every queue, so
    * there is nothing to fetch and nothing to filter. The template is built
    * here rather than in the view because it is an object, and producing the
    * objects is what this side does. An empty pattern list is what tells the
    * view which of the two it got, the same test the client made on a null
    * pattern list. */
   if (lGetNumberOfElem(qref_pattern_list_) == 0) {
      lListElem *tmpl = cqueue_create(answer_list, "template");
      if (tmpl == nullptr || !cqueue_set_template_attributes(tmpl, answer_list)) {
         lFreeElem(&tmpl);
         DRETURN(false);
      }
      object_list_ = lCreateList("cqueue_list", CQ_Type);
      lAppendElem(object_list_, tmpl);
      cqueue_json_type_complex_overrides(tmpl, centry_list_);

      CQueueOutput group;
      group.cqueues = lCreateList("cqueue_list", CQ_Type);
      lAppendElem(group.cqueues, lCopyElem(tmpl));
      cqueue_output_.push_back(group);
      DRETURN(true);
   }

   lCondition *cqueue_where = nullptr;
   for_each_ep_lv(qref_pattern, qref_pattern_list_) {
      dstring cqueue_name = DSTRING_INIT;
      dstring host_domain = DSTRING_INIT;
      bool has_hostname;
      bool has_domain;
      const char *name = lGetString(qref_pattern, QR_name);

      if (!cqueue_name_split(name, &cqueue_name, &host_domain, &has_hostname, &has_domain)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_CQUEUE_NOQMATCHING_S, name);
         sge_dstring_free(&cqueue_name);
         sge_dstring_free(&host_domain);
         lFreeWhere(&cqueue_where);
         DRETURN(false);
      }
      const char *cqueue_name_str = sge_dstring_get_string(&cqueue_name);
      if (cqueue_name_str == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_CQUEUE_NAMENOTCORRECT_SS, name, name);
         sge_dstring_free(&cqueue_name);
         sge_dstring_free(&host_domain);
         lFreeWhere(&cqueue_where);
         DRETURN(false);
      }
      lCondition *add_where = lWhere("%T(%I p= %s)", CQ_Type, CQ_name, cqueue_name_str);
      cqueue_where = (cqueue_where == nullptr) ? add_where : lOrWhere(cqueue_where, add_where);

      sge_dstring_free(&cqueue_name);
      sge_dstring_free(&host_domain);
   }

   if (!fetch_hgroups(answer_list)) {
      lFreeWhere(&cqueue_where);
      DRETURN(false);
   }

   /* Every attribute, not the reduced selection the client used to ask GDI for.
    * That selection exists to keep a queue's instances off the wire, and over GDI
    * the full descriptor is rebuilt when the answer is unpacked. lSelect() has no
    * such step: it hands back elements carrying exactly the reduced descriptor,
    * and the spooling field list then asks for a field that is not in them, which
    * CULL answers by aborting the daemon ("ASTRLIST_href not found in element",
    * CS-2818). Inside qmaster the copy never leaves the process, so the selection
    * saved nothing here anyway. */
   lEnumeration *what = lWhat("%T(ALL)", desc_->descr);
   const bool ok = fetch(answer_list, desc_->target, cqueue_where, what);
   lFreeWhat(&what);
   lFreeWhere(&cqueue_where);   /* fetch() does not take ownership, as in make_snapshot() */
   if (!ok) {
      DRETURN(false);
   }

   /* CS-2677: join the host lists back on, from the groups fetched above */
   cqueue_list_fill_hostlist(object_list_, hgroup_list_);

   /* The typed JSON values are resolved here rather than in the view, which is
    * where the descriptor table's prepare hook resolves them too, and it saves
    * the view a const_cast on the model's list. Nothing to do for plain output,
    * where centry_list_ was never fetched. */
   for (lListElem *cqueue = lFirstRW(object_list_); cqueue != nullptr; cqueue = lNextRW(cqueue)) {
      cqueue_json_type_complex_overrides(cqueue, centry_list_);
   }

   /* Resolve each reference into the group of objects it stands for. A reference
    * matching nothing fails the whole call: the framework hands the caller either
    * a rendering or an error, never both, and reporting one reference while
    * printing the others would be the odd case rather than the clear one. This is
    * a deliberate change - the hand written code printed what the other references
    * matched and still exited non-zero. Every unmatched reference is still added
    * to the answer list, so show_answer() reports the last of them exactly as
    * before. */
   static const int qu_ce[] = {QU_consumable_config_list, QU_load_thresholds,
                               QU_suspend_thresholds};
   bool matched_all = true;
   for_each_ep_lv(qref_pattern, qref_pattern_list_) {
      dstring cqueue_name = DSTRING_INIT;
      dstring host_domain = DSTRING_INIT;
      bool has_hostname;
      bool has_domain;
      const char *name = lGetString(qref_pattern, QR_name);

      /* split again rather than remember: the filter loop above threw the parts
       * away, and splitting is pure string work on a handful of references */
      (void) cqueue_name_split(name, &cqueue_name, &host_domain, &has_hostname, &has_domain);

      lList *qref_list = nullptr;
      cqueue_list_find_all_matching_references(object_list_, nullptr,
                                               sge_dstring_get_string(&cqueue_name), &qref_list);

      CQueueOutput group;
      if (has_domain || has_hostname) {
         group.qinstances = lCreateList("qinstance_list", QU_Type);
      } else {
         group.cqueues = lCreateList("cqueue_list", CQ_Type);
      }

      if (has_domain) {
         lList *href_list = nullptr;
         hgroup_list_find_matching_and_resolve(hgroup_list_, nullptr,
                                               sge_dstring_get_string(&host_domain), &href_list);
         for_each_ep_lv(qref, qref_list) {
            const lListElem *cqueue = lGetElemStr(object_list_, CQ_name, lGetString(qref, QR_name));
            if (cqueue == nullptr) {
               continue;
            }
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
            for_each_ep_lv(href, href_list) {
               const lListElem *qinstance =
                  lGetElemHost(qinstance_list, QU_qhostname, lGetHost(href, HR_name));
               if (qinstance != nullptr) {
                  lListElem *qi_copy = lCopyElem(qinstance);
                  qconf_json_fill_complex(qi_copy, qu_ce, 3, centry_list_);
                  lAppendElem(group.qinstances, qi_copy);
               }
            }
         }
         lFreeList(&href_list);
      } else if (has_hostname) {
         const char *h_pattern = sge_dstring_get_string(&host_domain);
         const bool is_expr = is_expression(h_pattern);
         for_each_ep_lv(qref, qref_list) {
            const lListElem *cqueue = lGetElemStr(object_list_, CQ_name, lGetString(qref, QR_name));
            if (cqueue == nullptr) {
               continue;
            }
            for_each_ep_lv(qinstance, lGetList(cqueue, CQ_qinstances)) {
               /* sge_eval_expression() returns 0 on a match, as strcmp does */
               if (sge_eval_expression(CEntry::Type::HOST, h_pattern,
                                       lGetHost(qinstance, QU_qhostname), nullptr,
                                       true, is_expr) == 0) {
                  lListElem *qi_copy = lCopyElem(qinstance);
                  qconf_json_fill_complex(qi_copy, qu_ce, 3, centry_list_);
                  lAppendElem(group.qinstances, qi_copy);
               }
            }
         }
      } else {
         for_each_ep_lv(qref, qref_list) {
            const lListElem *cqueue = lGetElemStr(object_list_, CQ_name, lGetString(qref, QR_name));
            if (cqueue != nullptr) {
               lAppendElem(group.cqueues, lCopyElem(cqueue));
            }
         }
      }

      if (lGetNumberOfElem(group.cqueues) == 0 && lGetNumberOfElem(group.qinstances) == 0) {
         answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                                 MSG_CQUEUE_NOQMATCHING_S, name);
         matched_all = false;
      }
      cqueue_output_.push_back(group);

      lFreeList(&qref_list);
      sge_dstring_free(&cqueue_name);
      sge_dstring_free(&host_domain);
   }

   DRETURN(matched_all);
}

/**
 * @brief Fetch what the operation asked for
 *
 * An empty list is an error, and since CS-2929 that no longer depends on the
 * output format: the format decides what reaches stdout, not whether the read
 * succeeded. It is reported without failing the read, so the empty output is
 * still rendered and the answer list is what makes the caller exit non-zero.
 * The message travels back in that answer list rather than going to stderr,
 * because inside qmaster stderr is the daemon's log and not the caller's
 * terminal.
 *
 * @param answer_list for error reporting
 * @param parameter   the operation and how it renders
 * @return true when there is something to show
 */
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
   if (parameter.get_sub_procedure_name() == QConfParameter::CATEGORY_LIST) {
      DRETURN(make_list_snapshot(answer_list, QConfObj::CAT));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::QUEUE_SICK) {
      DRETURN(make_sick_snapshot(answer_list));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::THREAD_POOL_LIST) {
      DRETURN(make_threadpool_snapshot(answer_list));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::SHARETREE_TREE ||
       parameter.get_sub_procedure_name() == QConfParameter::SHARETREE_NODES) {
      DRETURN(make_sharetree_snapshot(answer_list, parameter));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::PROCESSOR_LIST) {
      DRETURN(make_processor_snapshot(answer_list));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::EVENT_CLIENT_LIST) {
      DRETURN(make_eventclient_snapshot(answer_list));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::OBJECT_FILTER) {
      DRETURN(make_sobjl_snapshot(answer_list, parameter));
   }
   if (parameter.get_sub_procedure_name() == QConfParameter::CQUEUE_SHOW) {
      DRETURN(make_cqueue_snapshot(answer_list, parameter));
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

   /* CS-2677 and CS-2823: a cluster queue read in full needs the host groups,
    * because its host list is the member list of "@@<queue>" and the stored
    * object carries none. Without the join the editor of "-mq" would show an
    * empty host list, and saving that text would empty the real one - the read
    * of a round trip has to render what the write will parse. The name list of
    * "-sql" fetches names only and needs nothing.
    *
    * CS-2906: every read that renders the object, which is the export as well.
    * "qconf -Sq" wrote "hostlist NONE" for every queue and re-applying such a
    * file emptied the host list of all of them, reported success and exited 0 -
    * in the very path a backup takes. Asking for the kind that needs no join
    * rather than listing the two that do is what keeps the next cluster queue
    * read from losing it again in silence. */
   const bool cqueue_in_full = (object_ == QConfObj::CQ && kind_ != QConfReadKind::NAME_LIST);
   if (cqueue_in_full && !fetch_hgroups(answer_list)) {
      DRETURN(false);
   }

   lEnumeration *what;
   lCondition *where;
   bool bulk_export = false;   /* no name given and the read feeds an export */
   if (kind_ == QConfReadKind::NAME_LIST) {
      what = get_name_what(desc_->descr, key_nm_);
      where = get_name_where(desc_->descr, key_nm_);
   } else if (kind_ == QConfReadKind::SINGLE_OBJECT_BY_ID) {
      what = lWhat("%T(ALL)", desc_->descr);
      /* strtoull rather than a CULL conversion: the argument is whatever the
       * user typed, and a value that is not a number becomes 0, which matches no
       * category and is reported as "does not exist" - the wording the hand
       * written switch produced for a bad id too. */
      const char *arg = parameter.get_operation().arg(0);
      requested_id_ = (arg != nullptr) ? static_cast<uint32_t>(strtoull(arg, nullptr, 10)) : 0;
      where = lWhere("%T(%I==%u)", desc_->descr, key_nm_, requested_id_);
   } else if (kind_ == QConfReadKind::OBJECT_LIST) {
      /* The names may be one comma separated argument, several arguments, or
       * none at all - and none means every object of the type. */
      what = lWhat("%T(ALL)", desc_->descr);
      where = get_names_where(desc_->descr, key_nm_, parameter.get_operation());
      /* CS-2826: an export over everything leaves out what a restore cannot
       * create, see drop_from_export() - which runs below, once the objects are
       * there, because a predicate cannot be asked inside a condition.
       *
       * Only where no name was given: the user asking for one of those by name
       * gets it, because a name is an instruction and not a sweep. That is also
       * the boundary the refusal of a reduced export needs - a single export is
       * a choice, a bulk export is a backup. */
      bulk_export = (where == nullptr && parameter.get_for_export());
   } else if (kind_ == QConfReadKind::MEMBER_LIST) {
      /* One named object and its sublist. The name comes from the descriptor,
       * not from the command line - the switch itself says which reserved
       * object is meant, and the user cannot name another one. */
      what = lWhat("%T(%I %I)", desc_->descr, key_nm_, desc_->read.member.list_nm);
      where = get_object_where(desc_->descr, key_nm_, desc_->read.member.owner);
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

   if (bulk_export) {
      drop_from_export(object_list_, *desc_, key_nm_);
   }

   if (cqueue_in_full) {
      /* the join itself, see above. A name that matched nothing leaves an empty
       * list here and the template path below builds the object - which owns no
       * group yet, so there is nothing to join on either way. */
      cqueue_list_fill_hostlist(object_list_, hgroup_list_);
   }

   if (kind_ == QConfReadKind::MEMBER_LIST) {
      /* Replace the fetched owner by its members, so that everything below -
       * the sort, the empty check, the views - works on a name list and knows
       * nothing about where the names came from.
       *
       * The sublist is copied before the owner is freed, and an absent owner
       * becomes an empty list rather than a null one: the reserved groups are
       * seeded at every qmaster startup and cannot be deleted, so not finding
       * one means an empty membership, which then takes the normal empty path.
       */
      const lListElem *owner = lFirst(object_list_);
      lList *members = (owner != nullptr)
                       ? lCopyList("", lGetList(owner, desc_->read.member.list_nm))
                       : nullptr;
      lFreeList(&object_list_);
      object_list_ = (members != nullptr) ? members
                                         : lCreateList("", desc_->read.member.descr);
      key_nm_ = desc_->read.member.key_nm;
   }

   if (kind_ == QConfReadKind::NAME_LIST || kind_ == QConfReadKind::MEMBER_LIST) {
      lPSortList(object_list_, "%I+", key_nm_);

      /* An empty list is an error, whatever the output format. The hand written
       * engine made that an error for plain output only, so "-s<obj>l" exited 1
       * and "-s<obj>l -fmt json" exited 0 on the same cluster - one question,
       * two answers, decided by a rendering flag (CS-2929). The format decides
       * what reaches stdout and nothing else.
       *
       * Reported but *not* failed, which is the whole trick: exec_procedure()
       * skips the view when the snapshot fails, so ending the read here would
       * take the enveloped empty array away from a JSON caller and leave it
       * with no document to parse. Returning normally renders it, and the error
       * in the answer list is what the caller turns into a non-zero exit -
       * ProcedureModel::make_snapshot() keeps the response text precisely for
       * output that comes with a complaint (CS-2820).
       *
       * The message travels back in the answer list rather than going to stderr,
       * because inside qmaster stderr is the daemon's log and not the caller's
       * terminal. */
      if (lGetNumberOfElem(object_list_) == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_QCONF_NOXDEFINED_S, desc_->read.list_name);
      }
   } else {
      /* A read naming several objects reports per name, when the type says what
       * to call one - that is what -su always did, one line per userset it could
       * not find. A type without a noun, like the resource quota sets, reports the
       * read as a whole instead and never names anything (CS-2819). */
      /* An export says it itself, and in its own words: the client reports the
       * file it could not write, naming the object and not the type's noun, and
       * two complaints for one cause read as two causes (CS-2826). */
      if (kind_ == QConfReadKind::OBJECT_LIST && parameter.get_operation().arg_count() > 0 &&
          !parameter.get_for_export() &&
          (desc_->read.not_found_noun != nullptr ||
           (parameter.get_for_edit() && desc_->read.create_template != nullptr))) {
         lList *wanted = nullptr;
         for (size_t i = 0; i < parameter.get_operation().arg_count(); i++) {
            lString2List(parameter.get_operation().arg(i), &wanted, ST_Type, ST_name, ", ");
         }
         bool all_found = true;
         for_each_ep_lv(name_elem, wanted) {
            const char *name = lGetString(name_elem, ST_name);
            if (lGetElemStr(object_list_, key_nm_, name) != nullptr) {
               continue;
            }

            /* CS-2823: a read that feeds an editor answers a name that does not
             * exist with the type's template, for a list exactly as for a single
             * object - one template per name that matched nothing. That is what
             * makes "qconf -arqs a,b" the same operation as "-mrqs a,b", and
             * what the old switch did by building an element per name itself. */
            if (parameter.get_for_edit() && desc_->read.create_template != nullptr) {
               lListElem *tmpl = desc_->read.create_template(answer_list, name);
               if (tmpl == nullptr) {
                  /* the constructor said why - a name it refuses must not reach
                   * the editor as a half built template */
                  all_found = false;
                  break;
               }
               if (object_list_ == nullptr) {
                  object_list_ = lCreateList("", desc_->descr);
               }
               lAppendElem(object_list_, tmpl);
               answered_with_template_ = true;
               continue;
            }

            answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                    MSG_SGETEXT_DOESNOTEXIST_SS, desc_->read.not_found_noun, name);
            all_found = false;
         }
         lFreeList(&wanted);
         if (!all_found) {
            DRETURN(false);
         }
      }

      /* CS-2823: a read that feeds an editor answers a name that does not exist
       * with the type's template instead of "x is not a y". That is what makes
       * "-m<obj> <unknown>" offer a template and add it on exit (CS-2299 C2 and
       * its siblings), and what makes "-a<obj>" the same operation as
       * "-m<obj>" - one writer per type, and one reader.
       *
       * It is built here rather than in the client because the text goes
       * straight back through the write: rendering it anywhere else means two
       * renderers for one text format, which is how the exec host came to
       * render attributes its own write cannot parse (see
       * QConfParameter::get_for_edit()). Here the template passes the view, the
       * field list and the format of a stored object of its type.
       *
       * A row without a constructor keeps the report. The exec host is the one
       * such type among the eight: its template is an entry of its own list, so
       * neither "-me" nor "-ae" creates a host from a name. */
      /* CS-2823: a singleton reaches this for the same reason. "qconf -mstree" on
       * a cluster with no share tree opened the editor on the type's template,
       * which is how the first tree was ever written, and the read answers that
       * case now. A singleton has no name to pass to the constructor. Which
       * singletons may legitimately be absent is said by the row: the share tree
       * has a template, the scheduler configuration has none, because a cluster
       * without one is broken rather than new. */
      if ((kind_ == QConfReadKind::SINGLE_OBJECT || kind_ == QConfReadKind::SINGLETON) &&
          parameter.get_for_edit() && desc_->read.create_template != nullptr &&
          (object_list_ == nullptr || lGetNumberOfElem(object_list_) == 0)) {
         lListElem *tmpl = desc_->read.create_template(answer_list,
                                                      (kind_ == QConfReadKind::SINGLETON)
                                                      ? nullptr
                                                      : parameter.get_operation().arg(0));
         if (tmpl == nullptr) {
            /* the constructor said why - a name it refuses must not reach the
             * editor as a half built template */
            DRETURN(false);
         }
         if (object_list_ == nullptr) {
            object_list_ = lCreateList("", desc_->descr);
         }
         lAppendElem(object_list_, tmpl);
         answered_with_template_ = true;
      }

      /* lGetNumberOfElem() would flag a null list as a CULL error, so the list
       * itself is checked first - as the hand written engine did. The wording of
       * the "not found" message is per object type, its shape is not. */
      if (object_list_ == nullptr || lGetNumberOfElem(object_list_) == 0) {
         /* CS-2823: an edit read of a whole list finds nothing on a cluster that
          * has none, and that is not a failure - the editor opens on an empty
          * text, which is how "qconf -mrqs" let an administrator write the first
          * rule set before the switch came here. A named read still reports,
          * through the template above or the message below. */
         if (kind_ == QConfReadKind::OBJECT_LIST && parameter.get_for_edit() &&
             parameter.get_operation().arg_count() == 0) {
            if (object_list_ == nullptr) {
               object_list_ = lCreateList("", desc_->descr);
            }
            DRETURN(true);
         }

         /* CS-2826: and an export finds nothing in two legitimate ways. A bulk
          * run over a type the cluster does not use writes an empty directory
          * and reports zero objects - which is what the client did while it
          * fetched for itself, and what a backup script over all fifteen types
          * depends on. A named one has a message of its own in the client, which
          * names the object the file was to be called after.
          *
          * So the empty result travels back as an empty success either way and
          * the client decides, rather than this read reporting first and the
          * client reporting again. */
         if (parameter.get_for_export()) {
            if (object_list_ == nullptr) {
               object_list_ = lCreateList("", desc_->descr);
            }
            DRETURN(true);
         }

         if (kind_ == QConfReadKind::SINGLETON || kind_ == QConfReadKind::OBJECT_LIST) {
            /* no name was given, so the message takes no argument */
            if (desc_->read.empty_msg != nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                       "%s", desc_->read.empty_msg);
            }
         } else if (kind_ == QConfReadKind::SINGLE_OBJECT_BY_ID) {
            /* the message of a numeric key formats a number, not the text that
             * was typed - passing arg(0) would hand a string to a %u (CS-2819) */
            answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                    desc_->read.not_found_msg, requested_id_);
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
      /* CS-2826: not for an export. The hook types the values a *display* shows,
       * and for the exec host it rewrites complex_values - which is a field the
       * export writes and "-Ae" reads back, so running it would change what the
       * files hold without anything having verified that the import still parses
       * it. The client's own export never ran it, and the migration of the fetch
       * is not the place to find out. It would also run on the first object
       * only, because that is what a single-object read needs, so a directory of
       * exec hosts would come out typed in one file and untyped in the rest. */
      if (desc_->read.prepare != nullptr && !parameter.get_for_export() &&
          parameter.get_output_format() == ProcedureParameter::OutputFormat::JSON) {
         if (!fetch_centries(answer_list)) {
            DRETURN(false);
         }
         desc_->read.prepare(lFirstRW(object_list_), centry_list_);
      }
   }

   DRETURN(true);
}
