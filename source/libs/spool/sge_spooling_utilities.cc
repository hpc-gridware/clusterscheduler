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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Deciding which attributes of an object get spooled, and how
 */

#define NO_SGE_COMPILE_DEBUG

#include <cstring>

#include "uti/sge.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_utility.h"

#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/ocs_DataStore.h"

#include "sched/debit.h"

#include "spool/sge_spooling.h"
#include "spool/sge_spooling_utilities.h"
#include "spool/msg_spoollib.h"

#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_advance_reservation.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Model.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_SPR.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "msg_common.h"

const spool_instr_t spool_config_subinstr = {
        MT_SPOOL_SUBLIST,
        false,
        false,
        nullptr
};

const spool_instr_t spool_config_instr = {
        MT_SPOOL,
        true,
        true,
        &spool_config_subinstr
};

const spool_instr_t spool_complex_subinstr = {
        MT_SPOOL,
        false,
        false,
        nullptr
};

const spool_instr_t spool_complex_instr = {
        MT_SPOOL,
        false,
        false,
        &spool_complex_subinstr
};

const spool_instr_t spool_userprj_subinstr = {
        MT_SPOOL_SUBLIST,
        false,
        false,
        &spool_userprj_subinstr
};

const spool_instr_t spool_user_instr = {
        MT_SPOOL,
        true,
        true,
        &spool_userprj_subinstr
};

static spooling_field *
_spool_get_fields_to_spool(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lDescr *descr,
                           const spool_instr_t *instr);

/**
 * @brief Which fields are to be spooled
 *
 * Returns an array of field descriptions (sge_spooling_field).
 * Which fields have to be spooled is retrieved from the object
 * definition given in `descr` and the spooling instruction `instr`.
 * For each attribute the function checks if the attribute information
 * fulfils the selection given in `instr` (e.g. attribute property
 * MT_SPOOL).
 * If `instr` contains an instruction for sublists, the function will
 * take the object definition of the sublists from the spooling record
 * of the list field (the sub-type `ocs::tom::sub_type()` names) and call itself
 * recursively. Every entry carries the spooling record of its field in `ops`.
 *
 * @param answer_list answer list to report errors
 * @param descr object type to analyze
 * @param instr spooing instructions to use
 *
 * @return an array of type spooling_field, or nullptr, if an error occurred, error messages are returned in answer_list
 *
 * @note The returned spooling_field array has to be freed by the caller of this
 *       function using the function spool_free_spooling_fields().
 *
 * @see `ocs::tom::sub_type()`, #spool_free_spooling_fields
 */
spooling_field *
spool_get_fields_to_spool(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lDescr *descr,
                          const spool_instr_t *instr) {
   DENTER(TOP_LAYER);

   spooling_field *fields;

   SGE_CHECK_POINTER_NULL(descr, answer_list);

   fields = _spool_get_fields_to_spool(answer_list, descr, instr);

   DRETURN(fields);
}

/**
 * @brief Build the spooling field array of an object type
 *
 * Takes the fields that have a value in the descriptor and match the
 * selection of the instructions, in descriptor order, and recurses into
 * sub-lists with the sub-instructions. The object type is looked up once per
 * descriptor; every entry gets the spooling record of its field from it, made
 * as the record the typed field tables carry, so the walkers treat it like a
 * row of a typed table.
 *
 * @param answer_list receives error messages
 * @param descr the object type, checked by the caller
 * @param instr the spooling instructions, checked by the caller
 * @return the field array terminated by #NoName, or nullptr on error
 */
