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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Assembling the complex attributes the scheduler matches against
 *
 * An attribute can be defined at three levels - global, execution host, queue
 * instance - and a job's `-l` request has to be matched against the value
 * that actually applies. Which level wins is what the **dominance** of an
 * attribute records, and monitor_dominance() renders it as the `g`/`h`/`q`
 * prefix seen in `qstat -F` output.
 *
 * get_attribute() answers for one level, get_attribute_by_name() resolves
 * across all three, and host_complexes2scheduler() and
 * queue_complexes2scheduler() build the complete attribute list of one object
 * for the scheduler to match a request against.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fnmatch.h>

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_load.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/ocs_CEntry.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "sge_complex_schedd.h"
#include "sge_resource_utilization.h"
#include "msg_schedd.h"
#include "sgeobj/sge_answer.h"

static int resource_cmp(uint32_t relop, double req_all_slots, double src_dl);

static int string_cmp(ocs::CEntry::Type type, uint32_t relop, const char *request,
                      const char *offer);

static ocs::tom::ListPtr<ocs::tom::CE>
get_attribute_list_by_names(ocs::tom::ConstElement<ocs::tom::EH> global, ocs::tom::ConstElement<ocs::tom::EH> host,
                            ocs::tom::ConstElement<ocs::tom::QU> queue, ocs::tom::ConstList<ocs::tom::CE> centry_list,
                            ocs::tom::ConstList<ocs::tom::CE> load_adjustments,
                            ocs::tom::ConstList<ocs::tom::ST> attrnames);

template <auto F>
static void build_name_filter(ocs::tom::List<ocs::tom::ST> filter,
                              ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> list);

static bool is_attr_prior(ocs::tom::Element<ocs::tom::CE> upper_el, ocs::tom::Element<ocs::tom::CE> lower_el);

template <auto V, auto D>
static bool is_attr_prior2(ocs::tom::Element<ocs::tom::CE> upper_el, double lower_value);

static bool get_queue_resource(ocs::tom::Element<ocs::tom::CE> queue_elem, ocs::tom::ConstElement<ocs::tom::QU> queue,
                               const char *attrname);

static ocs::tom::ListPtr<ocs::tom::CE>
get_attribute_list(ocs::tom::ConstElement<ocs::tom::EH> global, ocs::tom::ConstElement<ocs::tom::EH> host,
                   ocs::tom::ConstElement<ocs::tom::QU> queue, ocs::tom::ConstList<ocs::tom::CE> centry_list,
                   ocs::tom::ConstList<ocs::tom::CE> load_adjustments);

/**
 * @brief A copy of the configuration of an attribute
 *
 * @param centry_list the system wide attribute configuration
 * @param attrname    the attribute
 *
 * @return the copy, or an empty handle if the attribute is not configured
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
copy_centry(const ocs::tom::ConstList<ocs::tom::CE> centry_list, const char *attrname) {
   const auto centry = ocs::tom::find_first<CE_name>(centry_list, attrname);
   return centry != nullptr ? centry.clone() : ocs::tom::ElementPtr<ocs::tom::CE>();
}

/**
 * @def LOAD_ADJUSTMENTS_LATE_FETCH
 * @brief Fetch the load adjustments in get_attribute() when the caller passed none
 *
 * The old behaviour, kept for safety: a caller that passes nullptr for
 * `load_adjustments` gets them fetched from the scheduler configuration
 * instead of having them ignored. Defined inside the body of
 * get_attribute(), which is why it is documented here.
 */

/**
 * @def STR_LEN_AVAIL_TEXT
 * @brief Size of the buffers holding the diagnosis of a failed match
 *
 * Defined inside the body of compare_complexes(), which is why it is
 * documented here.
 */


/**
 * @brief Renders the dominance of an attribute as the string `qstat -F` shows
 *
 * The first character is the layer the value comes from - `g` global, `h`
 * host, `q` queue - and the second where it comes from, e.g. a fixed
 * configuration value or a load value.
 *
 * @param[out] str  buffer receiving the rendering, at least 5 bytes
 * @param[in]  mask the `CE_dominant` value to render
 */
void monitor_dominance(char *str, uint32_t mask) {
   switch (mask & DOMINANT_LAYER_MASK) {
   case DOMINANT_LAYER_GLOBAL:   
      *str++ = 'g';
      break;
   case DOMINANT_LAYER_HOST:   
      *str++ = 'h';
      break;
   case DOMINANT_LAYER_QUEUE:   
      *str++ = 'q';
      break;
   default:
      *str++ = '?';
      break;
   }

   switch (mask & DOMINANT_TYPE_MASK) {
   case DOMINANT_TYPE_VALUE:   
      *str++ = 'v';
      break;
   case DOMINANT_TYPE_FIXED:   
      *str++ = 'f';
      break;
   case DOMINANT_TYPE_LOAD:   
      *str++ = 'l';
      break;
   case DOMINANT_TYPE_CLOAD:   
      *str++ = 'L';
      break;
   case DOMINANT_TYPE_CONSUMABLE:   
      *str++ = 'c';
      break;
   default:
      *str++ = '?';
      break;
   }

   *str++ = '\0';
}

/**
 * @brief Looks for an attribute, but only for one level (for host, global, or queue)
 *
 * Extracts the attribute specified with 'attrname' and finds the
 * more important one, if it is defined multiple times on the same
 * level. It only cares about one level.
 * If the attribute is a consumable, one can specify a point in time and a duration.
 * This will get the caller the min amount of that resource during the time frame.
 *
 * @param attrname attribute name one is looking for
 * @param config_attr user defined attributes (CE), this is a host's / queue's complex_values list: EH_consumable_config_list, QU_consumable_config_list
 * @param actual_attr current usage of consumables (RUE)
 * @param load_attr load attributes
 * @param centry_list the system wide attribute configuration
 * @param queue the current queue, or nullptr, if one works on hosts
 * @param layer the current layer
 * @param lc_factor the load correction value
 * @param reason space for error messages or nullptr
 * @param load_adjustments the load adjustments to apply, or nullptr to have
 *                         them fetched from the scheduler configuration
 * @param zero_utilization ignore the current utilization and report the value
 *                         as if nothing were booked
 * @param start_time begin of the time interval, one asks for the resource
 * @param duration the duration the interval
 *
 * @return the element one was looking for, owned by the caller, or an empty handle
 */
