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
 * @brief Attribute modification helpers shared by the configuration objects
 */


#include <type_traits>

#include "tom/ocs_Types.h"
#include "gdi/ocs_gdi_Command.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_daemonize.h"
#include "sgeobj/sge_object.h"
#include "uti/sge_rmon_macros.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_FreeAccess.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"

template <auto F>
int attr_mod_procedure(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                       ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> qep,
                       ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> new_ep,
                       const char *attr_name, const char *variables[]);

template <auto F>
int multiple_occurances(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                        ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> lp1,
                        ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> lp2, const char *name,
                        const char *obj_name);

/**
 * @brief Drop an empty sublist so it is not spooled as an empty element
 * @tparam F the sublist field, of the element's object type
 * @param ep the object
 */
template <auto F>
   requires (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::List && ocs::tom::FieldTraits<F>::has_sub)
void normalize_sublist(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> ep) {
   if (const auto lp = ocs::tom::get<F>(ep); lp && lp.empty()) {
      ocs::tom::set<F>(ep, nullptr);
   }
}

/**
 * @brief The mode of ocs::tom::modify_sublist() a GDI sub-command asks for
 * @param sub_cmd the GDI sub-command
 * @return its change, append and remove flags; none for a sub-command that sets the whole sub-list
 */
ocs::tom::SublistMode
ocs_sublist_mode(ocs::gdi::SubCommand sub_cmd);

/**
 * @brief Write the answer a sub-list modification gives for one of its events
 *
 * The messages attr_mod_sub_list() has always written: an error for a missing key, an info for a key held
 * already on append, for a key not held on remove, and - only for a change - for a sub-list created and for
 * a key appended.
 *
 * @param alpp receives the answer
 * @param event what happened
 * @param key the key it happened to, nullptr for a missing key
 * @param sub_cmd the GDI sub-command of the modification
 * @param sub_list_name the sub-list's name, for the message
 * @param object_name the object's name, for the message
 */
void
ocs_sublist_answer(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::SublistEvent event, const char *key,
                   ocs::gdi::SubCommand sub_cmd, const char *sub_list_name, const char *object_name);

/**
 * @brief Apply a sub-list of a modify request to the same sub-list of an object, as the GDI sub-command says
 *
 * ocs::tom::modify_sublist() with the mode of @p sub_cmd; a held element is changed when
 * object_has_differences() finds a difference, and every event is answered by ocs_sublist_answer().
 * The request's sub-list is consumed.
 *
 * @tparam ListF the sub-list field, of the object's type
 * @tparam SubKeyF the key field of the sub-list's elements, a string or host field
 * @tparam T the object type, deduced from @p this_elem
 * @param alpp receives the answers
 * @param this_elem the object to modify
 * @param delta_elem the (probably reduced) request element; its sub-list is consumed
 * @param sub_cmd the GDI sub-command: set, change, append or remove
 * @param sub_list_name the sub-list's name, for the messages
 * @param object_name the object's name, for the messages
 * @return whether the modification succeeded and whether it changed the object's sub-list
 */
template <auto ListF, auto SubKeyF, ocs::tom::ObjectType T>
ocs::tom::SublistOutcome
attr_mod_sub_list(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<T> this_elem,
                  const std::type_identity_t<ocs::tom::Element<T>> delta_elem, const ocs::gdi::SubCommand sub_cmd,
                  const char *sub_list_name, const char *object_name) {
   DENTER(TOP_LAYER);

   const ocs::tom::SublistOutcome outcome = ocs::tom::modify_sublist<ListF, SubKeyF>(
      this_elem, delta_elem, ocs_sublist_mode(sub_cmd),
      [](const auto request_ep, const auto held_ep) {
         return object_has_differences(request_ep, nullptr, held_ep);
      },
      [&](const ocs::tom::SublistEvent event, const char *key) {
         ocs_sublist_answer(alpp, event, key, sub_cmd, sub_list_name, object_name);
      });

   DRETURN(outcome);
}

/**
 * @brief Replace a sub-list of an object by a copy of the request's, as the SET sub-command does
 *
 * For the sub-lists whose elements have no key field the object's own key could name: the advance reservation
 * and resource quota set modifiers. Without a flag in @p sub_cmd the object's sub-list is replaced by a copy of
 * the request's; an empty copy, or none, is stored as an unset list.
 *
 * A keyed sub-command (change, append or remove) changes nothing and answers as the former run-time
 * attr_mod_sub_list() did, to which these callers passed their object's name field (AR_name, RQS_name) as the
 * key of the sub-list's elements: such a key is missing in every element. Its key walk reported
 * ocs_sublist_answer()'s missing key once for every element of the object's sub-list compared with the
 * request's first element, or, if the object's sub-list held no element, once for every element of the
 * request's sub-list. A keyed sub-command whose request sub-list holds no element answers nothing and keeps
 * the object's sub-list; only an empty one is removed.
 *
 * @tparam ListF the sub-list field, of the object's type, with a model sub-type
 * @tparam T the object type, deduced from @p this_elem
 * @param alpp receives the answers of a keyed sub-command
 * @param this_elem the object to modify
 * @param delta_elem the (probably reduced) request element; it is not changed
 * @param sub_cmd the GDI sub-command
 * @param sub_list_name the sub-list's name, for the messages
 * @param object_name the object's name, for the messages
 * @return false if the request lacks the sub-list or a keyed sub-command met a missing key, true otherwise
 */
template <auto ListF, ocs::tom::ObjectType T>
   requires (ocs::tom::FieldTraits<ListF>::type == ocs::tom::FieldType::List && ocs::tom::FieldTraits<ListF>::has_sub)
bool
replace_sublist(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<T> this_elem,
                const std::type_identity_t<ocs::tom::ConstElement<T>> delta_elem, const ocs::gdi::SubCommand sub_cmd,
                const char *sub_list_name, const char *object_name) {
   DENTER(TOP_LAYER);

   if (!ocs::tom::has<ListF>(delta_elem)) {
      DRETURN(false);
   }

   if (const ocs::tom::SublistMode mode = ocs_sublist_mode(sub_cmd); mode.change || mode.append || mode.remove) {
      const auto request_list = ocs::tom::get<ListF>(delta_elem);
      if (!request_list.empty()) {
         const auto held_list = ocs::tom::get<ListF>(ocs::tom::ConstElement<T>(this_elem));
         const std::size_t missing = held_list.empty() ? request_list.size() : held_list.size();
         for (std::size_t i = 0; i < missing; i++) {
            ocs_sublist_answer(alpp, ocs::tom::SublistEvent::MissingKey, nullptr, sub_cmd, sub_list_name,
                               object_name);
         }
         DRETURN(false);
      }
   } else {
      ocs::tom::set<ListF>(this_elem, ocs::tom::get<ListF>(delta_elem).clone(""));
   }

   // a sub-list left without elements is removed
   normalize_sublist<ListF>(this_elem);

   DRETURN(true);
}

bool
attr_mod_obj_binding(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::AR> new_ar,
                     ocs::tom::ConstElement<ocs::tom::AR> ar);