static spooling_field *
_spool_get_fields_to_spool(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lDescr *descr,
                           const spool_instr_t *instr) {
   DENTER(TOP_LAYER);

   spooling_field *fields;
   int i, j, size;
   size_t strip = 0;

   /* we don't check descr and instr, as we know they are ok
    * (it's a static function)
    */

   /* the spooling records of the type's fields; every descriptor is derived from the
    * model, so the lookup fails only for a descriptor made up by hand */
   const ocs_spool_type_ops *type_ops = ocs_spool_type_ops_of(descr);
   if (type_ops == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_UNKNOWNOBJECTTYPEFOR_SS,
                              ocs::tom::field_name(descr[0].nm), __func__);
      DRETURN(nullptr);
   }

   /* count fields to spool */
   size = 0;
   for (const auto f : ocs::tom::present_fields(descr)) {
      if ((descr[f.pos].mt & instr->selection) != 0) {
         size++;
      }
   }

   /* allocate memory */
   fields = (spooling_field *) sge_malloc((size + 1) * sizeof(spooling_field));
   if (fields == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_UNABLETOALLOCATEBYTES_DS,
                              (size * 1) * sizeof(spooling_field), __func__);
      DRETURN(nullptr);
   }

   /* initialize fields */
   for (i = 0; i < size; i++) {
      fields[i].nm = NoName;
      fields[i].width = 0;
      fields[i].name = nullptr;
      fields[i].free_name = false;
      fields[i].sub_fields = nullptr;
      fields[i].free_sub_fields = false;
      fields[i].clientdata = nullptr;
      fields[i].read_func = nullptr;
      fields[i].write_func = nullptr;
      fields[i].ops = nullptr;
   }

   /* do we have to strip field prefixes, e.g. "QU_" from field names? */
   if (instr->copy_field_names && instr->strip_field_prefix) {
      dstring buffer = DSTRING_INIT;
      const char *prefix = object_get_name_prefix(descr, &buffer);
      strip = sge_strlen(prefix);
      sge_dstring_free(&buffer);
   }

   /* copy field info */
   j = 0;
   for (const auto f : ocs::tom::present_fields(descr)) {
      if ((descr[f.pos].mt & instr->selection) != 0) {
         spooling_field *sub_fields = nullptr;

         DPRINTF("field " SFQ " will be spooled\n", ocs::tom::field_name(descr[f.pos].nm));

         fields[j].nm = descr[f.pos].nm;
         fields[j].ops = &type_ops->ops[descr[f.pos].nm - type_ops->base];

         if (instr->copy_field_names) {
            const char *name;
            name = ocs::tom::field_name(descr[f.pos].nm);
            if (name == nullptr || strlen(name) <= strip) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_NONAMEFORATTRIBUTE_D,
                                       descr[f.pos].nm);
               fields = spool_free_spooling_fields(fields);
               DRETURN(nullptr);
            }
            fields[j].name = strdup(name + strip);
            fields[j].free_name = true;
         }

         if (fields[j].ops->type == ocs::tom::FieldType::List) {
            const lDescr *sub_descr;

            if (instr->sub_instr == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_DONTKNOWHOWTOSPOOLSUBLIST_SS,
                                       ocs::tom::field_name(descr[f.pos].nm), __func__);
               fields = spool_free_spooling_fields(fields);
               DRETURN(nullptr);
            }

            // the model's sub-type, the one ocs::tom::sub_type() names (test_tom_traits checks that)
            sub_descr = fields[j].ops->sub_descr;
            if (sub_descr == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_ERROR,
                                       MSG_UNKNOWNOBJECTTYPEFOR_SS,
                                       ocs::tom::field_name(descr[f.pos].nm), __func__);
               fields = spool_free_spooling_fields(fields);
               DRETURN(nullptr);
            }

            /* recursive spooling, e.g. sharetree */
            if (instr->sub_instr == instr && descr == sub_descr) {
               sub_fields = fields;
               DPRINTF("recursive structure detected for field %s\n", ocs::tom::field_name(descr[f.pos].nm));
            } else {
               sub_fields = _spool_get_fields_to_spool(answer_list, sub_descr,
                                                       instr->sub_instr);
            }
         }

         fields[j].free_sub_fields = true;
         fields[j++].sub_fields = sub_fields;
      }
   }

   /* end of field array */
   fields[j].nm = NoName;

   DRETURN(fields);
}

/**
 * @brief Free a spooling field array
 *
 * Frees an array of spooling_field with all sublists and contained strings.
 *
 * @code
 * fields = spool_free_spooling_fields(fields);
 * @endcode
 *
 * @param fields the field array to free
 *
 * @return nullptr
 */