ocs::tom::ElementPtr<ocs::tom::CE>
get_attribute(const char *attrname, const ocs::tom::ConstList<ocs::tom::CE> config_attr,
              const ocs::tom::ConstList<ocs::tom::RUE> actual_attr, const ocs::tom::ConstList<ocs::tom::HL> load_attr,
              const ocs::tom::ConstList<ocs::tom::CE> centry_list, ocs::tom::ConstList<ocs::tom::CE> load_adjustments,
              const ocs::tom::ConstElement<ocs::tom::QU> queue, uint32_t layer, double lc_factor, dstring *reason,
              bool zero_utilization, uint64_t start_time, uint64_t duration) {
   DENTER(BASIS_LAYER);

   ocs::tom::ElementPtr<ocs::tom::CE> cplx_el; // this is what we return

   /* resource_attr is a complex_entry (CE) */
   if (config_attr != nullptr) {
      const auto temp = ocs::tom::find_first<CE_name>(config_attr, attrname);
      if (temp != nullptr) {
         cplx_el = copy_centry(centry_list, attrname);
         if (cplx_el == nullptr) {
            /* error */
            DPRINTF("==> ERROR in get_attribute(): didn't find centry for %s\n", attrname);
            DRETURN(ocs::tom::ElementPtr<ocs::tom::CE>());
         }
         ocs::tom::set<CE_dominant>(cplx_el, layer | DOMINANT_TYPE_FIXED);
         ocs::tom::set<CE_pj_dominant>(cplx_el, DOMINANT_TYPE_VALUE);  /* default, no value set */ 
         ocs::tom::set<CE_doubleval>(cplx_el, ocs::tom::get<CE_doubleval>(temp));
         ocs::tom::set<CE_stringval>(cplx_el, ocs::tom::get<CE_stringval>(temp));
      }
   }

   if (cplx_el != nullptr && ocs::tom::get<CE_consumable>(cplx_el) != CONSUMABLE_NO) {
      ocs::tom::set<CE_pj_dominant>(cplx_el, layer | DOMINANT_TYPE_CONSUMABLE);
      ocs::tom::set<CE_dominant>(cplx_el, DOMINANT_TYPE_VALUE);
      /* treat also consumables as fixed attributes when assuming an empty queuing system */
      if (sconf_get_qs_state() == QS_STATE_FULL) {
         const auto actual_el = actual_attr != nullptr ? ocs::tom::find_first<RUE_name>(actual_attr, attrname)
                                                       : ocs::tom::ConstElement<ocs::tom::RUE>();
         if (actual_el != nullptr) {
            DSTRING_STATIC(ds, 20);
            ocs::TopologyString binding_inuse;
            double utilized = zero_utilization ? 0 : utilization_max(nullptr, ocs::tom::ConstElement<ocs::tom::EH>(),
                                                                     actual_el, start_time, duration, 0.0, 0.0, 0.0,
                                                                     false, binding_inuse);
            double pj_doubleval;

            switch (ocs::tom::get<CE_relop>(cplx_el)) {
               case CMPLXGE_OP:
               case CMPLXGT_OP:
                     pj_doubleval = utilized;
               break;

               case CMPLXEQ_OP:
               case CMPLXLT_OP:
               case CMPLXLE_OP:
               case CMPLXNE_OP:
               default:
                  pj_doubleval = ocs::tom::get<CE_doubleval>(cplx_el) - utilized;
                  break;
            }
            ocs::tom::set<CE_pj_doubleval>(cplx_el, pj_doubleval);
            ocs::tom::set<CE_pj_stringval>(cplx_el, sge_dstring_sprintf(&ds, "%8.3f", pj_doubleval));
         } else {
            sge_dstring_sprintf(reason, MSG_ATTRIB_ACTUALELEMENTTOATTRIBXMISSING_S, attrname);
            DRETURN(ocs::tom::ElementPtr<ocs::tom::CE>());
         }
      } else {
         ocs::tom::set<CE_pj_doubleval>(cplx_el, ocs::tom::get<CE_doubleval>(cplx_el)); 
         ocs::tom::set<CE_pj_stringval>(cplx_el, ocs::tom::get<CE_stringval>(cplx_el));
      }
   }

   /** check for a load value */
   const auto load_el = load_attr != nullptr ? ocs::tom::find_first<HL_name>(load_attr, attrname)
                                             : ocs::tom::ConstElement<ocs::tom::HL>();
   if (load_el != nullptr &&
       (sconf_get_qs_state()==QS_STATE_FULL || ocs::tom::get<HL_is_static>(load_el)) &&
       !is_attr_prior(cplx_el.get(), cplx_el.get())) {
         if (cplx_el == nullptr) {
            cplx_el = copy_centry(centry_list, attrname);
            if (cplx_el == nullptr) {
               /* error */
               DRETURN(ocs::tom::ElementPtr<ocs::tom::CE>());
            }
            ocs::tom::set<CE_dominant>(cplx_el, DOMINANT_TYPE_VALUE);
            ocs::tom::set<CE_pj_dominant>(cplx_el, DOMINANT_TYPE_VALUE);
         }

         {
            double dval;
            const char *load_value = ocs::tom::get<HL_value>(load_el);

            /* are we working on string values? if though, then it is easy */
            auto type = static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(cplx_el));
            if (type == ocs::CEntry::Type::STR || type == ocs::CEntry::Type::CSTR
                || type == ocs::CEntry::Type::HOST || type == ocs::CEntry::Type::RESTR) {
               ocs::tom::set<CE_stringval>(cplx_el, load_value);
               ocs::tom::set<CE_dominant>(cplx_el, layer | DOMINANT_TYPE_LOAD);
            } else { /* working on numerical values */
               char err_str[256];
               uint32_t dom_type = DOMINANT_TYPE_LOAD;

               if (parse_ulong_val(&dval, nullptr, type, load_value, nullptr, 0)) {
                  /* when nullptr is passed as load_adjustments, does it mean
                   * - we do not need to consider them
                   * - we always want to consider them and need to fetch them locally (old behaviour)
                   * I would guess that we don't need to consider them in many cases, e.g. in client code.
                   * But for safety reasons keep the old behaviour - in most cases, esp. in scheduler
                   * we pass valid load_adjustments to this function and have overcome the issue CS-450.
                   * @todo do a full testsuite run with LOAD_ADJUSTMENTS_LATE_FETCH unset and re-decide.
                   */
#define LOAD_ADJUSTMENTS_LATE_FETCH
#ifdef LOAD_ADJUSTMENTS_LATE_FETCH
               ocs::tom::ListPtr<ocs::tom::CE> local_load_adjustments;
               if (load_adjustments == nullptr) {
                  local_load_adjustments = sconf_get_job_load_adjustments();
                  load_adjustments = local_load_adjustments.get();
               }
#endif
               // look for 'name' in our load_adjustments list
               const auto job_load = ocs::tom::find_first<CE_name>(load_adjustments, attrname);
               if (job_load != nullptr) {
                  const char *s;
                  double load_correction;

                  s = ocs::tom::get<CE_stringval>(job_load);
                  if (!parse_ulong_val(&load_correction, nullptr, type, s, err_str, 255)) {
                     ERROR(MSG_SCHEDD_LOADADJUSTMENTSVALUEXNOTNUMERIC_S , attrname);
                  } else if (lc_factor) {
                     double old_dval;
                     uint32_t relop;
                     if (strncmp(attrname, "np_", 3) == 0) {
                        int nproc = load_list_get_nproc(load_attr);
                        if (nproc != 1) {
                           DPRINTF("fillComplexFromHost: dividing lc_factor for \"%s\" with value %f by %d to %f\n", attrname, lc_factor, nproc, lc_factor / nproc);
                           lc_factor /= nproc;
                        }
                     }
                     load_correction *= lc_factor;

                     /* it depends on relop in complex config whether load_correction is pos/neg */
                     if ( (relop = ocs::tom::get<CE_relop>(cplx_el)) == CMPLXGE_OP || relop == CMPLXGT_OP){
                        old_dval = dval;
                        dval += load_correction;
                     } else {
                        old_dval = dval;
                        dval -= load_correction;
                     }

                     DPRINTF("%s: uc: %f c(%f): %f\n", attrname, old_dval, lc_factor, dval);
                     dom_type = DOMINANT_TYPE_CLOAD;
                  }
               }

               /* we can have a user, who wants to override the incoming load value. This is no
                  problem for consumables, but for fixed values. A custom fixed value is a per
                  slot value (stored in CE_doubleval) and a load value is a per job value (stored
                  in CE_pj_doubleval). 
   
                  This code changes a fixed custom value from a per slot to a per job value!!
               */
               if ( !(ocs::tom::get<CE_dominant>(cplx_el) == DOMINANT_TYPE_VALUE) &&
                     (ocs::tom::get<CE_pj_dominant>(cplx_el) == DOMINANT_TYPE_VALUE)){
                  ocs::tom::set<CE_pj_doubleval>(cplx_el, ocs::tom::get<CE_doubleval>(cplx_el));
                  ocs::tom::set<CE_pj_stringval>(cplx_el, ocs::tom::get<CE_stringval>(cplx_el));
                  ocs::tom::set<CE_dominant>(cplx_el, DOMINANT_TYPE_VALUE);
                  ocs::tom::set<CE_pj_dominant>(cplx_el, layer | DOMINANT_TYPE_FIXED);
               } 
 
               if (!is_attr_prior2<CE_pj_doubleval, CE_pj_dominant>(cplx_el.get(), dval)){
                  ocs::tom::set<CE_pj_stringval>(cplx_el, load_value);
                  ocs::tom::set<CE_pj_dominant>(cplx_el, layer | dom_type);
                  ocs::tom::set<CE_pj_doubleval>(cplx_el, dval );
               }
            } /* end numerical load value */
         }/* end block */
      }
   }

   /* we are working on queue level, so we have to check for queue resource values */
   if (queue != nullptr) {
      bool created=false;
      if (cplx_el == nullptr) {
         cplx_el = copy_centry(centry_list, attrname);
         if (cplx_el == nullptr) {
            /* error */
            DRETURN(ocs::tom::ElementPtr<ocs::tom::CE>());
         }         
         ocs::tom::set<CE_dominant>(cplx_el, DOMINANT_TYPE_VALUE);
         ocs::tom::set<CE_pj_dominant>(cplx_el, DOMINANT_TYPE_VALUE);
         created = true;
      }
      if (!get_queue_resource(cplx_el.get(), queue, attrname) && created) {
         cplx_el.reset();
      }
   }

   DRETURN(cplx_el);
}

