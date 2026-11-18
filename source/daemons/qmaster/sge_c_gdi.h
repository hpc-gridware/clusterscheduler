#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The generic GDI request path: one table, every configuration object
 *
 * Adding, modifying and deleting a calendar, a queue, a user or a project is
 * the same sequence every time - validate, spool, announce - and only the
 * validation and the spooling differ. So each object type contributes one
 * #gdi_object_t giving its list, its key field and its name, plus its typed
 * modifier, writer and on_success functions (ocs_gdi_hooks in sge_c_gdi.cc),
 * and ocs_gdi_add_mod() does the rest, compiled once per object type.
 *
 * That is why a new configuration object needs a table entry rather than a new
 * request handler, and why a bug in the sequence is a bug for every object at
 * once.
 */

#include <concepts>
#include <utility>

#include "uti/sge_monitor.h"

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/ocs_object_type.h"

#include "sgeobj/sge_daemonize.h"
#include "gdi/ocs_gdi_Packet.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_STU.h"

typedef struct _gdi_object_t gdi_object_t;   ///< @copybrief _gdi_object_t

/**
 * @brief What the on_success hooks of one request leave for after its last object
 *
 * Some work is done once per request rather than once per object, on the final state after the last object
 * of the request is stored. The hooks note it here, the caller of ocs_gdi_add_mod() carries it out
 * after its loop over the objects of the request. A caller that has no such step passes nullptr, the hooks
 * then note nothing.
 */
struct ocs_gdi_post_processing {
   ocs::tom::ListPtr<ocs::tom::STU> redebit_centries;   ///< consumables whose debitation is rebuilt once
};

/**
 * @brief Everything the generic request path needs to know about one object type at run time
 *
 * The functions that validate, spool and announce an object are not part of the entry: they are typed and are
 * bound to the object type at compile time (ocs_gdi_hooks in sge_c_gdi.cc), see ocs_gdi_add_mod().
 */
struct _gdi_object_t {
   ocs::gdi::Target target;          ///< Which GDI target this is, e.g. `SGE_QUEUE_LIST`
   int key_nm;          ///< The field that identifies an object, e.g. `QU_qname`
   const lDescr *type;           ///< The CULL type, e.g. `QU_Type`
   const char *object_name;    ///< Its name in messages, e.g. `"queue"`
   sge_object_type list_type;       ///< identifier to retrive the master list via ocs::DataStore::get_master_list
};

/** @brief The table entry for a GDI target, nullptr for a target without generic handling */
gdi_object_t *get_gdi_object(ocs::gdi::Target);

bool
sge_c_gdi_process_in_listener(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list, monitoring_t *monitor, bool has_next);

bool
sge_c_gdi_check_execution_permission(ocs::gdi::Packet *packet, ocs::gdi::Task *task);

/** @brief Check that the user of a request may modify objects of a list
 *
 * A manager for the configuration lists, the access lists included; see the
 * definition for the rest. A refusal is added to @p alpp.
 *
 * @param packet the request
 * @param alpp   receives the refusal
 * @param target the object list
 * @return true when the user may modify the list
 */
bool
sge_chck_mod_perm_user(const ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::gdi::Target target);

/** @brief Check that the host of a request may modify objects of a list
 *
 * An admin host for every configuration list; see the definition for the
 * exceptions. A refusal is added to @p alpp.
 *
 * @param packet  the request
 * @param alpp    receives the refusal
 * @param target  the object list
 * @param command the command of the request
 * @return true when the host may modify the list
 */
bool
sge_chck_mod_perm_host(const ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::gdi::Target target,
                       ocs::gdi::Command command);

/** @brief Delete one object, or the whole share tree, the way a GDI delete does
 *
 * What sge_c_gdi_del() does for each object of its data list, for the qconf
 * delete procedure (CS-2983).
 *
 * @param packet      the client request
 * @param task        the GDI task being answered
 * @param target      the object list
 * @param ep          the object to delete, carrying its name; nullptr deletes the
 *                    whole list, which only the share tree supports
 * @param alpp        receives what the delete has to say
 * @param cmd         the command of the request
 * @param sub_command what kind of delete this is
 * @param monitor     for monitoring qmaster threads
 */
void
sge_c_gdi_del_element(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::gdi::Target target, lListElem *ep,
                      ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
                      monitoring_t *monitor);

void
sge_c_gdi_process_in_worker(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list, monitoring_t *monitor, bool has_next);

/**
 * @brief The object types that have a generic add and modify: those whose table entry has typed hooks
 *
 * Exactly the types with a specialization of ocs_gdi_hooks in sge_c_gdi.cc, for which ocs_gdi_add_mod() is
 * instantiated there.
 *
 * @tparam T the object type
 */
template <typename T>
concept OcsGdiGenericType =
   std::same_as<T, ocs::tom::CAL> || std::same_as<T, ocs::tom::EH> || std::same_as<T, ocs::tom::CQ> ||
   std::same_as<T, ocs::tom::CE> || std::same_as<T, ocs::tom::PE> || std::same_as<T, ocs::tom::UU> ||
   std::same_as<T, ocs::tom::US> || std::same_as<T, ocs::tom::PR> || std::same_as<T, ocs::tom::CK> ||
   std::same_as<T, ocs::tom::RQS> || std::same_as<T, ocs::tom::HGRP> || std::same_as<T, ocs::tom::AR> ||
   std::same_as<T, ocs::tom::RL>;

template <OcsGdiGenericType T>
int
ocs_gdi_add_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                ocs::tom::Element<T> request, int add, gdi_object_t *object, const char *ruser, const char *rhost,
                ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, ocs_gdi_post_processing *post,
                monitoring_t *monitor);

int
ocs_gdi_add_mod_unsupported(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const gdi_object_t *object);

void sge_clean_lists();