spooling_field *
spool_free_spooling_fields(spooling_field *fields) {
   if (fields != nullptr) {
      int i;
      for (i = 0; fields[i].nm >= 0; i++) {
         if (fields[i].sub_fields != nullptr && fields[i].sub_fields != fields && fields[i].free_sub_fields) {
            fields[i].sub_fields = spool_free_spooling_fields(fields[i].sub_fields);
         }

         if (fields[i].name != nullptr && fields[i].free_name) {
            sge_free(&(fields[i].name));
         }
      }
      sge_free(&fields);
   }

   return nullptr;
}

/**
 * @brief Validate objects
 *
 * Verifies an object.
 *
 * The object comes from the reader of the backend, which created it with the
 * descriptor of @p object_type, so each case takes it with a checked handle of
 * that type. The object stays a raw handle: #spooling_validate_func serves
 * every object type.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param object object to validate
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 */
bool spool_default_validate_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 const ocs::tom::ConstElement<ocs::tom::SPT> type,
                                 const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                 const ocs::tom::RawElement object,
                                 const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   switch (object_type) {
      case SGE_TYPE_EXECHOST: {
         const auto ehost = ocs::tom::Element<ocs::tom::EH>(object);
         int cl_ret;
         char *old_name = strdup(ehost.get<EH_name>().c_str());

         /* try hostname resolving */
         if (strcmp(old_name, SGE_GLOBAL_NAME) != 0) {
            cl_ret = ocs_resolve_host<EH_name>(ehost);

            /* if hostname resolving failed: create error */
            if (cl_ret != CL_RETVAL_OK) {
               if (cl_ret != CL_RETVAL_GETHOSTNAME_ERROR) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_SPOOL_CANTRESOLVEHOSTNAME_SS, old_name, cl_get_error_text(ret));
                  ret = false;
               } else {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING,
                                          MSG_SPOOL_CANTRESOLVEHOSTNAME_SS, old_name, cl_get_error_text(ret));
               }
            } else {
               /* if hostname resolving changed hostname: spool */
               const char *new_name = ehost.get<EH_name>().c_str();
               if (strcmp(old_name, new_name) != 0) {
                  auto write_func = (spooling_write_func)rule.get<SPR_write_func>();
                  auto delete_func = (spooling_delete_func)rule.get<SPR_delete_func>();
                  write_func(answer_list, type, rule, ehost, new_name, object_type);
                  delete_func(answer_list, type, rule, old_name, object_type);
               }
            }
         }

         sge_free(&old_name);

         if (ret) {
            /* all spooled load values are static, therefore we tag them here */
            for (const ocs::tom::Element<ocs::tom::HL> load_value : ehost.get<EH_load_list>()) {
               load_value.set<HL_is_static>(true);
            }

            /* necessary to init double values of consumable configuration */
            const auto consumable_config_list = ehost.get<EH_consumable_config_list>();
            centry_list_fill_request(consumable_config_list, nullptr, master_centry_list, true, false, true);
            /* an RSMAP that was spooled as a bare amount gets the ids 0 to amount-1 here, which
               repairs a host that was configured before the ids were created implicitly. Has to
               happen before debit_host_consumable() below, which sets up the per id booking. */
            if (!centry_list_rsmap_expand_implicit_ids(answer_list, consumable_config_list)) {
               ret = false;
            }
            /* necessary to setup actual list of exechost */
            debit_host_consumable(ocs::tom::ConstElement<ocs::tom::JB>(), ocs::tom::ConstElement<ocs::tom::JAT>(),
                                  ocs::tom::ConstList<ocs::tom::GRU>(), ocs::tom::ConstElement<ocs::tom::PE>(), ehost,
                                  master_centry_list, 0, true, true, nullptr);

            if (ensure_attrib_available(nullptr, ehost.get<EH_consumable_config_list>(), master_centry_list)) {
               ret = false;
            }
         }
      }
         break;
      case SGE_TYPE_QINSTANCE:
         ret = qinstance_validate(ocs::tom::Element<ocs::tom::QU>(object), answer_list, master_ehost_list,
                                  master_centry_list);
         break;
      case SGE_TYPE_CQUEUE:
         ret = qinstance_list_validate(ocs::tom::Element<ocs::tom::CQ>(object).get<CQ_qinstances>(), answer_list,
                                       master_ehost_list, master_centry_list);
         break;
      case SGE_TYPE_CONFIG: {
         const auto config = ocs::tom::Element<ocs::tom::CONF>(object);
         int cl_ret;
         char *old_name = strdup(config.get<CONF_name>());

         /* try hostname resolving */
         if (strcmp(old_name, SGE_GLOBAL_NAME) != 0) {
            cl_ret = ocs_resolve_host<CONF_name>(config);
            /* if hostname resolving failed: create error */
            if (cl_ret != CL_RETVAL_OK) {
               if (cl_ret != CL_RETVAL_GETHOSTNAME_ERROR) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                          ANSWER_QUALITY_ERROR,
                                          MSG_SPOOL_CANTRESOLVEHOSTNAME_SS,
                                          old_name, cl_get_error_text(ret));
                  ret = false;
               } else {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                          ANSWER_QUALITY_WARNING,
                                          MSG_SPOOL_CANTRESOLVEHOSTNAME_SS,
                                          old_name, cl_get_error_text(ret));
               }
            } else {
               /* if hostname resolving changed hostname: spool */
               const char *new_name = config.get<CONF_name>();
               if (strcmp(old_name, new_name) != 0) {
                  auto write_func = (spooling_write_func)rule.get<SPR_write_func>();
                  auto delete_func = (spooling_delete_func)rule.get<SPR_delete_func>();
                  write_func(answer_list, type, rule, config, new_name, object_type);
                  delete_func(answer_list, type, rule, old_name, object_type);
               }
            }
         }
         sge_free(&old_name);
      }
         break;
      case SGE_TYPE_USERSET:
         if (userset_validate_entries(ocs::tom::ConstElement<ocs::tom::US>(object), answer_list) != STATUS_OK) {
            ret = false;
         }
         break;
      case SGE_TYPE_CKPT:
         if (ckpt_validate(ocs::tom::ConstElement<ocs::tom::CK>(object), answer_list) != STATUS_OK) {
            ret = false;
         }
         break;
      case SGE_TYPE_PE:
         if (pe_validate(ocs::tom::Element<ocs::tom::PE>(object), answer_list, 1, master_userset_list) != STATUS_OK) {
            ret = false;
         }
         break;
      case SGE_TYPE_CENTRY:
         // a complex already in the spool is reported, not refused - see centry_elem_validate()
         if (!centry_elem_validate(ocs::tom::ConstElement<ocs::tom::CE>(object), master_centry_list, answer_list,
                                   true)) {
            ret = false;
         }
         break;
      case SGE_TYPE_RQS:
         if (!rqs_verify_attributes(ocs::tom::Element<ocs::tom::RQS>(object), answer_list, true, master_centry_list)) {
            ret = false;
         }
         break;
      case SGE_TYPE_AR:
         if (!ar_validate(ocs::tom::Element<ocs::tom::AR>(object), answer_list, true, true, master_cqueue_list,
                          master_hgroup_list, master_centry_list, master_ckpt_list, master_pe_list,
                          master_userset_list)) {
            ret = false;
         }
         break;
      case SGE_TYPE_USER:
         ocs_null_out_none<UU_default_project>(ocs::tom::Element<ocs::tom::UU>(object));
         break;
      case SGE_TYPE_HGROUP:
      case SGE_TYPE_CALENDAR:
      case SGE_TYPE_PROJECT:
      case SGE_TYPE_SHARETREE:
      case SGE_TYPE_SCHEDD_CONF:
         break;
      case SGE_TYPE_JOB: {
         const auto job = ocs::tom::Element<ocs::tom::JB>(object);
         // fill in non spooled fields, see also code for SGE_TYPE_EXECHOST
         // The amount of every request is (re)computed here, into CE_doubleval, which is not
         // spooled. The scheduler reads it directly and no longer re-parses the string, so a
         // failure here would otherwise leave the amount at 0 and the job would be dispatched
         // while booking nothing. Report it rather than discarding the answer list.
         for (const ocs::tom::Element<ocs::tom::JRS> jrs : job.get<JB_request_set_list>()) {
            ocs::tom::ListPtr<ocs::tom::AN> fill_answer_list;
            // both calls must run; do not let the first failure skip the soft list
            int hard_ret = centry_list_fill_request(jrs.get<JRS_hard_resource_list>(), &fill_answer_list,
                                                    master_centry_list, true, false, true);
            int soft_ret = centry_list_fill_request(jrs.get<JRS_soft_resource_list>(), &fill_answer_list,
                                                    master_centry_list, true, false, true);
            if (hard_ret != 0 || soft_ret != 0) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_SPOOL_JOBREQUESTNOTPARSABLE_U, job.get<JB_job_number>());
               answer_list_append_list(answer_list, &fill_answer_list);
            }
            fill_answer_list.reset();
         }
      }
         break;
      default:
         break;
   }