/**
 * @brief Extracts attribut information from the queue
 *
 * All fixed queue attributes are directly coded into the queue structure. These have to extraced
 * and formed into a CE structure. That is, what this function does. It takes a name for an attribut
 * and returns a full CE structure, if the attribut is set in the queue. Otherwise it returns nullptr.
 *
 * @param[out] queue_elem the complex entry to fill in
 * @param[in]  queue      the queue instance to read the attribute from
 * @param[in]  attrname   name of the attribute
 *
 * @return true if the attribute is set in the queue, false otherwise
 */
static bool get_queue_resource(const ocs::tom::Element<ocs::tom::CE> queue_elem,
                               const ocs::tom::ConstElement<ocs::tom::QU> queue, const char *attrname) {
   DENTER(BASIS_LAYER);
   double dval=0.0;
   const char *value=nullptr;
   char as_str[100];
   ocs::CEntry::Type type;
   int field;

   if (!queue_elem){
      /* error */
      DRETURN(false);
   }

   if (get_rsrc(attrname, true, &field, nullptr, nullptr, &type)!=0) {
      DPRINTF("is not a system queue attribute: %s\n", attrname);
      DRETURN(false);
   }

   /* read stuff from queue and set to new elements */
   switch(type) {
   case ocs::CEntry::Type::RSMAP:
   case ocs::CEntry::Type::INT: {
      const auto ulong_value = queue.get<std::uint32_t>(field);
      dval = (double)ulong_value;
      snprintf(as_str, 100, sge_u32, ulong_value);
      break;
   }

   case ocs::CEntry::Type::TIME:
   case ocs::CEntry::Type::MEM:
   case ocs::CEntry::Type::DOUBLE:
      if ((value = queue.get<const char *>(field))) {
         parse_ulong_val(&dval, nullptr, type, value, nullptr, 0);
      } 
      break;

   case ocs::CEntry::Type::BOOL: {
      const auto bool_value = queue.get<bool>(field);
      dval = (double)bool_value;
      snprintf(as_str, 100, "%d", (int)bool_value);
      break;
   }

   case ocs::CEntry::Type::STR:
   case ocs::CEntry::Type::CSTR:
   case ocs::CEntry::Type::RESTR:
      value = queue.get<const char *>(field);
      break;
   case ocs::CEntry::Type::HOST:
      value = queue.get<ocs::tom::Host>(field).c_str();
      break;
   case ocs::CEntry::Type::TYPE_ACC:
   case ocs::CEntry::Type::TYPE_LOG:
   case ocs::CEntry::Type::NONE:
      break;
   }
  

   if (!is_attr_prior2<CE_doubleval, CE_dominant>(queue_elem, dval)){
      ocs::tom::set<CE_dominant>(queue_elem , DOMINANT_LAYER_QUEUE|DOMINANT_TYPE_FIXED);
      ocs::tom::set<CE_doubleval>(queue_elem , dval);

      if(value){
         ocs::tom::set<CE_stringval>(queue_elem, value);
      } 
      else{
         ocs::tom::set<CE_stringval>(queue_elem , as_str);
      }
   }

   DRETURN(true);
}


/**
 * @brief The comparison of is_attr_prior() once the dominant value set of the upper element is chosen
 *
 * If the lower element has no value of the chosen set, the other set is copied into it and marked
 * as not dominant, as before. The value sets are chosen per call from two known at compile time,
 * so they are template arguments. Called per attribute in the scheduler, hence without DENTER.
 *
 * @tparam UsedVal the double value field of the chosen set (CE_doubleval or CE_pj_doubleval)
 * @tparam UsedStr the string value field of the chosen set (CE_stringval or CE_pj_stringval)
 * @tparam UsedDom the dominant field of the chosen set (CE_dominant or CE_pj_dominant)
 * @tparam UnusedVal the double value field of the other set
 * @tparam UnusedStr the string value field of the other set
 * @tparam UnusedDom the dominant field of the other set
 * @param upper_el attribut, which should be overridden by the second one
 * @param lower_el attribut, which want to override the first one; may get the other value set
 * @param relop the relational operator of @p upper_el, neither == nor !=
 * @return true, when the first attribut has a higher priority
 */
