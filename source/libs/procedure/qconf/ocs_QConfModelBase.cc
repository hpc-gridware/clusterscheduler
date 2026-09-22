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
 * `-sconfl` and `-sss` are absent on purpose: CONF and EV have no row in the
 * object table, which is why CS-2796 left them hand written in the client.
 */
static const ocs::qconf_read_entry qconf_read_table[] = {
   {"-scall",  QConfObj::CAL},
   {"-sckptl", QConfObj::CK},
   {"-scel",   QConfObj::CE},
   {"-sel",    QConfObj::EH},
   {"-shgrpl", QConfObj::HGRP},
   {"-spl",    QConfObj::PE},
   {"-sprjl",  QConfObj::PR},
   {"-sql",    QConfObj::CQ},
   {"-srolel", QConfObj::RL},
   {"-srqsl",  QConfObj::RQS},
   {"-sul",    QConfObj::US},
   {"-suserl", QConfObj::UU},
};

static constexpr size_t qconf_read_table_size =
   sizeof(qconf_read_table) / sizeof(qconf_read_table[0]);

ocs::QConfModelBase::~QConfModelBase() {
   lFreeList(&object_list_);
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
   desc_ = &qconf_desc(entry->object);
   key_nm_ = desc_->name_nm;

   lEnumeration *what = get_name_what(desc_->descr, key_nm_);
   lCondition *where = get_name_where(desc_->descr, key_nm_);
   const bool ok = fetch(answer_list, desc_->target, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);
   if (!ok) {
      DRETURN(false);
   }

   lPSortList(object_list_, "%I+", key_nm_);

   if (lGetNumberOfElem(object_list_) == 0 &&
       parameter.get_output_format() != ProcedureParameter::OutputFormat::JSON) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_QCONF_NOXDEFINED_S, desc_->read.list_name);
      DRETURN(false);
   }

   DRETURN(true);
}
