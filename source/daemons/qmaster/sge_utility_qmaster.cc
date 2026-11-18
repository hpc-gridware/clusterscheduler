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
#include "uti/sge_hostname.h"

#include "cull/cull.h"

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

/** @brief Debug layer the cluster queue helpers log under */
#define CQUEUE_LAYER TOP_LAYER

/**
 * @brief Modify the prolog/epilog/pe_start/pe_stop procedures
 *
 * This function modifies a prolog/epilog/pe_start/pe_stop of "new_ep".
 * The attribute is the template argument. Possible errors will be reported
 * in "alpp". The "qep" element will be used to identify the changes which
 * have been done. "attr_name" is used to report errors.
 *
 * @tparam F the string field holding the procedure, of the object type of both elements
 * @param alpp AN_Type, The answer list
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
   if (ocs::tom::has<F>(qep)) {
      DPRINTF("got new %s\n", attr_name);

      const char *s = ocs::tom::get<F>(qep);
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
      ocs::tom::set<F>(new_ep, s);
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
 * @brief This function modifies a certain configuration sublist
 *
 * This function modifies a certain configuration sublist of "this_elem".
 * Possible errors will be reported in answer_list "alpp". The reduced_elem
 * "delta_elem" will be used to identify the changes which have been made.
 * "sub_command" defines how these changes should be applied to "this_elem".
 * A removal removes every element of the sublist that holds the key.
 *
 * @param alpp The AN_Type, answer_list
 * @param this_elem The target object element, CQ_Type
 * @param this_elem_name The name of the list elemet (lList)
 * @param this_elem_primary_key The primary field for sublist
 * @param delta_elem The source (probably reduced) list of the elements, CQ_Type
 * @param sub_cmd The add, modify, remove command, GDI subcommand
 * @param sub_list_name The sublist name
 * @param object_name The target object name
 * @param no_info Skip or add the info messages
 * @param changed Report back if changes have been made (may be nullptr)
 * @param cmd the command being executed
 *
 * @return true, the success
 *
 * @note MT-NOTE: attr_mod_sub_list() is MT safe
 */