template <auto UsedVal, auto UsedStr, auto UsedDom, auto UnusedVal, auto UnusedStr, auto UnusedDom>
static bool ocs_attr_prior_by(const ocs::tom::Element<ocs::tom::CE> upper_el,
                              const ocs::tom::Element<ocs::tom::CE> lower_el, const uint32_t relop) {
   const uint32_t dom = ocs::tom::get<UsedDom>(lower_el);
   if (dom == 0 || (dom & DOMINANT_TYPE_VALUE)) {
      ocs::tom::set<UsedVal>(lower_el, ocs::tom::get<UnusedVal>(lower_el));
      ocs::tom::set<UsedStr>(lower_el, ocs::tom::get<UnusedStr>(lower_el));
      ocs::tom::set<UsedDom>(lower_el, ocs::tom::get<UnusedDom>(lower_el));
      ocs::tom::set<UnusedDom>(lower_el, static_cast<uint32_t>(DOMINANT_TYPE_VALUE));
   }

   const double upper_value = ocs::tom::get<UsedVal>(upper_el);
   const double lower_value = ocs::tom::get<UsedVal>(lower_el);

   if (relop == CMPLXGE_OP || relop == CMPLXGT_OP) {
      return upper_value >= lower_value;
   }
   return upper_value <= lower_value;
}

/**
 * @brief Compares two attribut instances with each other
 *
 * checks if the first given attribut instance has a higher priority than
 * second instance.
 * if the first is nullptr, it returns false
 * if the second or the second and first is nullptr, it returns true
 * if the "==" or "!=" operators are used, it is true
 * if both are the same, it may returns false.
 * otherwise it computes the minimum or maximum between the values.
 *
 * @param upper_el attribut, which should be overridden by the second one.
 * @param lower_el attribut, which want to override the first one.
 *
 * @return true, when the first attribut has a higher priority.
 */
static bool is_attr_prior(const ocs::tom::Element<ocs::tom::CE> upper_el,
                          const ocs::tom::Element<ocs::tom::CE> lower_el) {
   DENTER(BASIS_LAYER);

   uint32_t relop;
   bool ret;
   uint32_t dom;

   /* the order is important must not be changed */   
   if(!upper_el){
      DRETURN(false);
   }
   if(!lower_el){
      DRETURN(true);
   }

   relop = ocs::tom::get<CE_relop>(upper_el);
   if ((relop == CMPLXEQ_OP || relop == CMPLXNE_OP)) {
      DRETURN(true);
  }

   /* if both elements are the same, than I can not say which one is more important */
   if(upper_el == lower_el) {
      DRETURN(false);
   }   

   if ((dom = ocs::tom::get<CE_pj_dominant>(upper_el)) == 0 || (dom & DOMINANT_TYPE_VALUE) ){
      ret = ocs_attr_prior_by<CE_doubleval, CE_stringval, CE_dominant,
                              CE_pj_doubleval, CE_pj_stringval, CE_pj_dominant>(upper_el, lower_el, relop);
   }
   else {
      ret = ocs_attr_prior_by<CE_pj_doubleval, CE_pj_stringval, CE_pj_dominant,
                              CE_doubleval, CE_stringval, CE_dominant>(upper_el, lower_el, relop);
   }

   DRETURN(ret);
}

/**
 * @brief Checks, if the set value in the structure has a higher priority
 *
 * Computes the priority between a given structure and its values and a new value. This
 * is done on some basic rules. If the value is not set (dominant ==  DOMINANT_TYPE_VALUE)
 * or which relational opperator is used. If this is not enough, the two values are compared
 * and based on the opperator, it returns a true or false:
 * if no value is set in the structure: false
 * if the relops are == or != : true
 * if the relops are >= or > : true, when the new value is smaller than the old one
 * if the relops are <= or < : true, when the new value is bigger than the old one
 *
 * @tparam V which value field to use (CE_doubleval or CE_pj_doubleval)
 * @tparam D which dominant field to use (CE_dominant, CE_pj_dominant)
 * @param upper_el target structure
 * @param lower_value new value
 *
 * @return true, if the value in the structure has the higher priority
 */
template <auto V, auto D>
static bool is_attr_prior2(const ocs::tom::Element<ocs::tom::CE> upper_el, const double lower_value) {
   DENTER(BASIS_LAYER);

   uint32_t relop;
   uint32_t dom;
   bool ret;
   double upper_value;

   if ((dom = ocs::tom::get<D>(upper_el)) == 0 || (dom & DOMINANT_TYPE_VALUE) ){
      DRETURN(false);
   }

   relop = ocs::tom::get<CE_relop>(upper_el);
   if ((relop == CMPLXEQ_OP || relop == CMPLXNE_OP)) {
      DRETURN(true);
   }

   upper_value = ocs::tom::get<V>(upper_el);

   if (relop == CMPLXGE_OP || relop == CMPLXGT_OP ){
      ret = upper_value >= lower_value ? true : false;
   } else {
      ret = upper_value <= lower_value ? true : false;
   }
   DRETURN(ret);
}

/**
 * @brief Builds the complete attribute list of one execution host
 *
 * The global attributes are merged in, so the result is what a request has to
 * be matched against for this host.
 *
 * @param[in] host             the execution host, or an empty handle for the
 *                             global attributes alone
 * @param[in] exechost_list    all execution hosts, to find the global host
 * @param[in] centry_list      the system wide attribute configuration
 * @param[in] load_adjustments the job load adjustments of the scheduler
 *                             configuration; an empty handle fetches them from
 *                             the data store of the calling thread
 *
 * @return the attribute list, owned by the caller, or an empty handle if no
 *         attribute exists
 */
ocs::tom::ListPtr<ocs::tom::CE>
host_complexes2scheduler(const ocs::tom::ConstElement<ocs::tom::EH> host,
                         const ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                         const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                         const ocs::tom::ConstList<ocs::tom::CE> load_adjustments) {
   DENTER(TOP_LAYER);

   if (host == nullptr) {
      DPRINTF("!!missing host!!\n");
   }
   /* build global complex and add it to result */
   DRETURN(get_attribute_list(exechost_list.find_first<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME)), host,
                              ocs::tom::ConstElement<ocs::tom::QU>(), centry_list, load_adjustments));
}

/**
 * @brief Builds the complete attribute list of one queue instance
 *
 * All templates are filled with the actual values of the queue, its host and
 * the global host, so the scheduler can decide from this list alone whether a
 * request matches the queue.
 *
 * @param[in] queue            the queue instance, or an empty handle for the
 *                             global attributes alone
 * @param[in] exechost_list    all execution hosts, to find the queue's host
 *                             and the global host
 * @param[in] centry_list      the system wide attribute configuration
 * @param[in] load_adjustments the job load adjustments of the scheduler
 *                             configuration; an empty handle fetches them from
 *                             the data store of the calling thread
 *
 * @return the attribute list, owned by the caller, or an empty handle if no
 *         attribute exists
 */
