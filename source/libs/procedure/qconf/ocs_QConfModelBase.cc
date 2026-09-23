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
#include "sgeobj/config.h"
#include <cstring>

#include "ocs_QConfModelBase.h"

#include "msg_common.h"

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