bool
attr_mod_sub_list(ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *this_elem, int this_elem_name,
                  int this_elem_primary_key,
                  const lListElem *delta_elem, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_cmd,
                  const char *sub_list_name, const char *object_name, int no_info, bool *changed) {
   DENTER(TOP_LAYER);

   bool ret = true;
   bool did_changes = false;

   if (lGetPosViaElem(delta_elem, this_elem_name, SGE_NO_ABORT) >= 0) {
      if ((sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE ||
          (sub_cmd & ocs::gdi::SubCommand::APPEND) == ocs::gdi::SubCommand::APPEND ||
          (sub_cmd & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE) {
         lList *reduced_sublist;
         lList *full_sublist;
         lListElem *reduced_element, *next_reduced_element;
         lListElem *full_element, *next_full_element;

         reduced_sublist = lGetListRW(delta_elem, this_elem_name);
         full_sublist = lGetListRW(this_elem, this_elem_name);
         next_reduced_element = lFirstRW(reduced_sublist);
         /*
         ** we try to find each element of the delta_elem
         ** in the sublist if this_elem. Elements which can be found
         ** will be moved into sublist of this_elem.
         */
         while ((reduced_element = next_reduced_element)) {
            int restart_loop = 0;
            bool remove_reduced = false;

            next_reduced_element = lNextRW(reduced_element);
            next_full_element = lFirstRW(full_sublist);
            while ((full_element = next_full_element)) {
               int pos, type;
               const char *rstring = nullptr, *fstring = nullptr;

               next_full_element = lNextRW(full_element);

               pos = lGetPosViaElem(reduced_element, this_elem_primary_key, SGE_NO_ABORT);
               type = lGetPosType(lGetElemDescr(reduced_element), pos);
               if (type == lStringT) {
                  rstring = lGetString(reduced_element, this_elem_primary_key);
                  fstring = lGetString(full_element, this_elem_primary_key);
               } else if (type == lHostT) {
                  rstring = lGetHost(reduced_element, this_elem_primary_key);
                  fstring = lGetHost(full_element, this_elem_primary_key);
               }

               if (rstring == nullptr || fstring == nullptr) {
                  ERROR(SFNMAX, MSG_OBJECT_VALUEMISSING);
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
                  ret = false;
               }

               if (ret &&
                   (((type == lStringT) && strcmp(rstring, fstring) == 0) ||
                    ((type == lHostT) && sge_hostcmp(rstring, fstring) == 0))) {
                  lListElem *new_sub_elem;
                  lListElem *old_sub_elem;

                  /* element already exists */
                  next_reduced_element = lNextRW(reduced_element);
                  if ((sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE) {
                     if (object_has_differences(reduced_element, nullptr, full_element)) {
                        /* new object differs from old one - exchange them */
                        did_changes = true;
                        new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                        old_sub_elem = lDechainElem(full_sublist, full_element);
                        lFreeElem(&old_sub_elem);
                        lAppendElem(full_sublist, new_sub_elem);
                     } else {
                        /* objects do not differ - delete the new one */
                        new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                        lFreeElem(&new_sub_elem);
                     }

                     restart_loop = 1;
                     break;
                  } else if ((sub_cmd & ocs::gdi::SubCommand::APPEND) == ocs::gdi::SubCommand::APPEND) {
                     if (!no_info) {
                        INFO(MSG_OBJECT_ALREADYEXIN_SSS, rstring, sub_list_name, object_name);
                        answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                        ret = false;
                     }

                     new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                     lFreeElem(&new_sub_elem);

                     restart_loop = 1;
                     break;
                  } else if ((sub_cmd & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE) {
                     // remove every element with the key, so that a key held twice (e.g. in a spool
                     // written before CS-3053) cannot survive its removal
                     did_changes = true;
                     old_sub_elem = lDechainElem(full_sublist, full_element);
                     lFreeElem(&old_sub_elem);
                     remove_reduced = true;
                     continue;
                  }
               }
            }
            if (remove_reduced) {
               lListElem *new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
               lFreeElem(&new_sub_elem);
               restart_loop = 1;
            }
            if (!ret) {
               break;
            }
            if (restart_loop) {
               next_reduced_element = lFirstRW(reduced_sublist);
            }
         }

         /* now we process elements which have not been found in the first step above */
         if (ret && ((sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE
             || (sub_cmd & ocs::gdi::SubCommand::APPEND) == ocs::gdi::SubCommand::APPEND
             || (sub_cmd & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE)) {
            next_reduced_element = lFirstRW(reduced_sublist);

            while ((reduced_element = next_reduced_element)) {
               int pos, type;
               const char *rstring = nullptr;
               lListElem *new_sub_elem;

               next_reduced_element = lNextRW(reduced_element);

               pos = lGetPosViaElem(reduced_element, this_elem_primary_key, SGE_NO_ABORT);
               type = lGetPosType(lGetElemDescr(reduced_element), pos);
               if (type == lStringT) {
                  rstring = lGetString(reduced_element, this_elem_primary_key);
               } else if (type == lHostT) {
                  rstring = lGetHost(reduced_element, this_elem_primary_key);
               }

               if (rstring == nullptr) {
                  ERROR(SFNMAX, MSG_OBJECT_VALUEMISSING);
                  answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC,
                                  ANSWER_QUALITY_ERROR);
                  ret = false;
               }

               if (ret) {
                  if (!no_info && (sub_cmd & ocs::gdi::SubCommand::REMOVE) == ocs::gdi::SubCommand::REMOVE) {
                     INFO(SFQ " does not exist in " SFQ " of " SFQ, rstring, sub_list_name, object_name);
                     answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                  } else {
                     if (!full_sublist) {
                        if (!no_info && (sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE) {
                           INFO(SFQ " of " SFQ " is empty - Adding new element(s).", sub_list_name, object_name);
                           answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                        }
                        lSetList(this_elem, this_elem_name, lCopyList("", lGetList(delta_elem, this_elem_name)));
                        full_sublist = lGetListRW(this_elem, this_elem_name);

                        did_changes = true;
                        break;
                     } else {
                        if (!no_info && (sub_cmd & ocs::gdi::SubCommand::CHANGE) == ocs::gdi::SubCommand::CHANGE) {
                           INFO("Unable to find " SFQ " in " SFQ " of " SFQ " - Adding new element.", rstring, sub_list_name, object_name);
                           answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
                        }
                        new_sub_elem = lDechainElem(reduced_sublist, reduced_element);
                        lAppendElem(full_sublist, new_sub_elem);
                        did_changes = true;
                     }
                  }
               }
            }
         }
      } else {
         /* Overwrite the complete list */
         lSetList(this_elem, this_elem_name, lCopyList("", lGetList(delta_elem, this_elem_name)));
         did_changes = true;
      }

      /* If the list does not contain any elements, we will delete the list itself */
      if (ret) {
         const lList *tmp_list = lGetList(this_elem, this_elem_name);

         if (tmp_list != nullptr && lGetNumberOfElem(tmp_list) == 0) {
            lSetList(this_elem, this_elem_name, nullptr);
         }
      }
   } else {
      ret = false;
   }

   if (changed != nullptr) {
      *changed = did_changes;
   }

   DRETURN(ret);
}

/**
 * @brief Apply a binding change to an advance reservation
 *
 * @param alpp receives messages for the caller
 * @param new_ar the advance reservation as it is to become (AR_Type)
 * @param ar the reduced element the client sent (AR_Type)
 *
 * @return true on success
 */
bool
attr_mod_obj_binding(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<ocs::tom::AR> new_ar,
                     const ocs::tom::ConstElement<ocs::tom::AR> ar) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (ocs::tom::has<AR_binding>(ar)) {
      const auto old_binding_elem = ocs::tom::get<AR_binding>(new_ar);
      const auto new_binding_elem = ocs::tom::get<AR_binding>(ar);
      if (old_binding_elem == nullptr) {
         // clone() keeps an empty handle empty, as the former nullptr check did
         ocs::tom::set<AR_binding>(new_ar, new_binding_elem.clone());
      } else {
         if (new_binding_elem != nullptr) {
            // copy new amount but only if it is not UNINITIALIZED and different from the old one
            uint32_t new_amount = ocs::tom::get<BN_amount>(new_binding_elem);
            if (new_amount != static_cast<uint32_t>(-1)) {
               uint32_t old_amount = ocs::AdvanceReservation::binding_get_amount(new_ar);
               if (old_amount != new_amount) {
                  ocs::tom::set<BN_amount>(old_binding_elem, new_amount);
               }
            }

            // copy new instance but only if it is not UNINITIALIZED and different from the old one
            auto new_instance = static_cast<ocs::BindingInstance::Instance>(ocs::tom::get<BN_instance>(new_binding_elem));
            if (new_instance != ocs::BindingInstance::Instance::UNINITIALIZED) {
               ocs::BindingInstance::Instance old_instance = ocs::AdvanceReservation::binding_get_instance(new_ar);
               if (old_instance != new_instance) {
                  ocs::tom::set<BN_instance>(old_binding_elem, new_instance);
               }
            }

            // copy new unit but only if it is not UNINITIALIZED and different from the old one
            auto new_unit = static_cast<ocs::BindingUnit::Unit>(ocs::tom::get<BN_unit>(new_binding_elem));
            if (new_unit != ocs::BindingUnit::Unit::UNINITIALIZED) {
               ocs::BindingUnit::Unit old_unit = ocs::AdvanceReservation::binding_get_unit(new_ar);
               if (old_unit != new_unit) {
                  ocs::tom::set<BN_unit>(old_binding_elem, new_unit);
               }
            }

            // copy new type but only if it is not UNINITIALIZED and different from the old one
            auto new_type = static_cast<ocs::BindingType::Type>(ocs::tom::get<BN_new_type>(new_binding_elem));
            if (new_type != ocs::BindingType::Type::UNINITIALIZED) {
               ocs::BindingType::Type old_type = ocs::AdvanceReservation::binding_get_type(new_ar);
               if (old_type != new_type) {
                  ocs::tom::set<BN_new_type>(old_binding_elem, new_type);
               }
            }

            // copy new filter but only if it is not UNINITIALIZED and different from the old one
            const char *new_filter = ocs::tom::get<BN_filter>(new_binding_elem);
            if (new_filter != nullptr) {
               std::string old_filter = ocs::AdvanceReservation::binding_get_filter(new_ar);
               if (strcmp(old_filter.c_str(), new_filter) != 0) {
                  ocs::tom::set<BN_filter>(old_binding_elem, new_filter);
               }
            }

            // copy new sort but only if it is not UNINITIALIZED and different from the old one
            const char *new_sort = ocs::tom::get<BN_sort>(new_binding_elem);
            if (new_sort != nullptr) {
               std::string old_sort = ocs::AdvanceReservation::binding_get_sort(new_ar);
               if (old_sort != new_sort) {
                  ocs::tom::set<BN_sort>(old_binding_elem, new_sort);
               }
            }

            // copy new start but only if it is not UNINITIALIZED and different from the old one
            auto new_start = static_cast<ocs::BindingStart::Start>(ocs::tom::get<BN_start>(new_binding_elem));
            if (new_start != ocs::BindingStart::Start::UNINITIALIZED) {
               ocs::BindingStart::Start old_start = ocs::AdvanceReservation::binding_get_start(new_ar);
               if (old_start != new_start) {
                  ocs::tom::set<BN_start>(old_binding_elem, new_start);
               }
            }

            // copy new end but only if it is not UNINITIALIZED and different from the old one
            auto new_stop = static_cast<ocs::BindingStop::Stop>(ocs::tom::get<BN_stop>(new_binding_elem));
            if (new_stop != ocs::BindingStop::Stop::UNINITIALIZED) {
               ocs::BindingStop::Stop old_stop = ocs::AdvanceReservation::binding_get_stop(new_ar);
               if (old_stop != new_stop) {
                  ocs::tom::set<BN_stop>(old_binding_elem, new_stop);
               }
            }

            // copy new strategy but only if it is not UNINITIALIZED and different from the old one
            auto new_strategy = static_cast<ocs::BindingStrategy::Strategy>(ocs::tom::get<BN_strategy>(new_binding_elem));
            if (new_strategy != ocs::BindingStrategy::Strategy::UNINITIALIZED) {
               ocs::BindingStrategy::Strategy old_strategy = ocs::AdvanceReservation::binding_get_strategy(new_ar);
               if (old_strategy != new_strategy) {
                  ocs::tom::set<BN_strategy>(old_binding_elem, new_strategy);
               }
            }
         }

         // if the new binding has no amount, we remove binding completely
         if (ocs::AdvanceReservation::binding_get_amount(new_ar) == 0) {
            ocs::tom::set<AR_binding>(new_ar, nullptr);
         }
      }

      // make sure that all binding fields have valid values
      // or apply implicit binding if enabled
      ocs::AdvanceReservation::binding_set_missing_defaults(new_ar, alpp);
   }
   DRETURN(ret);
}



/**
 * @brief Modify cqueues configuration sublist
 *
 * This function modifies a certain configuration sublist of "this_elem".
 * Possible errors will be reported in "answer_list". "reduced_elem"
 * will be used to identify the changes which have been done.
 * "sub_command" defines how these changes should be applied to
 * "this_elem". "sublist_value_name" is the sublist of "this_elem" which
 * should be modified whereas "subsub_key" defines the attribute
 * which containes the primary key of that sublist. "attribute_name_str"
 * is the name of the cqueue attribute which will be modified with
 * this operation. It will be used for error output. "object_name_str"
 * is the name of the cqueue which will be modified by this operation.
 *
 * @param this_elem CQ_Type
 * @param answer_list AN_Type
 * @param reduced_elem reduced CQ_Type element
 * @param sub_cmd GDI subcommand
 * @param attribute_name CULL attribute name (CQ_Type)
 * @param sublist_host_name CULL sublist attribute name depend on sublist of CQ_Type
 * @param sublist_value_name CULL sublist attribute name of that field which containes the value of the attribute to be modified.
 * @param subsub_key CULL sublist attribute key
 * @param attribute_name_str string used for user output
 * @param object_name_str cqueue name
 * @param cmd the command being executed
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: cqueue_mod_sublist() is MT safe
 */
bool
cqueue_mod_sublist(lListElem *this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lListElem *reduced_elem,
                   ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_cmd,
                   int attribute_name, int sublist_host_name, int sublist_value_name, int subsub_key,
                   const char *attribute_name_str, const char *object_name_str) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;
   int pos;

   pos = lGetPosViaElem(reduced_elem, attribute_name, SGE_NO_ABORT);
   if (pos >= 0) {
      lList *mod_list = lGetPosList(reduced_elem, pos);
      lList *org_list = lGetListRW(this_elem, attribute_name);

      /* 
       * Delete all configuration lists except the default-configuration
       * if sub_command is SGE_GDI_SET_ALL
       */
      if ((sub_cmd & ocs::gdi::SubCommand::SET_ALL) == ocs::gdi::SubCommand::SET_ALL) {
         lListElem *elem, *next_elem;

         next_elem = lFirstRW(org_list);
         while ((elem = next_elem)) {
            const char *name = lGetHost(elem, sublist_host_name);

            next_elem = lNextRW(elem);
            lListElem *mod_elem = lGetElemHostRW(mod_list, sublist_host_name, name);
            if (mod_elem == nullptr) {
               DPRINTF("Removing attribute list for " SFQ "\n", name);
               lRemoveElem(org_list, &elem);
            }
         }
      }

      /*
       * Do modifications for all given elements of 
       * domain/host-configuration list
       */
      for_each_rw_lv(mod_elem, mod_list) {
         const char *name = lGetHost(mod_elem, sublist_host_name);
         char resolved_name[CL_MAXHOSTNAMELEN + 1];
         lListElem *org_elem = nullptr;

         if (name == nullptr) {
            ERROR(MSG_SGETEXT_INVALIDHOST_S, "");
            answer_list_add(answer_list, SGE_EVENT,
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
            break;
         }
         /* Don't try to resolve hostgroups */
         if (name[0] != '@') {
            int back = getuniquehostname(name, resolved_name, 0);

            if (back == CL_RETVAL_OK) {
               /* 
                * This assignment is ok because preious name contained a const
                * string from the mod_elem that we didn't need to free.  
                * Now it will contain a string that's on the stack, 
                * so we still don't have to free it. 
                */
               name = resolved_name;
            } else {
               /*
                * Due to CR 6319231, IZ 1760 this is allowed
                */
            }
         }

         org_elem = lGetElemHostRW(org_list, sublist_host_name, name);

         /*
          * Create element if it does not exist
          */
         if (org_elem == nullptr && (sub_cmd & ocs::gdi::SubCommand::REMOVE) != ocs::gdi::SubCommand::REMOVE) {
            if (org_list == nullptr) {
               org_list = lCreateList("", lGetElemDescr(mod_elem));
               lSetList(this_elem, attribute_name, org_list);
            }
            // the element gets the layout of the list it goes into, which may exist already (CS-3112)
            org_elem = lCreateElem(lGetListDescr(org_list));
            lSetHost(org_elem, sublist_host_name, name);
            lAppendElem(org_list, org_elem);
         }

         /*
          * Modify sublist according to subcommand
          */
         if (org_elem != nullptr) {
            if (subsub_key != NoName) {
               attr_mod_sub_list(answer_list, org_elem, sublist_value_name, subsub_key, mod_elem, cmd, sub_cmd,
                                 attribute_name_str, object_name_str, 0, nullptr);
            } else {
               object_replace_any_type(org_elem, sublist_value_name, mod_elem);
            }
         }
      }
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
      const char *s = ocs::tom::get<F>(ep1);
      if (ocs::tom::find_first<F>(lp2, s)) {
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