ocs::tom::ListPtr<ocs::tom::CE>
queue_complexes2scheduler(const ocs::tom::ConstElement<ocs::tom::QU> queue,
                          const ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                          const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                          const ocs::tom::ConstList<ocs::tom::CE> load_adjustments) {
   DENTER(BASIS_LAYER);

   const auto host = queue != nullptr
                     ? exechost_list.find_first<EH_name>(ocs::tom::get<QU_qhostname>(queue))
                     : ocs::tom::ConstElement<ocs::tom::EH>();
   DRETURN(get_attribute_list(exechost_list.find_first<EH_name>(ocs::tom::Host(SGE_GLOBAL_NAME)), host, queue, centry_list,
                              load_adjustments));
}

/**
 * @brief Generates a list of attributes from the given names
 *
 * Assembles a list of attributes for a given queue, host, global, which contains all
 * the specified elements. The general sort order is, global, host, queue. If an
 * element could not be found, it will not exist. If no elements exist, the function
 * will return nullptr
 *
 * @param global global host
 * @param host host (or nullptr, if only global resources are asked for )
 * @param queue queue (or nullptr, if only global / host resources are asked for)
 * @param centry_list the system wide attribut config list
 * @param load_adjustments the job load adjustments (`CE`); nullptr fetches them from the data store
 * @param attrnames ST list of attribute names
 *
 * @return the list of attributes, owned by the caller, or an empty handle
 */
static ocs::tom::ListPtr<ocs::tom::CE>
get_attribute_list_by_names(const ocs::tom::ConstElement<ocs::tom::EH> global, const ocs::tom::ConstElement<ocs::tom::EH> host,
                            const ocs::tom::ConstElement<ocs::tom::QU> queue,
                            const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                            const ocs::tom::ConstList<ocs::tom::CE> load_adjustments,
                            const ocs::tom::ConstList<ocs::tom::ST> attrnames) {
   ocs::tom::ListPtr<ocs::tom::CE> list;

   for (const auto elem : attrnames) {
      auto attr = get_attribute_by_name(global, host, queue, ocs::tom::get<ST_name>(elem), centry_list,
                                        load_adjustments, DISPATCH_TIME_NOW, 0);
      if (attr != nullptr) {
         if (list == nullptr) {
            list = ocs::tom::ListPtr<ocs::tom::CE>::create("attr");
         }
         list->push_back(std::move(attr));
      }
   }

   return list;
}


/**
 * @brief Generates a list for all defined elements in a queue, host, global
 *
 * Generates a list for all attributes defined at the given queue, host, global.
 *
 * @param global global host
 * @param host host (or nullptr, if only global attributes are important)
 * @param queue queue (or nullptr if only host/global attributes are important)
 * @param centry_list system wide attribute config list
 * @param load_adjustments the job load adjustments (`CE`); nullptr fetches them from the data store
 *
 * @return the list of attributes, owned by the caller, or an empty handle if no attributes exist
 */
static ocs::tom::ListPtr<ocs::tom::CE>
get_attribute_list(const ocs::tom::ConstElement<ocs::tom::EH> global, const ocs::tom::ConstElement<ocs::tom::EH> host,
                   const ocs::tom::ConstElement<ocs::tom::QU> queue, const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                   const ocs::tom::ConstList<ocs::tom::CE> load_adjustments) {
   DENTER(BASIS_LAYER);

   auto filter = ocs::tom::ListPtr<ocs::tom::ST>::create("");

   if (global != nullptr) {
      build_name_filter<HL_name>(filter.get(), ocs::tom::get<EH_load_list>(global));
      build_name_filter<CE_name>(filter.get(), ocs::tom::get<EH_consumable_config_list>(global));
   }    
   if (host != nullptr) {
      build_name_filter<HL_name>(filter.get(), ocs::tom::get<EH_load_list>(host));
      build_name_filter<CE_name>(filter.get(), ocs::tom::get<EH_consumable_config_list>(host));
   } 

   if (queue != nullptr) {
      for (int x = 0; x < max_queue_resources; x++) {
         if (filter->find_first<ST_name>(queue_resource[x].name) == nullptr) {
            filter->emplace_back<ST_name>(queue_resource[x].name);
         }
      }
      build_name_filter<CE_name>(filter.get(), ocs::tom::get<QU_consumable_config_list>(queue));
   }

   DRETURN(get_attribute_list_by_names(global, host, queue, centry_list, load_adjustments, filter.get()));
}

/**
 * @brief Adds the names of a list to a name filter, each name once
 *
 * @tparam F the name field of the list's object type, `HL_name` for a host's
 *           load list or `CE_name` for a complex value list
 * @param filter the names collected so far, extended here
 * @param list   the list whose names are added, may be an empty handle
 */