#pragma GCC diagnostic pop

   DRETURN(ret);
}


/**
 * @brief Finish a freshly read master list
 *
 * The default #spooling_validate_list_func, called by a backend once it has
 * read every object of one type.
 *
 * @param answer_list to return error messages
 * @param type        the spooling type element, unused here
 * @param rule        the spooling rule, unused here
 * @param object_type which master list was just read
 *
 * @return true if the list is usable, else false
 *
 * @note Despite the name this is not primarily a validation. It is the work
 *       that can only be done once the whole list is present: merging the
 *       host list, sorting the complex entries, resolving the host group
 *       references. Only the scheduler configuration is really validated.
 */
bool
spool_default_validate_list_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 [[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::SPT> type,
                                 [[maybe_unused]] const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                 const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   switch (object_type) {
      case SGE_TYPE_EXECHOST:
         host_list_merge(ocs::DataStore::master_list_rw<SGE_TYPE_EXECHOST>());
         break;
      case SGE_TYPE_CONFIG:
      case SGE_TYPE_USERSET:
      case SGE_TYPE_CKPT:
      case SGE_TYPE_PE:
         break;
      case SGE_TYPE_CENTRY:
         centry_list_sort(ocs::DataStore::master_list_rw<SGE_TYPE_CENTRY>());
         break;
      case SGE_TYPE_HGROUP:
      case SGE_TYPE_CALENDAR:
      case SGE_TYPE_PROJECT:
      case SGE_TYPE_USER:
      case SGE_TYPE_SHARETREE:
         break;
      case SGE_TYPE_SCHEDD_CONF:
         ret = sconf_validate_config_(answer_list);
         break;
      case SGE_TYPE_JOB:
      case SGE_TYPE_AR:
      default:
         break;
   }

   DRETURN(ret);
}

/**
 * @brief A copy of an exec host to spool, without its dynamic load values
 *
 * Only static load values shall be persisted (CS-1597); dynamic load values
 * are volatile and will be reported again by the execution daemon once it
 * (re)connects. The spooling backends write this copy instead of the object,
 * which stays as it is: before, they moved the dynamic values out of the
 * object and back in after the write, which changed the order of its
 * EH_load_list and wrote through a const object.
 *
 * @param object the exec host object
 * @return the copy, owned by the caller
 */
ocs::tom::ElementPtr<ocs::tom::EH>
ocs_spool_exechost_static_copy(const ocs::tom::ConstElement<ocs::tom::EH> object) {
   DENTER(TOP_LAYER);

   auto copy = object.clone();
   if (const auto load_list = copy->get<EH_load_list>(); load_list) {
      auto next_load_value = load_list.front();
      while (const auto load_value = next_load_value) {
         next_load_value = load_value.next();
         if (!load_value.get<HL_is_static>()) {
            load_list.erase(load_value);
         }
      }
   }

   DRETURN(copy);
}


