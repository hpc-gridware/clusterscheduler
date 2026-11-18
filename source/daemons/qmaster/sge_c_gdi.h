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
 * #gdi_object_t giving its list, its key field and those few functions, and
 * sge_gdi_add_mod_generic() does the rest.
 *
 * That is why a new configuration object needs a table entry rather than a new
 * request handler, and why a bug in the sequence is a bug for every object at
 * once.
 */

#include <utility>

#include "uti/sge_monitor.h"

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "sgeobj/sge_object.h"

#include "sgeobj/sge_daemonize.h"
#include "gdi/ocs_gdi_Packet.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_STU.h"

typedef struct _gdi_object_t gdi_object_t;   ///< @copybrief _gdi_object_t

/**
 * @brief What the on_success hooks of one request leave for after its last object
 *
 * Some work is done once per request rather than once per object, on the final state after the last object
 * of the request is stored. The hooks note it here, the caller of sge_gdi_add_mod_generic() carries it out
 * after its loop over the objects of the request. A caller that has no such step passes nullptr, the hooks
 * then note nothing.
 */
struct ocs_gdi_post_processing {
   ocs::tom::ListPtr<ocs::tom::STU> redebit_centries;   ///< consumables whose debitation is rebuilt once
};

/** @brief Validates one attribute change and applies it to the destination object
 *
 * Called once per attribute the client wants changed.
 */
typedef int (*modifier_func_t)(
        ocs::gdi::Packet *packet,
        ocs::gdi::Task *task,
        ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *new_cal,   /* destination */
        lListElem *cep,       /* reduced element */
        int add,              /* 1 for add/0 for mod */
        const char *ruser,
        const char *rhost,
        gdi_object_t *object, /* some kind of "this" */
        ocs::gdi::Command cmd,
        ocs::gdi::SubCommand sub_com,
        monitoring_t *monitor
);

/** @brief Spools the object once every change has been validated */
typedef int (*writer_func_t)(
        ocs::gdi::Packet *packet,
        ocs::gdi::Task *task,
        ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *ep,      /* new modified element */
        gdi_object_t *thiz   /* some kind of "this" */
);

/** @brief allows to retrieve a master list */
typedef lList **(*getMasterList)();

/** @brief Does whatever has to happen once the object is safely spooled
 *
 * Anything that must not be done before the change is on disk - announcing it
 * to the event clients, triggering a scheduling run - belongs here rather than
 * in the modifier.
 */
typedef int (*on_success_func_t)(
        ocs::gdi::Packet *packet,
        ocs::gdi::Task *task,
        lListElem *ep,       /* new modified and already spooled element */
        lListElem *old_ep,   /* old element is nullptr in add case */
        gdi_object_t *thiz,  /* some kind of "this" */
        ocs_gdi_post_processing *post,   /* what is left for after the last object, or nullptr */
        monitoring_t *monitor  /* monitoring structure */
);

/** @brief Everything the generic request path needs to know about one object type */
struct _gdi_object_t {
   ocs::gdi::Target target;          ///< Which GDI target this is, e.g. `SGE_QUEUE_LIST`
   int key_nm;          ///< The field that identifies an object, e.g. `QU_qname`
   const lDescr *type;           ///< The CULL type, e.g. `QU_Type`
   const char *object_name;    ///< Its name in messages, e.g. `"queue"`
   sge_object_type list_type;       ///< identifier to retrive the master list via ocs::DataStore::get_master_list
   modifier_func_t modifier;        ///< responsible for validating each our attribute modifier
   writer_func_t writer;          ///< function that spools our object
   on_success_func_t on_success;      ///< do everything what has to be done on successful writing
};

/** @brief The table entry for a GDI target, nullptr for a target without generic handling */
gdi_object_t *get_gdi_object(ocs::gdi::Target);

/**
 * @brief The gdi_object_t::modifier of a table entry: calls the typed modifier of the entry's object type
 *
 * Has exactly the signature of #modifier_func_t, so the table keeps its raw function pointers while every
 * modifier is typed. The raw elements are taken into handles without a check: the table entry pairs @p F
 * with @p T, the new element is created with the entry's type or copied from its master list, and the
 * request is structure-checked by the listener (CS-3125) or built by qmaster for that entry.
 *
 * @tparam T the object type of the table entry
 * @tparam F the typed modifier, taking an `Element<T>` destination and an `Element<T>` or `ConstElement<T>`
 *           request in place of the raw elements, the other parameters as #modifier_func_t
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param new_ep the object as it is to become (of type T)
 * @param request the reduced element the client sent (of type T)
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param object the table entry
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return what @p F returns: 0 on success
 */
template <ocs::tom::ObjectType T, auto F>
int
ocs_gdi_modifier(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                 lListElem *new_ep, lListElem *request, const int add, const char *ruser, const char *rhost,
                 gdi_object_t *object, const ocs::gdi::Command cmd, const ocs::gdi::SubCommand sub_command,
                 monitoring_t *monitor) {
   // the types are guaranteed by the table entry and the listener's structure check, see above
   return F(packet, task, alpp, ocs::tom::Element<T>::from_typed(new_ep), ocs::tom::Element<T>::from_typed(request),
            add, ruser, rhost, object, cmd, sub_command, monitor);
}

/**
 * @brief The gdi_object_t::writer of a table entry: calls the typed writer of the entry's object type
 *
 * Has exactly the signature of #writer_func_t; the element is the one the modifier has built, of the entry's
 * type (see ocs_gdi_modifier()), and is taken into a handle without a check.
 *
 * @tparam T the object type of the table entry
 * @tparam F the typed writer, taking an `Element<T>` or `ConstElement<T>` in place of the raw element
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param ep the object to spool (of type T)
 * @param object the table entry
 * @return what @p F returns: 0 on success
 */
template <ocs::tom::ObjectType T, auto F>
int
ocs_gdi_writer(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
               lListElem *ep, gdi_object_t *object) {
   // the type is guaranteed by the table entry, see ocs_gdi_modifier()
   return F(packet, task, alpp, ocs::tom::Element<T>::from_typed(ep), object);
}

/**
 * @brief The gdi_object_t::on_success of a table entry: calls the typed on_success of the entry's object type
 *
 * Has exactly the signature of #on_success_func_t; the new element is the spooled one of the entry's type,
 * the old one comes from the entry's master list, so both are taken into handles without a check.
 *
 * @tparam T the object type of the table entry
 * @tparam F the typed on_success, taking an `Element<T>` and a `ConstElement<T>` in place of the raw elements
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the new, already spooled object (of type T)
 * @param old_ep the object as it was (of type T), nullptr on add
 * @param object the table entry
 * @param post what is left for after the last object of the request, or nullptr
 * @param monitor for monitoring qmaster threads
 * @return what @p F returns: 0 on success
 */
template <ocs::tom::ObjectType T, auto F>
int
ocs_gdi_on_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep,
                   gdi_object_t *object, ocs_gdi_post_processing *post, monitoring_t *monitor) {
   // the types are guaranteed by the table entry and its master list, see above
   return F(packet, task, ocs::tom::Element<T>::from_typed(ep), ocs::tom::ConstElement<T>::from_typed(old_ep),
            object, post, monitor);
}

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

int
sge_gdi_add_mod_generic(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                        lListElem *instructions, int add, gdi_object_t *object, const char *ruser,
                        const char *rhost, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
                        ocs_gdi_post_processing *post, monitoring_t *monitor);

void sge_clean_lists();