template <auto F>
static void build_name_filter(const ocs::tom::List<ocs::tom::ST> filter,
                              const ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> list) {
   DENTER(BASIS_LAYER);

   for (const auto current : list) {
      const char *name = ocs::tom::get<F>(current);

      if (filter.find_first<ST_name>(name) == nullptr) {
         filter.emplace_back<ST_name>(name);
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Matches a string value against a pattern, for all string types
 *
 * @param[in] type the attribute type, which decides how the match is done
 * @param[in] s1   the pattern
 * @param[in] s2   the string to match against the pattern
 *
 * @return non-zero if the string matches the pattern
 */
int string_base_cmp(ocs::CEntry::Type type, const char *s1, const char *s2) {
   return sge_eval_expression(type, s1, s2, nullptr);
}

/**
 * @brief The previous implementation of string_base_cmp()
 *
 * Kept so the expression based implementation can be compared against it in
 * performance tests. Not used by the scheduler.
 *
 * @param[in] type the attribute type, which decides how the match is done
 * @param[in] s1   the pattern
 * @param[in] s2   the string to match against the pattern
 *
 * @return non-zero if the string matches the pattern
 */
int string_base_cmp_old(ocs::CEntry::Type type, const char *s1, const char *s2) {

   int match=0;

   switch(type){
      case ocs::CEntry::Type::STR: match = strcmp(s1, s2);
         break;
      case ocs::CEntry::Type::CSTR:  match = strcasecmp(s1, s2);
         break;
      case ocs::CEntry::Type::HOST:  match = sge_hostcmp(s1, s2);
         break;
      case ocs::CEntry::Type::RESTR:  {
                           char *s = nullptr;
                           struct saved_vars_s *context=nullptr;
                           for (s=sge_strtok_r(s1, "|", &context); s; s=sge_strtok_r(nullptr, "|", &context)) {
                              /* Old implementation shloud be kept for performance tests */                              
                              if ((match = fnmatch(s, s2, 0)) == 0) {
                                 break;
                              }   
                           }
                           sge_free_saved_vars(context);
                           context = nullptr;
                        }
         break;
      default: match = -1;
   }
   return match;
}


/* compare string type attributes under consideration of relop */
static int string_cmp(ocs::CEntry::Type type, uint32_t relop, const char *request,
const char *offer) {
   int match;

   switch(relop) {
   case CMPLXEQ_OP:
      match = (string_base_cmp(type, request, offer) == 0);
      break;
   case CMPLXLE_OP :
      match = (string_base_cmp(type, request, offer) <= 0);
      break;
   case CMPLXLT_OP :
      match = (string_base_cmp(type, request, offer) < 0);
      break;
   case CMPLXGT_OP :
      match = (string_base_cmp(type, request, offer) > 0);
      break;
   case CMPLXGE_OP :
      match = (string_base_cmp(type, request, offer) >= 0);
      break;
   case CMPLXNE_OP :
      match = (string_base_cmp(type, request, offer) != 0);
      break;
   default:
      match = 0; /* default -> no match */
   }

   return match;
}

static int resource_cmp(uint32_t relop, double req, double src_dl) {
   DENTER(CULL_LAYER);

   int match;

   switch(relop) {
   case CMPLXEQ_OP :
      match = (req==src_dl);
      break;
   case CMPLXLE_OP :
      match = (req<=src_dl);
      break;
   case CMPLXLT_OP :
      match = (req<src_dl);
      break;
   case CMPLXGT_OP :
      match = (req>src_dl);
      break;
   case CMPLXGE_OP :
      match = (req>=src_dl);
      break;
   case CMPLXNE_OP :
      match = (req!=src_dl);
      break;
   case CMPLXEXCL_OP :
      match = (req == 0 || req<=src_dl);
      break;
   default:
      match = 0; /* default -> no match */
   }

   DPRINTF(" %f %s %f -> match = %d\n", req, map_op2str(relop), src_dl, match);

   DRETURN(match);
}

/**
 * @brief Matches a request against what an object offers
 *
 * The type of the comparison comes from the **request**, `req_cplx`, and the
 * relational operator of the attribute decides what "matches" means.
 *
 * @param[in]  slots             number of slots the request is made for
 * @param[in]  req_cplx          the requested attribute (`CE`)
 * @param[in]  src_cplx          what the object offers (`CE`)
 * @param[out] availability_text receives the diagnosis when the match fails,
 *                               at least #STR_LEN_AVAIL_TEXT bytes
 * @param[in]  is_threshold      true when matching a threshold, where either
 *                               half matching is enough
 * @param[in]  force_existence   true if an attribute that does not exist at
 *                               the object makes the match fail
 *
 * @return 1 if the request is satisfied, 0 otherwise
 */
int compare_complexes(int slots, const ocs::tom::ConstElement<ocs::tom::CE> req_cplx,
                      const ocs::tom::ConstElement<ocs::tom::CE> src_cplx, char *availability_text,
                      int is_threshold, int force_existence) {
   DENTER(TOP_LAYER);

   uint32_t used_relop = 0;
   double req_dl, src_dl;
   int match, m1, m2;
   const char *offer;
   char dom_str[5];
#define STR_LEN_AVAIL_TEXT 2048   
   char availability_text1[STR_LEN_AVAIL_TEXT];
   char availability_text2[STR_LEN_AVAIL_TEXT]; 
   dstring resource_string = DSTRING_INIT;

   const char *name = ocs::tom::get<CE_name>(src_cplx);
   auto type = static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(src_cplx));
   uint32_t relop = ocs::tom::get<CE_relop>(src_cplx);

   if (is_threshold) {
      switch (relop) {
      case CMPLXLE_OP:
         used_relop = CMPLXGT_OP;
         break;
      case CMPLXGT_OP:
         used_relop = CMPLXLE_OP;
         break;
      case CMPLXLT_OP:
         used_relop = CMPLXGE_OP;
         break;
      case CMPLXGE_OP:
         used_relop = CMPLXLT_OP;
         break;
      case CMPLXNE_OP:
      case CMPLXEQ_OP:
      default:
         used_relop = relop;
         break;
      }
   } else {
      used_relop = relop;
   }
   switch (type) {
      const char *request;
      double req_all_slots;

   case ocs::CEntry::Type::STR:
   case ocs::CEntry::Type::CSTR:
   case ocs::CEntry::Type::HOST:
   case ocs::CEntry::Type::RESTR:
      request = ocs::tom::get<CE_stringval>(req_cplx);

      offer = ocs::tom::get<CE_stringval>(src_cplx);
      monitor_dominance(dom_str, ocs::tom::get<CE_dominant>(src_cplx));
#if 0
      DPRINTF("%s(\"%s\", \"%s\")\n", type==ocs::CEntry::Type::STR?"strcmp":"strcasecmp", request, offer);
#endif
      match = string_cmp(type, used_relop, request, offer);
      snprintf(availability_text, STR_LEN_AVAIL_TEXT, "%s:%s=%s", dom_str, name, offer);
#if 0
      DPRINTF("-l %s=%s, Q: %s:%s%s%s, Comparison: %s\n", name, request, dom_str, name, map_op2str(relop), offer, match?"ok":"no match");
#endif
      DRETURN(match);

   case ocs::CEntry::Type::RSMAP:
   case ocs::CEntry::Type::INT:
   case ocs::CEntry::Type::TIME:
   case ocs::CEntry::Type::MEM:
   case ocs::CEntry::Type::BOOL:
   case ocs::CEntry::Type::DOUBLE:
      // CE_doubleval carries the parsed amount; see the comment in ri_time_by_slots().
      // The previous code re-parsed CE_stringval here and silently substituted 0 when the
      // parse failed, which turned a malformed request into a request for nothing.
      req_dl = ocs::tom::get<CE_doubleval>(req_cplx);

      if (is_threshold) {
         m1 = m2 = 0; /* nothing exceeded per default */
      } else {
         m1 = m2 = 1; /* matched per default */
      }

      /* is there a per job limit */
      if (!(ocs::tom::get<CE_pj_dominant>(src_cplx) & (DOMINANT_TYPE_VALUE))) {
         /* Actually request matching for utilization attributes 
            fails if request is higher than what we get as per job 
            limit also for attributes marked as consumable.
        
            In future we could say that the actual value (being a load 
            value or a consumable resource value) plus the jobs request 
            may not exceed the maximal utilization from (global) host 
            or queue - but only if the attributes marked as consumable. */
         src_dl = ocs::tom::get<CE_pj_doubleval>(src_cplx);
         
         req_all_slots = req_dl*slots;

         m1 = resource_cmp(used_relop, req_all_slots, src_dl);

         monitor_dominance(dom_str, ocs::tom::get<CE_pj_dominant>(src_cplx));

         switch (type) {
         case ocs::CEntry::Type::BOOL:
            sge_dstring_copy_string(&resource_string, (src_dl > 0)?"true":"false");
#if 0
            snprintf(availability_text1, sizeof(availability_text1), "%s:%s=%s", dom_str, name, src_dl?"true":"false");
            DPRINTF("-l %s=%f, Q: %s:%s:%f, Comparison(1): %s\n",
                    name, req_all_slots, dom_str, map_op2str(used_relop),
                    src_dl, m1?"ok":"no match");
#endif
            break;
         case ocs::CEntry::Type::MEM:
            double_print_memory_to_dstring(src_dl, &resource_string);
#if 0
            { 
               dstring request_string = DSTRING_INIT;

               double_print_memory_to_dstring(req_dl, &request_string);
               DPRINTF("%d times of -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                       slots, name, sge_dstring_get_string(&request_string),
                       dom_str, name, map_op2str(used_relop),
                       sge_dstring_get_string(&resource_string),
                       m1?"ok":"no match");
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         case ocs::CEntry::Type::TIME:
            double_print_time_to_dstring(src_dl, &resource_string, true);
#if 0            
            {
               dstring request_string = DSTRING_INIT;

               double_print_time_to_dstring(req_dl, &request_string, true);
               DPRINTF("%d times of -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                       slots, name, sge_dstring_get_string(&request_string),
                       dom_str, name, map_op2str(used_relop),
                       sge_dstring_get_string(&resource_string),
                       m1?"ok":"no match");
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         default:
            double_print_to_dstring(src_dl, &resource_string);
            break;
         } 
         snprintf(availability_text1, STR_LEN_AVAIL_TEXT, "%s:%s=%s", dom_str, name, sge_dstring_get_string(&resource_string));
      }

      /* is there a per slot limit */
      if (!(ocs::tom::get<CE_dominant>(src_cplx) & (DOMINANT_TYPE_VALUE)) ||     /* per slot set || */
              ((ocs::tom::get<CE_dominant>(src_cplx) & (DOMINANT_TYPE_VALUE)) && /* (per slot not set && */
           (ocs::tom::get<CE_pj_dominant>(src_cplx) & (DOMINANT_TYPE_VALUE)) &&  /* and per job not set) */
            force_existence)) {

         src_dl = ocs::tom::get<CE_doubleval>(src_cplx);
         req_all_slots = req_dl;
         m2 = resource_cmp(used_relop, req_all_slots, src_dl);

         monitor_dominance(dom_str, ocs::tom::get<CE_dominant>(src_cplx));

         switch (type) {
         case ocs::CEntry::Type::BOOL:
            sge_dstring_copy_string(&resource_string, (src_dl > 0)?"true":"false");
#if 0
            DPRINTF("-l %s=%f, Q: %s:%s%s%f, Comparison(2): %s\n",
                    name, req_dl?"true":"false", dom_str, name,
                    map_op2str(used_relop),
                    src_dl?"true":"false", m2?"ok":"no match");
#endif
            break;
         case ocs::CEntry::Type::MEM:
            double_print_memory_to_dstring(src_dl, &resource_string);
#if 0            
            {
               dstring request_string = DSTRING_INIT;

               double_print_memory_to_dstring(req_dl, &request_string);
               DPRINTF("per slot -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                       name, sge_dstring_get_string(&request_string),
                       dom_str, name, map_op2str(used_relop),
                       sge_dstring_get_string(&resource_string),
                       m2?"ok":"no match");
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         case ocs::CEntry::Type::TIME:
            double_print_time_to_dstring(src_dl, &resource_string, true);
#if 0            
            {
               dstring request_string = DSTRING_INIT;

               double_print_time_to_dstring(req_dl, &request_string, true);
               DPRINTF("per slot -l %s=%s, Q: %s:%s%s%s, Comparison: %s\n",
                       name, sge_dstring_get_string(&request_string),
                       dom_str, name, map_op2str(used_relop),
                       sge_dstring_get_string(&resource_string),
                       m2?"ok":"no match");
               sge_dstring_free(&request_string);
            }
#endif            
            break;
         default:
            double_print_to_dstring(src_dl, &resource_string);
            break;
         } 
         snprintf(availability_text2, STR_LEN_AVAIL_TEXT, "%s:%s=%s", dom_str, name, sge_dstring_get_string(&resource_string));
      }
      sge_dstring_free(&resource_string);
      if (is_threshold) {
         match = m1 || m2;
      } else {
         match = m1 && m2;
         if (!m1) {
            sge_strlcpy(availability_text, availability_text1, STR_LEN_AVAIL_TEXT);
         } else if (!m2) {
            sge_strlcpy(availability_text, availability_text2, STR_LEN_AVAIL_TEXT);
         } else {
            sge_strlcpy(availability_text, "", STR_LEN_AVAIL_TEXT);
         }
      }
      DRETURN(match);

   default:  /* should never reach this -> undefined type */
      *availability_text = '\0';
      break;
   }
   DRETURN(0);
}


/**
 * @brief Returns an attribut by name
 *
 * It looks into the different configurations on host, global and queue and returns
 * the attribute, which was asked for. It the attribute is defined multiple times, only
 * the valid one is returned.
 *
 * @param global the global host
 * @param host a given host can be nullptr, then only the global host is important
 * @param queue a queue on the given host, can be null, then only the host and global is important
 * @param attrname the attribute name one is looking for
 * @param centry_list the system wide attribute config list
 * @param load_adjustments the load adjustments to apply, or nullptr to have
 *                         them fetched from the scheduler configuration
 * @param start_time begin of the time interval one asks for the resource
 * @param duration the duration of the interval
 *
 * @return the element one is looking for (a copy owned by the caller), or an empty handle
 */
ocs::tom::ElementPtr<ocs::tom::CE>
get_attribute_by_name(const ocs::tom::ConstElement<ocs::tom::EH> global, const ocs::tom::ConstElement<ocs::tom::EH> host,
                      const ocs::tom::ConstElement<ocs::tom::QU> queue, const char *attrname,
                      const ocs::tom::ConstList<ocs::tom::CE> centry_list,
                      const ocs::tom::ConstList<ocs::tom::CE> load_adjustments, uint64_t start_time, uint64_t duration) {
   DENTER(BASIS_LAYER);

   ocs::tom::ElementPtr<ocs::tom::CE> ret_el;

   if (global != nullptr) {
      double lc_factor = 0;
      const auto load_attr = ocs::tom::get<EH_load_list>(global);
      const auto config_attr = ocs::tom::get<EH_consumable_config_list>(global);
      const auto actual_attr = ocs::tom::get<EH_resource_utilization>(global);

      /* is there a multiplier for load correction (may be not in qstat, qmon etc) */
      if (ocs::tom::has<EH_load_correction_factor>(global)) {
         if ((lc_factor=ocs::tom::get<EH_load_correction_factor>(global)) != 0) {
            lc_factor = ((double)lc_factor)/100;
         }   
      }
      ret_el = get_attribute(attrname, config_attr, actual_attr, load_attr, centry_list, load_adjustments,
                             ocs::tom::ConstElement<ocs::tom::QU>(), DOMINANT_LAYER_GLOBAL, lc_factor, nullptr, false,
                             start_time, duration);
   } 

   if (host != nullptr) {   
      double lc_factor = 0;
      const auto load_attr = ocs::tom::get<EH_load_list>(host);
      const auto config_attr = ocs::tom::get<EH_consumable_config_list>(host);
      const auto actual_attr = ocs::tom::get<EH_resource_utilization>(host);

      /* is there a multiplier for load correction (may be not in qstat, qmon etc) */
      if (ocs::tom::has<EH_load_correction_factor>(host)) {
         if ((lc_factor=ocs::tom::get<EH_load_correction_factor>(host)) != 0) {
            lc_factor = ((double)lc_factor)/100;
         }
      }
      auto host_el = get_attribute(attrname, config_attr, actual_attr, load_attr, centry_list, load_adjustments,
                                   ocs::tom::ConstElement<ocs::tom::QU>(), DOMINANT_LAYER_HOST, lc_factor, nullptr,
                                   false, start_time, duration);
      // the host value wins unless the global one has the higher priority; the loser is freed with its handle
      if (host_el != nullptr && (ret_el == nullptr || !is_attr_prior(ret_el.get(), host_el.get()))) {
         ret_el = std::move(host_el);
      }
   }

   if (queue != nullptr) {
      const auto config_attr = ocs::tom::get<QU_consumable_config_list>(queue);
      const auto actual_attr = ocs::tom::get<QU_resource_utilization>(queue);
      
      auto queue_el = get_attribute(attrname, config_attr, actual_attr, ocs::tom::ConstList<ocs::tom::HL>(), centry_list,
                                    load_adjustments, queue, DOMINANT_LAYER_QUEUE, 0.0, nullptr, false, start_time,
                                    duration);
      if (queue_el != nullptr && (ret_el == nullptr || !is_attr_prior(ret_el.get(), queue_el.get()))) {
         ret_el = std::move(queue_el);
      }
   }
   DRETURN(ret_el);
}

#ifdef TEST
/* 
for testing purposes compile (on linux) with:
gcc -Wall -DLINUX -DTEST -o complex complex.c ../LINUX/sge_parse_num_par.o ../LINUX/log.o ../LINUX/utility.o  ../LINUX/pack.o ../LINUX/free.o ../LINUX/io.o ../LINUX/libcull.a 
*/

int main(int argc, char *argv[], char *envp[]) {
   lListElem *l;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
  
#ifdef __SGE_COMPILE_WITH_GETTEXT__   
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(nullptr,nullptr);
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */
 

   if (argc!=3) {
      fprintf(stdout, "usage: complex read_file write_file\n");
      exit(1);
   }

   l = read_cmplx(argv[1], "cmplx_name", &alp);

   if (answer_list_has_error(&alp)) {
      answer_list_output(&alp);
      DRETURN(1);
   }
   

   write_cmplx(1, argv[2], l, nullptr, &alp);

   if (answer_list_has_error(&alp)) {
      answer_list_output(&alp);
      DRETURN(1);
   }

   alp.reset();
   DRETURN(0);
}
#endif

/**
 * @brief Check, if -l request forecloses cluster queue
 *
 * Do -l matching with the aim to foreclose the entire cluster queue.
 * Each cluster queue configuration profile must specify a fixed value
 * otherwise we can't rule out a cluster queue. Both complex_values and
 * queue resource limits are checked.
 *
 * @param hard_resource_list resource list -l (CE)
 * @param cq cluster queue (CQ)
 * @param centry_list complex entry list (CE)
 * @param single_slot true to match for a single slot only, false to take the
 *                    whole slot range of the job into account
 * @param unsatisfied diagnosis information, if rejected
 *
 * @return true, if the cluster queue is ruled out
 *
 * @note MT-NOTE: request_cq_rejected() is MT safe
 */
bool request_cq_rejected(const ocs::tom::ConstList<ocs::tom::CE> hard_resource_list,
                         const ocs::tom::ConstElement<ocs::tom::CQ> cq,
                         const ocs::tom::ConstList<ocs::tom::CE> centry_list, bool single_slot, dstring *unsatisfied) {
   DENTER(TOP_LAYER);

   const char *name, *request, *offer;
   uint32_t relop;
   ocs::CEntry::Type type;
   bool rejected;
   int match;

   for (const auto req : hard_resource_list) {
      int cqfld, valfld;
      name = ocs::tom::get<CE_name>(req);

      const auto ce = ocs::tom::find_first<CE_name>(centry_list, name);
      if (ce == nullptr) {
         sge_dstring_sprintf(unsatisfied, "unknown: " SFN, name);
         DRETURN(true);
      }

      request = ocs::tom::get<CE_stringval>(req);
      relop = ocs::tom::get<CE_relop>(ce);

      if (get_rsrc(name, true, nullptr, &cqfld, &valfld, &type)==0) {
         if (cqfld == 0) {
            continue;
         } 
      } else {
         type = static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(ce));
         cqfld = CQ_consumable_config_list;
         valfld = ACELIST_value;
      }

      /* use of resource_cmp() can be wrong for parallel jobs that request more than a single slot */
      if (!single_slot && type != ocs::CEntry::Type::STR && type != ocs::CEntry::Type::CSTR
          && type != ocs::CEntry::Type::HOST && type != ocs::CEntry::Type::RESTR)
         continue;

      rejected = true;
      // the attribute list of the cluster queue is chosen at run time, so is its type (N-H-21)
      for (const auto alist : ocs::tom::ConstRawElement(cq).get<ocs::tom::ConstRawList>(cqfld)) {
         if (valfld == ACELIST_value) {
            /* complex_values upper limit */
            // the list is a CQ_consumable_config_list here; the checked constructor confirms it is an ACELIST
            const auto val_ce = ocs::tom::find_first_in<ACELIST_value, CE_name>(
               ocs::tom::ConstElement<ocs::tom::ACELIST>(alist.raw()), name);
            if (!val_ce) {
               rejected = false;
               break;
            }
            offer = ocs::tom::get<CE_stringval>(val_ce);
         } else {
            /* queue resource upper limit */ 
            offer = alist.get<const char *>(valfld);
         }

         switch (type) {
         case ocs::CEntry::Type::STR:
         case ocs::CEntry::Type::CSTR:
         case ocs::CEntry::Type::HOST:
         case ocs::CEntry::Type::RESTR:
            match = string_cmp(type, relop, request, offer);
            break;

         case ocs::CEntry::Type::INT:
         case ocs::CEntry::Type::TIME:
         case ocs::CEntry::Type::MEM:
         case ocs::CEntry::Type::BOOL:
         case ocs::CEntry::Type::DOUBLE:
            {
               // the request side is already parsed (CE_doubleval); only the offer, which
               // comes from a queue or host configuration value, still needs parsing
               double req_dl = ocs::tom::get<CE_doubleval>(req);
               double off_dl;
               if (!parse_ulong_val(&off_dl, nullptr, type, offer, nullptr, 0)) {
                  DPRINTF("%s is not of type %s\n", offer, map_type2str(type));
                  match = 0;
               } else {
                  match = resource_cmp(relop, req_dl, off_dl);
               }
            }
            break;

         default:
            match = true; /* well */
            break;
         }
         if (match) {
            rejected = false;
            break;
         }
      }

      if (rejected) {
         DPRINTF("cluster queue \"%s\" will never match due to -l %s=%s\n", ocs::tom::get<CQ_name>(cq), name, request);
         sge_dstring_sprintf(unsatisfied, SFN "=" SFN, name, request);
         DRETURN(true);
      }

      DPRINTF("cluster queue \"%s\" might be suited according -l %s=%s\n", ocs::tom::get<CQ_name>(cq), name, request);
   }

   DRETURN(false);
}
