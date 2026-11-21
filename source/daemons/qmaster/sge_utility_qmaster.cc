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
#include <cstring>
#include <cctype>

#include "uti/sge.h"               // INFINITY_STR
#include "uti/config_file.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"

#include "tom/ocs_Types.h"

#include "gdi/ocs_gdi_Command.h"

#include "comm/lists/cl_errors.h"
#include "comm/cl_commlib.h"

#include "sgeobj/ocs_AdvanceReservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "sge_utility_qmaster.h"

/**
 * @brief Modify the prolog/epilog/pe_start/pe_stop procedures
 *
 * This function modifies a prolog/epilog/pe_start/pe_stop of "new_ep".
 * The attribute is the template argument. Possible errors will be reported
 * in "alpp". The "qep" element will be used to identify the changes which
 * have been done. "attr_name" is used to report errors.
 *
 * @tparam F the string field holding the procedure, of the object type of both elements
 * @param alpp AN, The answer list
 * @param qep reduced changes source element
 * @param new_ep target element
 * @param attr_name the attribute's name, for the messages
 * @param variables the procedure variables that may appear in the value
 *
 * @return 0 success, error othewise
 *
 * @note MT-NOTE: attr_mod_procedure() is MT safe
 */
template <auto F>
int
attr_mod_procedure(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                   const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> qep,
                   const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> new_ep,
                   const char *attr_name, const char *variables[]) {
   DENTER(TOP_LAYER);

   /* ---- attribute F */
   if (qep.template has<F>()) {
      DPRINTF("got new %s\n", attr_name);

      const char *s = qep.template get<F>();
      if (s) {
         char *t;
         char *script = (char *) s;

         /* skip user name */
         if ((t = strpbrk(script, "@ ")) && *t == '@') {
            script = &t[1];
         }

         /* ensure that variables are valid */
         if (replace_params(script, nullptr, 0, variables)) {
            ERROR(MSG_GDI_VARS_SS, attr_name, err_msg);
            answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }

         // force use of absolut path's
         // we might also see a $ sign from a special variable, whose validity has been checked above
         if (script[0] != '/' && script[0] != '$') {
            ERROR(MSG_GDI_APATH_S, attr_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }
      new_ep.template set<F>(s);
   }

   DRETURN(0);
}

/// @cond explicit instantiations for the callers in sge_pe_qmaster.cc
template int attr_mod_procedure<PE_start_proc_args>(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                                    ocs::tom::ConstElement<ocs::tom::PE> qep,
                                                    ocs::tom::Element<ocs::tom::PE> new_ep,
                                                    const char *attr_name, const char *variables[]);
template int attr_mod_procedure<PE_stop_proc_args>(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                                   ocs::tom::ConstElement<ocs::tom::PE> qep,
                                                   ocs::tom::Element<ocs::tom::PE> new_ep,
                                                   const char *attr_name, const char *variables[]);
/// @endcond

/**
 * @brief The mode of ocs::tom::modify_sublist() a GDI sub-command asks for
 * @param sub_cmd the GDI sub-command
 * @return its change, append and remove flags; none for a sub-command that sets the whole sub-list
 */
ocs::tom::SublistMode
ocs_sublist_mode(const ocs::gdi::SubCommand sub_cmd) {
   DENTER(TOP_LAYER);

   ocs::tom::SublistMode mode;
   mode.change = (sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE;
   mode.append = (sub_cmd & ocs::gdi::SubCommand::APPEND) == ocs::gdi::SubCommand::APPEND;
   mode.remove = (sub_cmd & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE;

   DRETURN(mode);
}

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
ocs_sublist_answer(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::SublistEvent event, const char *key,
                   const ocs::gdi::SubCommand sub_cmd, const char *sub_list_name, const char *object_name) {
   DENTER(TOP_LAYER);

   const bool change = (sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE;
   switch (event) {
      case ocs::tom::SublistEvent::MissingKey:
         ERROR(SFNMAX, MSG_OBJECT_VALUEMISSING);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         break;
      case ocs::tom::SublistEvent::AlreadyHeld:
         INFO(MSG_OBJECT_ALREADYEXIN_SSS, key, sub_list_name, object_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         break;
      case ocs::tom::SublistEvent::NotHeld:
         INFO(SFQ " does not exist in " SFQ " of " SFQ, key, sub_list_name, object_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         break;
      case ocs::tom::SublistEvent::CreatedList:
         if (change) {
            INFO(SFQ " of " SFQ " is empty - Adding new element(s).", sub_list_name, object_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }
         break;
      case ocs::tom::SublistEvent::Appended:
         if (change) {
            INFO("Unable to find " SFQ " in " SFQ " of " SFQ " - Adding new element.", key, sub_list_name,
                 object_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
         }
         break;
   }

   DRETURN_VOID;
}

/**
 * @brief Apply a binding change to an advance reservation
 *
 * @param alpp receives messages for the caller
 * @param new_ar the advance reservation as it is to become (AR)
 * @param ar the reduced element the client sent (AR)
 *
 * @return true on success
 */
bool
attr_mod_obj_binding(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<ocs::tom::AR> new_ar,
                     const ocs::tom::ConstElement<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (ar.has<AR_binding>()) {
      const auto old_binding_elem = new_ar.get<AR_binding>();
      const auto new_binding_elem = ar.get<AR_binding>();
      if (old_binding_elem == nullptr) {
         // clone() keeps an empty handle empty, as the former nullptr check did
         new_ar.set<AR_binding>(new_binding_elem.clone());
      } else {
         if (new_binding_elem != nullptr) {
            // copy new amount but only if it is not UNINITIALIZED and different from the old one
            uint32_t new_amount = new_binding_elem.get<BN_amount>();
            if (new_amount != static_cast<uint32_t>(-1)) {
               uint32_t old_amount = ocs::AdvanceReservation::binding_get_amount(new_ar);
               if (old_amount != new_amount) {
                  old_binding_elem.set<BN_amount>(new_amount);
               }
            }

            // copy new instance but only if it is not UNINITIALIZED and different from the old one
            auto new_instance = static_cast<ocs::BindingInstance::Instance>(new_binding_elem.get<BN_instance>());
            if (new_instance != ocs::BindingInstance::Instance::UNINITIALIZED) {
               ocs::BindingInstance::Instance old_instance = ocs::AdvanceReservation::binding_get_instance(new_ar);
               if (old_instance != new_instance) {
                  old_binding_elem.set<BN_instance>(new_instance);
               }
            }

            // copy new unit but only if it is not UNINITIALIZED and different from the old one
            auto new_unit = static_cast<ocs::BindingUnit::Unit>(new_binding_elem.get<BN_unit>());
            if (new_unit != ocs::BindingUnit::Unit::UNINITIALIZED) {
               ocs::BindingUnit::Unit old_unit = ocs::AdvanceReservation::binding_get_unit(new_ar);
               if (old_unit != new_unit) {
                  old_binding_elem.set<BN_unit>(new_unit);
               }
            }

            // copy new type but only if it is not UNINITIALIZED and different from the old one
            auto new_type = static_cast<ocs::BindingType::Type>(new_binding_elem.get<BN_new_type>());
            if (new_type != ocs::BindingType::Type::UNINITIALIZED) {
               ocs::BindingType::Type old_type = ocs::AdvanceReservation::binding_get_type(new_ar);
               if (old_type != new_type) {
                  old_binding_elem.set<BN_new_type>(new_type);
               }
            }

            // copy new filter but only if it is not UNINITIALIZED and different from the old one
            const char *new_filter = new_binding_elem.get<BN_filter>();
            if (new_filter != nullptr) {
               std::string old_filter = ocs::AdvanceReservation::binding_get_filter(new_ar);
               if (strcmp(old_filter.c_str(), new_filter) != 0) {
                  old_binding_elem.set<BN_filter>(new_filter);
               }
            }

            // copy new sort but only if it is not UNINITIALIZED and different from the old one
            const char *new_sort = new_binding_elem.get<BN_sort>();
            if (new_sort != nullptr) {
               std::string old_sort = ocs::AdvanceReservation::binding_get_sort(new_ar);
               if (old_sort != new_sort) {
                  old_binding_elem.set<BN_sort>(new_sort);
               }
            }

            // copy new start but only if it is not UNINITIALIZED and different from the old one
            auto new_start = static_cast<ocs::BindingStart::Start>(new_binding_elem.get<BN_start>());
            if (new_start != ocs::BindingStart::Start::UNINITIALIZED) {
               ocs::BindingStart::Start old_start = ocs::AdvanceReservation::binding_get_start(new_ar);
               if (old_start != new_start) {
                  old_binding_elem.set<BN_start>(new_start);
               }
            }

            // copy new end but only if it is not UNINITIALIZED and different from the old one
            auto new_stop = static_cast<ocs::BindingStop::Stop>(new_binding_elem.get<BN_stop>());
            if (new_stop != ocs::BindingStop::Stop::UNINITIALIZED) {
               ocs::BindingStop::Stop old_stop = ocs::AdvanceReservation::binding_get_stop(new_ar);
               if (old_stop != new_stop) {
                  old_binding_elem.set<BN_stop>(new_stop);
               }
            }

            // copy new strategy but only if it is not UNINITIALIZED and different from the old one
            auto new_strategy = static_cast<ocs::BindingStrategy::Strategy>(new_binding_elem.get<BN_strategy>());
            if (new_strategy != ocs::BindingStrategy::Strategy::UNINITIALIZED) {
               ocs::BindingStrategy::Strategy old_strategy = ocs::AdvanceReservation::binding_get_strategy(new_ar);
               if (old_strategy != new_strategy) {
                  old_binding_elem.set<BN_strategy>(new_strategy);
               }
            }
         }

         // if the new binding has no amount, we remove binding completely
         if (ocs::AdvanceReservation::binding_get_amount(new_ar) == 0) {
            new_ar.set<AR_binding>(nullptr);
         }
      }

      // make sure that all binding fields have valid values
      // or apply implicit binding if enabled
      ocs::AdvanceReservation::binding_set_missing_defaults(new_ar, alpp);
   }
   DRETURN(ret);
}



/**
 * @brief Reject a request that names the same key twice
 *
 * Two entries for one key in a sublist would leave the result depending on
 * which the code happened to read first.
 *
 * @tparam F the string key field of the list elements
 * @param alpp receives messages for the caller
 * @param lp1 the first list to compare
 * @param lp2 the second list to compare
 * @param name the attribute's name, for the message
 * @param obj_name the object's name, for the message
 * @return non-zero when a key occurs more than once
 */
template <auto F>
int
multiple_occurances(ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                    const ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> lp1,
                    const ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> lp2, const char *name,
                    const char *obj_name) {
   DENTER(TOP_LAYER);

   if (!lp1 || !lp2) {
      DRETURN(0);
   }

   for (const auto ep1 : lp1) {
      const char *s = ep1.template get<F>();
      if (lp2.template find_first<F>(s)) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_MULTIPLE_OCCUR_SSSS,
                  (F == US_name) ? MSG_OBJ_USERSET : MSG_JOB_PROJECT, s, obj_name, name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

/// @cond explicit instantiation for the callers in sge_pe_qmaster.cc and sge_userprj_qmaster.cc
template int multiple_occurances<US_name>(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ConstList<ocs::tom::US> lp1,
                                          ocs::tom::ConstList<ocs::tom::US> lp2, const char *name,
                                          const char *obj_name);
/// @endcond
