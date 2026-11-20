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
 * @brief Generic access to any CULL object, without knowing its type
 *
 * The implementation behind `sge_object.h`: the type table, the attribute
 * conversion helpers, and the verification the GDI layer runs on incoming
 * objects.
 *
 * @see sge_object.h
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_object_type.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/ocs_Role.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_ABOOL.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JJ.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PETR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"

#include "msg_common.h"

/// Debug layer the generic object access traces are written to
#define OBJECT_LAYER BASIS_LAYER

/// The type table, indexed by #sge_object_type; generated from the compile-time table #ocs_object_types
static constexpr const auto &object_base = ocs_object_descriptions;



namespace {
   /**
    * @brief Parse the name of a complex entry type, as `CE_valtype` takes it
    *
    * @param[out] answer_list receives the message naming an unknown type
    * @param value the name, e.g. `MEMORY`, case insensitive
    * @return the result; a failure stores nothing
    */
   ocs_object_parsed_symbol
   parse_ce_valtype(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value) {
      /* JG: TODO: we should better have a function map_str2type
       * and isn't there some sort of framework for mapping
       * strings to ints and vice versa? See QU_qtype implementation.
       */
      ocs::CEntry::Type type = ocs::CEntry::Type::NONE;

      for (int i = static_cast<int>(ocs::CEntry::Type::FIRST);
           type == ocs::CEntry::Type::NONE && i <= static_cast<int>(ocs::CEntry::Type::CE_LAST);
           i++) {
         if (strcasecmp(value, map_type2str(static_cast<ocs::CEntry::Type>(i))) == 0) {
            type = static_cast<ocs::CEntry::Type>(i);
         }
      }

      if (type == ocs::CEntry::Type::NONE) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_UNKNOWN_ATTR_TYPE_S,
                                 value);
         return {false, false, 0};
      }
      return {true, true, static_cast<uint32_t>(type)};
   }

   /**
    * @brief Parse a relational operator, as `CE_relop` takes it
    *
    * The symbols of the operators CMPLXEQ_OP to CMPLXEXCL_OP are compared (CS-3169); no message is written for an
    * unknown symbol.
    *
    * @param value the symbol, e.g. `<=`
    * @return the result; a failure stores nothing
    */
   ocs_object_parsed_symbol
   parse_ce_relop(const char *value) {
      uint32_t op = 0;

      // CS-3169: walk the operators, not the value types - map_op2str() returns "??" for out-of-range values,
      // which made "??" a valid relop
      for (uint32_t i = CMPLXEQ_OP; op == 0 && i <= CMPLXEXCL_OP; i++) {
         if (strcasecmp(value, map_op2str(i)) == 0) {
            op = i;
         }
      }

      if (op == 0) {
         /* error output necessary? Should be created by parsing function */
         return {false, false, 0};
      }
      return {true, true, op};
   }

   /**
    * @brief Parse a requestable setting, as `CE_requestable` takes it
    *
    * @param value `y`/`yes`, `n`/`no` or `f`/`forced`, case insensitive
    * @return the result; an unknown word stores #REQU_NO and fails without a message
    */
   ocs_object_parsed_symbol
   parse_ce_requestable(const char *value) {
      if (strcasecmp(value, "y") == 0 || strcasecmp(value, "yes") == 0) {
         return {true, true, REQU_YES};
      }
      if (strcasecmp(value, "n") == 0 || strcasecmp(value, "no") == 0) {
         return {true, true, REQU_NO};
      }
      if (strcasecmp(value, "f") == 0 || strcasecmp(value, "forced") == 0) {
         return {true, true, REQU_FORCED};
      }
      return {false, true, REQU_NO};
   }

   /**
    * @brief Parse a consumable setting, as `CE_consumable` takes it
    *
    * @param value `y`/`yes`, `n`/`no`, `j`/`job` or `h`/`host`, case insensitive
    * @return the result; an unknown word stores #CONSUMABLE_NO and fails without a message
    */
   ocs_object_parsed_symbol
   parse_ce_consumable(const char *value) {
      if (strcasecmp(value, "y") == 0 || strcasecmp(value, "yes") == 0) {
         return {true, true, CONSUMABLE_YES};
      }
      if (strcasecmp(value, "n") == 0 || strcasecmp(value, "no") == 0) {
         return {true, true, CONSUMABLE_NO};
      }
      if (strcasecmp(value, "j") == 0 || strcasecmp(value, "job") == 0) {
         return {true, true, CONSUMABLE_JOB};
      }
      if (strcasecmp(value, "h") == 0 || strcasecmp(value, "host") == 0) {
         return {true, true, CONSUMABLE_HOST};
      }
      return {false, true, CONSUMABLE_NO};
   }

   /**
    * @brief Parse a queue type list, as the cluster queue attribute (`AQTLIST_value`) takes it
    *
    * @param[out] answer_list receives the messages of the bit parser and the parse error
    * @param value the type names; nullptr is an error
    * @return the result; a failure stores nothing
    */
   ocs_object_parsed_symbol
   parse_attr_queue_type(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value) {
      if (value == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                 "<null>");
         return {false, false, 0};
      }
      uint32_t type;
      if (!sge_parse_bitfield_str(value, queue_types, &type, "", answer_list, true)) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QTYPE_INCORRECTSTRING, value);
         return {false, false, 0};
      }
      return {true, true, type};
   }

   /**
    * @brief Add the error for a missing value text to an answer list
    * @param[out] answer_list the answer list
    */
   void
   add_null_value_error(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                              "<null>");
   }

   /**
    * @brief Parse a whitespace or comma separated list of names and store it in a list field
    *
    * The single word `NONE` leaves the field as it was rather than storing a one element list.
    *
    * @tparam F the list field
    * @tparam KEY the string field of the list's elements the names are stored in
    * @param ep the element whose field is set
    * @param[out] answer_list receives the parse error
    * @param string the text to parse; nullptr is an error
    * @return true when the value was parsed
    */
   template <auto F, auto KEY>
   bool
   parse_name_list(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> ep,
                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
      DENTER(OBJECT_LAYER);

      if (string == nullptr) {
         add_null_value_error(answer_list);
         DRETURN(false);
      }

      auto list = ocs::tom::ListPtr<typename ocs::tom::FieldTraits<F>::Sub>::template from_string<KEY>(string,
                                                                                                    "\t \v\r,");
      if (!list) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                 string);
         DRETURN(false);
      }
      if (strcasecmp(NONE_STR, ocs::tom::get<KEY>(list->front())) != 0) {
         ocs::tom::set<F>(ep, std::move(list));
      }
      DRETURN(true);
   }

   /**
    * @brief Parse a complex entry list and store it in the cluster queue attribute
    *
    * Each entry is a `name=value` pair, as in a complex request.
    *
    * @param ep the attribute whose value is set
    * @param[out] answer_list receives the parse error
    * @param string the text to parse; nullptr is an error
    * @return true when the value was parsed and stored
    */
   bool
   parse_celist(const ocs::tom::Element<ocs::tom::ACELIST> ep, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                const char *string) {
      DENTER(TOP_LAYER);
      static int rule[] = {CE_name, CE_stringval, 0};

      if (string == nullptr) {
         add_null_value_error(answer_list);
         DRETURN(false);
      }

      // raw out-parameter of cull_parse_definition_list(), which creates the list with CE_Type
      lList *tmp_list = nullptr;
      const int parse_ret = cull_parse_definition_list(const_cast<char *>(string), &tmp_list, "", CE_Type, rule);
      auto list = ocs::tom::ListPtr<ocs::tom::CE>::from_typed(tmp_list);
      if (parse_ret != 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                 string);
         DRETURN(false);
      }
      ocs::tom::set<ACELIST_value>(ep, std::move(list));
      DRETURN(true);
   }

   /**
    * @brief Parse a subordinate list and store it in the cluster queue attribute
    *
    * Two forms are accepted: queue wise (`A.q=3,B.q=4`) and slot wise
    * (`slots=4(A.q:1:lr, B.q:2:sr)`). A slot wise list that fails to parse in
    * one of its entries clears the field; a queue wise list that fails leaves it
    * as it was.
    *
    * @param ep the attribute whose value is set
    * @param[out] answer_list receives the parse error
    * @param string the text to parse; nullptr is an error
    * @return true when the value was parsed and stored
    */
   bool
   parse_solist(const ocs::tom::Element<ocs::tom::ASOLIST> ep, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                const char *string) {
      DENTER(OBJECT_LAYER);
      bool ret = true;

      if (string == nullptr) {
         add_null_value_error(answer_list);
         DRETURN(false);
      }

      ocs::tom::ListPtr<ocs::tom::SO> tmp_list;
      struct saved_vars_s *last = nullptr;

      /*
       * The queue wise subordinate is defined like this:
       * subordinate_list  A.q=3,B.q=4
       * The slotwise subordinate is defined like this:
       * subordinate_list slots=4(A.q:1:lr, B.q:2:sr)
       * If the value of the "subordinate_list" key begins with
       * "slots=", it's slotwise subordinate, otherwise it's
       * queue wise subordinate.
       *
       * TODO: HP: This parser could be improved, now a queue wise subordinate
       * that begins with a queue called "slots" will be interpreted as
       * slotwise subordinate and therefore lead to an error.
       */
      const char *slots_text = sge_strtok_r(string, "=", &last);
      if (slots_text != nullptr && strncasecmp("slots", slots_text, 5) == 0) {
         /*
          * slot-wise suspend on subordinate
          */
         char *end_ptr = nullptr;
         lUlong slots_sum_value = 0;

         /*
          * format of string: slots=8(queue1.q:3:sr, queue2.q:2:lr)
          */
         /* the "slots=" was already skipped in the first sge_strtok_r() */
         /* parse the "8" in our example above */
         const char *slots_sum_text = sge_strtok_r(nullptr, "(", &last);
         if (slots_sum_text != nullptr) {
            slots_sum_value = strtol(slots_sum_text, &end_ptr, 10);
         } else {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                    string);
            ret = false;
         }
         if (end_ptr != nullptr && *end_ptr != '\0') {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                    string);
            ret = false;
         }
         if (ret) {
            /* point the "sub_queues_text" to the start of the sub queue list */
            const char *sub_queues_text = sge_strtok_r(nullptr, ")", &last);
            /* split up the original string from the first subordinated queue on */
            tmp_list = ocs::tom::ListPtr<ocs::tom::SO>::from_string<SO_name>(sub_queues_text, ",) \t");
            for (const auto tmp_elem : *tmp_list) {
               const char *queue_value = ocs::tom::get<SO_name>(tmp_elem);
               char *queuename = sge_strtok(queue_value, ":");
               char *value_str = sge_strtok(nullptr, ":");
               char *action_str = sge_strtok(nullptr, ":");

               sge_strip_blanks(queuename);
               sge_strip_blanks(value_str);
               sge_strip_blanks(action_str);

               if (queuename != nullptr) {
                  ocs::tom::set<SO_name>(tmp_elem, queuename);
               } else {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_ERRORPARSINGVALUEFORNM_S, string);
                  ret = false;
                  break;
               }

               if (slots_sum_value > 0) {
                  ocs::tom::set<SO_slots_sum>(tmp_elem, slots_sum_value);
               } else {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_ERRORPARSINGVALUEFORNM_S, string);
                  ret = false;
                  break;
               }

               if (value_str != nullptr) {
                  char *value_end = nullptr;
                  const uint32_t value = strtol(value_str, &value_end, 10);

                  if (value_end != nullptr && *value_end == '\0') {
                     ocs::tom::set<SO_seq_no>(tmp_elem, value);
                  } else {
                     answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                             MSG_ERRORPARSINGVALUEFORNM_S, string);
                     ret = false;
                     break;
                  }
               } else {
                  ocs::tom::set<SO_seq_no>(tmp_elem, 0);
               }

               if (action_str != nullptr) {
                  if (strcmp(action_str, "lr") == 0) {
                     ocs::tom::set<SO_action>(tmp_elem, SO_ACTION_LR);
                  } else if (strcmp(action_str, "sr") == 0) {
                     ocs::tom::set<SO_action>(tmp_elem, SO_ACTION_SR);
                  } else {
                     answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                             MSG_ERRORPARSINGVALUEFORNM_S, string);
                     ret = false;
                     break;
                  }
               } else {
                  /* default is "sr" */
                  ocs::tom::set<SO_action>(tmp_elem, SO_ACTION_SR);
               }
            }
            // a failed entry drops the whole list, and the field is cleared, as it always was
            if (!ret) {
               tmp_list.reset();
            }
            ocs::tom::set<ASOLIST_value>(ep, std::move(tmp_list));
         }
      } else {
         tmp_list = ocs::tom::ListPtr<ocs::tom::SO>::from_string<SO_name>(string, ", \t");
         if (tmp_list) {
            if (!strcasecmp("NONE", ocs::tom::get<SO_name>(tmp_list->front()))) {
               tmp_list.reset();
            } else {
               /*
                * queue instance-wise suspend on subordinate
                */
               for (const auto tmp_elem : *tmp_list) {
                  const char *queue_value = ocs::tom::get<SO_name>(tmp_elem);
                  const char *queuename = sge_strtok(queue_value, ":=");
                  const char *value_str = sge_strtok(nullptr, ":=");

                  ocs::tom::set<SO_name>(tmp_elem, queuename);
                  if (value_str != nullptr) {
                     char *endptr = nullptr;
                     const uint32_t value = strtol(value_str, &endptr, 10);

                     if (endptr != nullptr && *endptr == '\0') {
                        ocs::tom::set<SO_threshold>(tmp_elem, value);
                     } else {
                        answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                                MSG_ERRORPARSINGVALUEFORNM_S, string);
                        ret = false;
                        break;
                     }
                  } else {
                     /*
                      * No value is explicitely allowed
                      */
                  }
               }
               if (ret) {
                  ocs::tom::set<ASOLIST_value>(ep, std::move(tmp_list));
               }
            }
         }
      }
      sge_free_saved_vars(last);
      DRETURN(ret);
   }
} // namespace

/**
 * @brief Append a value of a field that is written as a word, see #ocs_object_symbolic
 *
 * Shared by the typed ocs_object_append_field() and by the spooling walkers, which take the kind from the
 * field's spooling record. A userset type without a bit of a name is written as the plain number, as it
 * always was.
 *
 * @param[out] buffer receives the text, appended
 * @param kind the kind of the field, not ocs_object_symbolic::none
 * @param value the field's value
 * @return the buffer's text, nullptr for ocs_object_symbolic::none
 *
 * @note MT-NOTE: ocs_object_append_symbolic() is MT safe
 */
const char *
ocs_object_append_symbolic(dstring *buffer, const ocs_object_symbolic kind, const uint32_t value) {
   DENTER(OBJECT_LAYER);

   const char *ret = nullptr;
   switch (kind) {
      case ocs_object_symbolic::ce_valtype:
         ret = sge_dstring_append(buffer, map_type2str(static_cast<ocs::CEntry::Type>(value)));
         break;
      case ocs_object_symbolic::ce_relop:
         ret = sge_dstring_append(buffer, map_op2str(value));
         break;
      case ocs_object_symbolic::ce_requestable:
         ret = sge_dstring_append(buffer, map_req2str(value));
         break;
      case ocs_object_symbolic::ce_consumable:
         ret = sge_dstring_append(buffer, map_consumable2str(value));
         break;
      case ocs_object_symbolic::qinstance_type:
         ret = ocs_qinstance_qtype_append_to_dstring(value, buffer);
         break;
      case ocs_object_symbolic::userset_type:
         ret = ocs_userset_type_append_to_dstring(value, buffer);
         if (ret == nullptr) {
            ret = sge_dstring_sprintf_append(buffer, sge_u32, value);
         }
         break;
      case ocs_object_symbolic::attr_queue_type:
         ret = qtype_append_to_dstring(value, buffer);
         break;
      case ocs_object_symbolic::none:
         break;
   }
   DRETURN(ret);
}

/**
 * @brief Parse a value of a field that is written as a word, see #ocs_object_symbolic
 *
 * Shared by the typed ocs_object_parse_field() and by the spooling walkers. What a failure reports and stores
 * differs by kind, as it always did: an unknown complex type reports and stores nothing, an unknown operator
 * stores nothing without a message, an unknown requestable or consumable word stores `NO` without a message, a
 * bad queue or userset type of an object stores the bits understood before, a bad queue type of a cluster queue
 * attribute reports and stores nothing.
 *
 * @param[out] answer_list receives the messages
 * @param kind the kind of the field, not ocs_object_symbolic::none
 * @param value the text
 * @return whether the text was understood, whether to store, and the value to store
 *
 * @note MT-NOTE: ocs_object_parse_symbolic() is not MT safe for the bit fields, as sge_parse_bitfield_str() is not
 */
ocs_object_parsed_symbol
ocs_object_parse_symbolic(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs_object_symbolic kind,
                          const char *value) {
   DENTER(OBJECT_LAYER);

   ocs_object_parsed_symbol ret{true, false, 0};
   switch (kind) {
      case ocs_object_symbolic::ce_valtype:
         ret = parse_ce_valtype(answer_list, value);
         break;
      case ocs_object_symbolic::ce_relop:
         ret = parse_ce_relop(value);
         break;
      case ocs_object_symbolic::ce_requestable:
         ret = parse_ce_requestable(value);
         break;
      case ocs_object_symbolic::ce_consumable:
         ret = parse_ce_consumable(value);
         break;
      case ocs_object_symbolic::qinstance_type: {
         const auto [ok, type] = ocs_qinstance_qtype_parse(value);
         ret = {ok, true, type};
         break;
      }
      case ocs_object_symbolic::userset_type: {
         const auto [ok, type] = ocs_userset_type_parse(answer_list, value);
         ret = {ok, true, type};
         break;
      }
      case ocs_object_symbolic::attr_queue_type:
         ret = parse_attr_queue_type(answer_list, value);
         break;
      case ocs_object_symbolic::none:
         break;
   }
   DRETURN(ret);
}

/**
 * @brief Parse a boolean
 *
 * Accepts `true`/`t`/`1`/`yes`/`y` and `false`/`f`/`0`/`no`/`n`, each
 * case insensitively.
 *
 * @param[out] answer_list receives the parse error
 * @param string the text to parse; nullptr is an error
 * @return the value, std::nullopt on an error
 */
std::optional<bool>
ocs_object_parse_bool(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
   DENTER(OBJECT_LAYER);

   std::optional<bool> ret;

   if (string != nullptr) {
      if (!strcasecmp(string, "true") || !strcasecmp(string, "t") || !strcmp(string, "1") ||
          !strcasecmp(string, "yes") || !strcasecmp(string, "y")) {
         ret = true;
      } else if (!strcasecmp(string, "false") || !strcasecmp(string, "f") || !strcmp(string, "0") ||
                 !strcasecmp(string, "no") || !strcasecmp(string, "n")) {
         ret = false;
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                 string);
      }
   } else {
      add_null_value_error(answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Check that a text is a value of a complex type, for a field that stores the text verbatim
 *
 * The cluster queue attributes of times, intervals and memory keep the form the user wrote; the text is only
 * validated here.
 *
 * @param[out] answer_list receives the parse error
 * @param type the complex type the text must be a value of: ocs::CEntry::Type::TIME or ocs::CEntry::Type::MEM
 * @param string the text to check; nullptr is an error
 * @return true when the text is a value of the type
 */
bool
ocs_object_check_value_string(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::CEntry::Type type,
                              const char *string) {
   DENTER(OBJECT_LAYER);

   bool ret = true;

   if (string != nullptr) {
      if (!parse_ulong_val(nullptr, nullptr, type, string, nullptr, 0)) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ERRORPARSINGVALUEFORNM_S,
                                 string);
         ret = false;
      }
   } else {
      add_null_value_error(answer_list);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Parse a 32 bit unsigned integer
 *
 * An empty text is 0.
 *
 * @param[out] answer_list receives the parse error
 * @param string the text to parse; nullptr is an error
 * @return the value, std::nullopt on an error
 */
std::optional<std::uint32_t>
ocs_object_parse_uint32(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
   DENTER(OBJECT_LAYER);

   std::optional<std::uint32_t> ret;

   if (string != nullptr) {
      if (strlen(string) == 0) {
         /*
          * Empty string will be parsed as '0'
          */
         ret = 0;
      } else {
         const double epsilon = 1.0E-12;
         char *end_ptr = nullptr;
         const double dbl_value = strtod(string, &end_ptr);
         const uint32_t ulong_value = dbl_value;

         if (dbl_value < 0 || dbl_value - ulong_value > epsilon) {
            /*
             * value to big for uint32_t variable
             */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_OBJECT_VALUENOTULONG_S,
                                    string);
         } else if (end_ptr != nullptr && *end_ptr == '\0') {
            ret = ulong_value;
         } else {
            /*
             * Not a number or
             * garbage after number
             */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ULONG_INCORRECTSTRING,
                                    string);
         }
      }
   } else {
      add_null_value_error(answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Parse a 64 bit unsigned integer
 *
 * An empty text is 0.
 *
 * @param[out] answer_list receives the parse error
 * @param string the text to parse; nullptr is an error
 * @return the value, std::nullopt on an error
 */
std::optional<std::uint64_t>
ocs_object_parse_uint64(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
   DENTER(OBJECT_LAYER);

   std::optional<std::uint64_t> ret;

   if (string != nullptr) {
      if (strlen(string) == 0) {
         /*
          * Empty string will be parsed as '0'
          */
         ret = 0;
      } else {
         const double epsilon = 1.0E-12;
         char *end_ptr = nullptr;
         const double dbl_value = strtod(string, &end_ptr);
         const uint64_t ulong_value = dbl_value;

         if (dbl_value < 0 || dbl_value - ulong_value > epsilon) {
            /*
             * value to big for uint32_t variable
             */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_OBJECT_VALUENOTULONG_S,
                                    string);
         } else if (end_ptr != nullptr && *end_ptr == '\0') {
            ret = ulong_value;
         } else {
            /*
             * Not a number or
             * garbage after number
             */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ULONG_INCORRECTSTRING,
                                    string);
         }
      }
   } else {
      add_null_value_error(answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Parse an int
 *
 * @param[out] answer_list receives the parse error
 * @param string the text to parse; nullptr is an error
 * @return the value, std::nullopt on an error
 */
std::optional<std::int32_t>
ocs_object_parse_int32(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
   DENTER(OBJECT_LAYER);

   std::optional<std::int32_t> ret;

   if (string != nullptr) {
      int value;

      if (sscanf(string, "%d", &value) == 1) {
         ret = value;
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_INT_INCORRECTSTRING, string);
      }
   } else {
      add_null_value_error(answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Parse a long
 *
 * @param[out] answer_list receives the parse error
 * @param string the text to parse; nullptr is an error
 * @return the value, std::nullopt on an error
 */
std::optional<std::int64_t>
ocs_object_parse_int64(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
   DENTER(OBJECT_LAYER);

   std::optional<std::int64_t> ret;

   if (string != nullptr) {
      long value;

      if (sscanf(string, "%ld", &value) == 1) {
         ret = value;
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_LONG_INCORRECTSTRING, string);
      }
   } else {
      add_null_value_error(answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Parse a double
 *
 * @param[out] answer_list receives the parse error
 * @param string the text to parse; nullptr is an error
 * @return the value, std::nullopt on an error
 */
std::optional<double>
ocs_object_parse_double(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string) {
   DENTER(OBJECT_LAYER);

   std::optional<double> ret;

   if (string != nullptr) {
      double value;

      if (sscanf(string, "%lf", &value) == 1) {
         ret = value;
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_DOUBLE_INCORRECTSTRING,
                                 string);
      }
   } else {
      add_null_value_error(answer_list);
   }
   DRETURN(ret);
}

/**
 * @brief Append the value of a field as an administrator writes it
 *
 * A field of a #ocs_object_symbolic kind is written as a word (ocs_object_append_symbolic()), a list of the
 * cluster queue attributes as its names or entries, anything else in the default form of its type: a number,
 * `TRUE`/`FALSE`, a string or `NONE` for an unset one. The text is appended, without quotes.
 *
 * @tparam F the field
 * @param ep the element, may be an empty handle
 * @param[out] buffer receives the text, appended
 * @return the buffer's text, nullptr for an empty handle
 *
 * @note MT-NOTE: ocs_object_append_field() is MT safe
 *
 * @see #ocs_object_parse_field
 */
template <auto F>
   requires OcsAttrValueField<F>
const char *
ocs_object_append_field(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep, dstring *buffer) {
   DENTER(OBJECT_LAYER);

   using ocs::tom::FieldType;
   constexpr FieldType type = ocs::tom::FieldTraits<F>::type;
   constexpr ocs_object_symbolic kind = ocs_object_symbolic_of(static_cast<int>(F));
   constexpr int nm = static_cast<int>(F);

   const char *ret = nullptr;
   if (ep) {
      if constexpr (kind != ocs_object_symbolic::none) {
         ret = ocs_object_append_symbolic(buffer, kind, ocs::tom::get<F>(ep));
      } else if constexpr (nm == ASTRLIST_value) {
         ret = str_list_append_to_dstring(ocs::tom::get<F>(ep), buffer, ' ');
      } else if constexpr (nm == AUSRLIST_value) {
         ret = userset_list_append_to_dstring(ocs::tom::get<F>(ep), buffer);
      } else if constexpr (nm == APRJLIST_value) {
         ret = prj_list_append_to_dstring(ocs::tom::get<F>(ep), buffer);
      } else if constexpr (nm == ACELIST_value) {
         ret = centry_list_append_to_dstring(ocs::tom::get<F>(ep), buffer);
      } else if constexpr (nm == ASOLIST_value) {
         ret = so_list_append_to_dstring(ocs::tom::get<F>(ep), buffer);
      } else if constexpr (type == FieldType::Uint32) {
         ret = sge_dstring_sprintf_append(buffer, sge_u32, ocs::tom::get<F>(ep));
      } else if constexpr (type == FieldType::Uint64) {
         ret = sge_dstring_sprintf_append(buffer, sge_u64, ocs::tom::get<F>(ep));
      } else if constexpr (type == FieldType::Int32) {
         ret = sge_dstring_sprintf_append(buffer, "%d", ocs::tom::get<F>(ep));
      } else if constexpr (type == FieldType::Int64) {
         ret = sge_dstring_sprintf_append(buffer, "%ld", static_cast<long>(ocs::tom::get<F>(ep)));
      } else if constexpr (type == FieldType::Double) {
         ret = sge_dstring_sprintf_append(buffer, "%lf", ocs::tom::get<F>(ep));
      } else if constexpr (type == FieldType::Bool) {
         ret = sge_dstring_append(buffer, ocs::tom::get<F>(ep) ? TRUE_STR : FALSE_STR);
      } else {
         static_assert(type == FieldType::String, "a field of the cluster queue attributes has no other type");
         const char *str = ocs::tom::get<F>(ep);
         ret = sge_dstring_append(buffer, str != nullptr ? str : NONE_STR);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Set the value of a field from the text an administrator writes
 *
 * The counterpart of ocs_object_append_field(): a field of a #ocs_object_symbolic kind is parsed by
 * ocs_object_parse_symbolic(), a time, interval or memory attribute is validated and stored verbatim, a list of
 * the cluster queue attributes is parsed into its elements, anything else by the parser of its type.
 *
 * @tparam F the field
 * @param ep the element to change
 * @param[out] answer_list receives the messages
 * @param value the text
 * @return true on success, false on an error, described in the answer list for most fields
 *
 * @note MT-NOTE: ocs_object_parse_field() is not MT safe for the bit fields, as sge_parse_bitfield_str() is not
 *
 * @see #ocs_object_append_field
 */
template <auto F>
   requires OcsAttrValueField<F>
bool
ocs_object_parse_field(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> ep,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value) {
   DENTER(OBJECT_LAYER);

   using ocs::tom::FieldType;
   constexpr FieldType type = ocs::tom::FieldTraits<F>::type;
   constexpr ocs_object_symbolic kind = ocs_object_symbolic_of(static_cast<int>(F));
   constexpr int nm = static_cast<int>(F);

   if (!ep) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_NULLELEMENTPASSEDTO_S,
                              __func__);
      DRETURN(false);
   }

   bool ret = true;
   if constexpr (kind != ocs_object_symbolic::none) {
      const ocs_object_parsed_symbol parsed = ocs_object_parse_symbolic(answer_list, kind, value);
      if (parsed.store) {
         ocs::tom::set<F>(ep, parsed.value);
      }
      ret = parsed.ok;
   } else if constexpr (nm == ATIME_value || nm == AINTER_value || nm == AMEM_value) {
      constexpr ocs::CEntry::Type value_type = nm == AMEM_value ? ocs::CEntry::Type::MEM : ocs::CEntry::Type::TIME;
      ret = ocs_object_check_value_string(answer_list, value_type, value);
      if (ret) {
         ocs::tom::set<F>(ep, value);
      }
   } else if constexpr (nm == ASTRLIST_value) {
      ret = parse_name_list<F, ST_name>(ep, answer_list, value);
   } else if constexpr (nm == AUSRLIST_value) {
      ret = parse_name_list<F, US_name>(ep, answer_list, value);
   } else if constexpr (nm == APRJLIST_value) {
      ret = parse_name_list<F, PR_name>(ep, answer_list, value);
   } else if constexpr (nm == ACELIST_value) {
      ret = parse_celist(ep, answer_list, value);
   } else if constexpr (nm == ASOLIST_value) {
      ret = parse_solist(ep, answer_list, value);
   } else if constexpr (type == FieldType::String) {
      ocs::tom::set<F>(ep, value);
   } else {
      std::optional<typename ocs::tom::ValueOf<type>::type> parsed;
      if constexpr (type == FieldType::Uint32) {
         parsed = ocs_object_parse_uint32(answer_list, value);
      } else if constexpr (type == FieldType::Uint64) {
         parsed = ocs_object_parse_uint64(answer_list, value);
      } else if constexpr (type == FieldType::Int32) {
         parsed = ocs_object_parse_int32(answer_list, value);
      } else if constexpr (type == FieldType::Int64) {
         parsed = ocs_object_parse_int64(answer_list, value);
      } else if constexpr (type == FieldType::Double) {
         parsed = ocs_object_parse_double(answer_list, value);
      } else {
         static_assert(type == FieldType::Bool, "a field of the cluster queue attributes has no other type");
         parsed = ocs_object_parse_bool(answer_list, value);
      }
      if (parsed) {
         ocs::tom::set<F>(ep, *parsed);
      } else {
         ret = false;
      }
   }
   DRETURN(ret);
}

/// @cond
template const char *ocs_object_append_field<ASTR_value>(ocs::tom::ConstElement<ocs::tom::ASTR>, dstring *);
template const char *ocs_object_append_field<AULNG_value>(ocs::tom::ConstElement<ocs::tom::AULNG>, dstring *);
template const char *ocs_object_append_field<ABOOL_value>(ocs::tom::ConstElement<ocs::tom::ABOOL>, dstring *);
template const char *ocs_object_append_field<ATIME_value>(ocs::tom::ConstElement<ocs::tom::ATIME>, dstring *);
template const char *ocs_object_append_field<AMEM_value>(ocs::tom::ConstElement<ocs::tom::AMEM>, dstring *);
template const char *ocs_object_append_field<AINTER_value>(ocs::tom::ConstElement<ocs::tom::AINTER>, dstring *);
template const char *ocs_object_append_field<AQTLIST_value>(ocs::tom::ConstElement<ocs::tom::AQTLIST>, dstring *);
template const char *ocs_object_append_field<ASTRLIST_value>(ocs::tom::ConstElement<ocs::tom::ASTRLIST>,
                                                             dstring *);
template const char *ocs_object_append_field<AUSRLIST_value>(ocs::tom::ConstElement<ocs::tom::AUSRLIST>,
                                                             dstring *);
template const char *ocs_object_append_field<APRJLIST_value>(ocs::tom::ConstElement<ocs::tom::APRJLIST>,
                                                             dstring *);
template const char *ocs_object_append_field<ACELIST_value>(ocs::tom::ConstElement<ocs::tom::ACELIST>, dstring *);
template const char *ocs_object_append_field<ASOLIST_value>(ocs::tom::ConstElement<ocs::tom::ASOLIST>, dstring *);

template bool ocs_object_parse_field<ASTR_value>(ocs::tom::Element<ocs::tom::ASTR>,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<AULNG_value>(ocs::tom::Element<ocs::tom::AULNG>,
                                                  ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<ABOOL_value>(ocs::tom::Element<ocs::tom::ABOOL>,
                                                  ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<ATIME_value>(ocs::tom::Element<ocs::tom::ATIME>,
                                                  ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<AMEM_value>(ocs::tom::Element<ocs::tom::AMEM>,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<AINTER_value>(ocs::tom::Element<ocs::tom::AINTER>,
                                                   ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<AQTLIST_value>(ocs::tom::Element<ocs::tom::AQTLIST>,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<ASTRLIST_value>(ocs::tom::Element<ocs::tom::ASTRLIST>,
                                                     ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<AUSRLIST_value>(ocs::tom::Element<ocs::tom::AUSRLIST>,
                                                     ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<APRJLIST_value>(ocs::tom::Element<ocs::tom::APRJLIST>,
                                                     ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<ACELIST_value>(ocs::tom::Element<ocs::tom::ACELIST>,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
template bool ocs_object_parse_field<ASOLIST_value>(ocs::tom::Element<ocs::tom::ASOLIST>,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *, const char *);
/// @endcond

/**
 * @brief Has an object a certain type?
 *
 * Checks if an object has a certain type.
 * The check is done by looking up the primary key field in descr and
 * checking, if this key field is contained in the given object.
 *
 * @param object object to check
 * @param descr type to check against
 *
 * @return true, if the object has the given type, else false
 *
 * @note As looking up the primary key of an object is only implemented
 *       for selected types, this function also will work only for these
 *       object types.
 *
 * @see #object_get_primary_key
 */
bool
object_has_type(const lListElem *object, const lDescr *descr) {
   bool ret = false;

   /*
    * we assume that "object" is of the given type when the 
    * primary key is contained in the element
    *
    * --> make sure that your object is handled in object_get_primary_key() 
    */
   if (object != nullptr && descr != nullptr &&
       _lGetPosInDescr(object->descr, object_get_primary_key(descr)) != -1) {
      ret = true;
   }

   return ret;
}

/**
 * @brief Return type (descriptor) for object
 *
 * Returns the cull type (descriptor) for a certain object.
 * This descriptor can be different from the objects descriptor,
 * as the objects descriptor can come from an object created from
 * communication, can be a partial descriptor etc.
 *
 * @param object the object to analyze
 *
 * @return the object type / descriptor
 */
const lDescr *
object_get_type(const lListElem *object) {
   const lDescr *ret = nullptr;

   if (object_has_type(object, CAL_Type)) {
      ret = CAL_Type;
   } else if (object_has_type(object, CK_Type)) {
      ret = CK_Type;
   } else if (object_has_type(object, EH_Type)) {
      ret = EH_Type;
   } else if (object_has_type(object, JAT_Type)) {
      ret = JAT_Type;
   } else if (object_has_type(object, JB_Type)) {
      ret = JB_Type;
   } else if (object_has_type(object, PE_Type)) {
      ret = PE_Type;
   } else if (object_has_type(object, PET_Type)) {
      ret = PET_Type;
   } else if (object_has_type(object, QU_Type)) {
      ret = QU_Type;
   } else if (object_has_type(object, QR_Type)) {
      ret = QR_Type;
   } else if (object_has_type(object, RN_Type)) {
      ret = RN_Type;
   } else if (object_has_type(object, VA_Type)) {
      ret = VA_Type;
   }

   return ret;
}

/**
 * @brief Get primary key for object type
 *
 * Returns the primary key field for a given object type.
 *
 * @param descr the object type (descriptor)
 *
 * @return name (nm) of the primary key field or NoName (-1) on error.
 *
 * @note Only implemented for selected object types.
 *       It would be better to have the necessary data in the object
 *       description, e.g. via a field property CULL_PRIMARY_KEY.
 *
 *       Function interface breaks style guide - either we would have to
 *       pass an object, or call it descr_get_primary_key.
 */
int
object_get_primary_key(const lDescr *descr) {
   int ret = NoName;

   if (descr != nullptr) {
      for (const auto f : ocs_cull_present_fields(descr)) {
         if (descr[f.pos].mt & CULL_PRIMARY_KEY) {
            ret = descr[f.pos].nm;
            break;
         }
      }
   }

#if 0
   ret = MR_user; /* JG: TODO: is this really the primary key? */
   ret = PA_origin; /* JG: TODO: this is most probably no primary key! */
#endif

   return ret;
}

/**
 * @brief Get prefix of cull attribute name
 *
 * Returns the prefix that is used in attribute names characterizing
 * the object type (e.g. "QU_" for the QU_Type).
 *
 * @code
 * object_get_name_prefix(QU_Type, buffer) = "QU_"
 * object_get_name_prefix(JB_Type, buffer) = "JB_"
 * @endcode
 *
 * @param descr object type to use
 * @param buffer buffer that is used to return the result
 *
 * @return the prefix or nullptr, if an error occurred
 *
 * @note The function relies on object_get_primary_key. This function only
 *       is implemented for some object types.
 *       For types not handled in object_get_primary_key, nullptr will be
 *       returned.
 *
 * @see #object_get_primary_key
 */
const char *
object_get_name_prefix(const lDescr *descr, dstring *buffer) {
   int nm;

   if (descr == nullptr || buffer == nullptr) {
      return nullptr;
   }

   nm = descr[0].nm;

   if (nm != NoName) {
      const char *name = lNm2Str(nm);

      if (name != nullptr) {
         char *underscore = strchr((char *) name, '_');

         if (underscore != nullptr) {
            sge_dstring_sprintf(buffer, "%.*s", underscore - name + 1, name);
            return sge_dstring_get_string(buffer);
         }
      }
   }

   return nullptr;
}

/**
 * @brief Get the object name by object descriptor
 *
 * Returns the object name for a given descriptor.
 * If the descriptor doesn't match a descriptor in object_base,
 * return "unknown".
 *
 * @code
 * object_get_name(JOB_Type) returns "JOB"
 * @endcode
 *
 * @param descr descriptor.
 *
 * @return the object name
 *
 * @note MT-NOTE: object_get_name() is MT safe
 */
const char *
object_get_name(const lDescr *descr) {
   const char *name = "unknown";

   if (descr != nullptr) {
      int i;

      for (i = SGE_TYPE_FIRST; i < SGE_TYPE_ALL; i++) {
         if (object_base[i].descr == descr) {
            name = object_base[i].type_name;
            break;
         }
      }
   }

   return name;
}

/**
 * @brief Type name of an object, identified by content.
 *
 * Returns the registered type name (e.g. "MANAGER", "USERSET", "CQUEUE") of @p object
 * by scanning object_base[] for the type whose primary key attribute is present in the
 * object's descriptor. Unlike object_get_name(object_get_type()), this covers every
 * registered type and works on GDI-transported elements (whose descriptor is a rebuilt
 * copy, so a pointer compare would fail). Types without a primary key (e.g. the
 * scheduler configuration) cannot be identified this way and yield "unknown".
 *
 * @param object  the object to identify
 * @return the registered type name, or "unknown"
 */
const char *
object_get_type_name(const lListElem *object) {
   const char *name = "unknown";

   if (object != nullptr) {
      const lDescr *descr = lGetElemDescr(object);

      for (int i = SGE_TYPE_FIRST; i < SGE_TYPE_ALL; i++) {
         if (object_base[i].descr != nullptr && object_base[i].key_nm != NoName &&
             lGetPosInDescr(descr, object_base[i].key_nm) >= 0) {
            name = object_base[i].type_name;
            break;
         }
      }
   }

   return name;
}

/**
 * @brief Del certain id from an range_list
 *
 * Deletes a certain id from an objects sublist that is a range list. The list
 * is taken out of the field, changed and put back.
 *
 * @tparam F the range list field (RN_Type) of the object's type
 * @param object the object to handle
 * @param answer_list error messages will be put here
 * @param id id to delete
 */
template <auto F>
   requires ocs::tom::FieldTraits<F>::has_sub &&
            std::is_same_v<typename ocs::tom::FieldTraits<F>::Sub, ocs::tom::RN>
void
object_delete_range_id(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> object,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const uint32_t id) {
   auto range_list = object.template exchange<F>(nullptr);
   range_list_remove_id(range_list, answer_list, id);
   range_list_compress(range_list);
   static_cast<void>(object.template exchange<F>(std::move(range_list)));
}

/// @cond explicit instantiations for the fields in use
template void object_delete_range_id<JB_ja_n_h_ids>(ocs::tom::Element<ocs::tom::JB> object,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t id);
template void object_delete_range_id<JB_ja_u_h_ids>(ocs::tom::Element<ocs::tom::JB> object,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t id);
template void object_delete_range_id<JB_ja_o_h_ids>(ocs::tom::Element<ocs::tom::JB> object,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t id);
template void object_delete_range_id<JB_ja_s_h_ids>(ocs::tom::Element<ocs::tom::JB> object,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t id);
template void object_delete_range_id<JB_ja_a_h_ids>(ocs::tom::Element<ocs::tom::JB> object,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t id);
template void object_delete_range_id<JJ_not_yet_finished_ids>(ocs::tom::Element<ocs::tom::JJ> object,
                                                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                              uint32_t id);
/// @endcond

/**
 * @brief Store the initial range ids in "job"
 *
 * The function stores the initial range id values ("start", "end"
 * and "step") in the range list of an object. It should only be used
 * in functions initializing objects range lists. The first range of the
 * list is overwritten; an object without a range list gets a new list
 * holding one range.
 *
 * @tparam F the range list field (RN_Type) of the object's type
 * @param object object to handle
 * @param start first id
 * @param end last id
 * @param step step size
 *
 * @return 0; the former result 1 for a lack of memory is gone, the handles throw then
 *
 * @note MT-NOTE: object_set_range_id() is MT safe
 */
template <auto F>
   requires ocs::tom::FieldTraits<F>::has_sub &&
            std::is_same_v<typename ocs::tom::FieldTraits<F>::Sub, ocs::tom::RN>
int
object_set_range_id(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> object, const uint32_t start,
                    const uint32_t end, const uint32_t step) {
   DENTER(OBJECT_LAYER);

   auto range_elem = ocs::tom::get<F>(object).front();
   if (!range_elem) {
      auto range_list = ocs::tom::ListPtr<ocs::tom::RN>::create("task_id_range");
      range_elem = range_list->push_back(ocs::tom::ElementPtr<ocs::tom::RN>::create());
      ocs::tom::set<F>(object, std::move(range_list));
   }
   ocs::tom::set<RN_min>(range_elem, start);
   ocs::tom::set<RN_max>(range_elem, end);
   ocs::tom::set<RN_step>(range_elem, step);
   DRETURN(0);
}

/// @cond explicit instantiations for the fields in use
template int object_set_range_id<JB_ja_structure>(ocs::tom::Element<ocs::tom::JB> object, uint32_t start,
                                                  uint32_t end, uint32_t step);
template int object_set_range_id<JJ_not_yet_finished_ids>(ocs::tom::Element<ocs::tom::JJ> object, uint32_t start,
                                                          uint32_t end, uint32_t step);
/// @endcond


/**
 * @brief Get a printable name for event type
 *
 * Returns a printable name for an event type.
 *
 * @code
 * object_type_get_name(SGE_TYPE_JOB) will return "JOB"
 * @endcode
 *
 * @param type the event type
 *
 * @return string describing the type
 *
 * @see #object_type_get_descr, #object_type_get_key_nm
 */
const char *object_type_get_name(sge_object_type type) {
   DENTER(OBJECT_LAYER);

   const char *ret = "unknown";

   if (/* type >= SGE_TYPE_FIRST && */ type < SGE_TYPE_ALL) {
      ret = object_base[type].type_name;
   } else if (type == SGE_TYPE_ALL) {
      ret = "default";
   } else {
      ERROR(MSG_OBJECT_INVALID_OBJECT_TYPE_SI, __func__, type);
   }

   DRETURN(ret);
}

/**
 * @brief Return type id of a certain object
 *
 * returns the type id a an object given by "name"
 * We allow to pass in names in the form `object_name`:`key`, e.g.
 * USERSET:deadlineusers.
 *
 * @param name object name
 *
 * @return type of the object
 *
 * @note MT-NOTE: object_name_get_type() is MT safe
 */
sge_object_type object_name_get_type(const char *name) {
   DENTER(OBJECT_LAYER);

   sge_object_type ret = SGE_TYPE_ALL;
   int i;
   char *type_name;
   char *colon;

   type_name = strdup(name);
   colon = strchr(type_name, ':');
   if (colon != nullptr) {
      *colon = '\0';
   }

   for (i = (int) SGE_TYPE_FIRST; i < (int) SGE_TYPE_ALL; i++) {
      if (strcasecmp(object_base[i].type_name, type_name) == 0) {
         ret = (sge_object_type) i;
         break;
      }
   }

   sge_free(&type_name);
   DRETURN(ret);
}

/**
 * @brief Get the descriptor for an event type
 *
 * Returns the CULL element descriptor for the object type
 * associated with the given event.
 *
 * @code
 * object_type_get_descr(SGE_TYPE_JOB) will return the descriptor
 *    JB_Type,
 * object_type_get_descr(SGE_TYPE_SHUTDOWN) will return nullptr
 * @endcode
 *
 * @param type the event type
 *
 * @return the descriptor, or nullptr, if no descriptor is associated with the type
 *
 * @see #object_type_get_name, #object_type_get_key_nm
 */
const lDescr *object_type_get_descr(sge_object_type type) {
   DENTER(OBJECT_LAYER);

   const lDescr *ret = nullptr;

   if (/* type >= 0 && */ type < SGE_TYPE_ALL) {
      ret = object_base[type].descr;
   } else {
      ERROR(MSG_OBJECT_INVALID_OBJECT_TYPE_SI, __func__, type);
   }

   DRETURN(ret);
}

/**
 * @brief Get the primary key attribute for type
 *
 * Returns the primary key attribute for the object type associated
 * with the given event type.
 *
 * @code
 * object_type_get_key_nm(SGE_TYPE_JOB) will return JB_job_number
 * object_type_get_key_nm(SGE_TYPE_SHUTDOWN) will return -1
 * @endcode
 *
 * @param type event type
 *
 * @return the key number (struct element nm of the descriptor), or -1, if no object type is associated with the
 *         event type
 *
 * @see #object_type_get_name, #object_type_get_descr
 */
int object_type_get_key_nm(sge_object_type type) {
   DENTER(OBJECT_LAYER);

   int ret = NoName;

   if (/* type >= 0 && */ type < SGE_TYPE_ALL) {
      ret = object_base[type].key_nm;
   } else {
      ERROR(MSG_OBJECT_INVALID_OBJECT_TYPE_SI, __func__, type);
   }

   DRETURN(ret);
}

/**
 * @brief Do two strings differ, with nullptr as a value of its own?
 * @param a the first string, or nullptr
 * @param b the second string, or nullptr
 * @return true if exactly one is nullptr, or both are set and strcmp() finds them different
 */
static bool
object_strings_differ(const char *a, const char *b) {
   if (a == nullptr || b == nullptr) {
      return a != b;
   }
   return a != b && strcmp(a, b) != 0;
}

/**
 * @brief Do two elements differ in a field both hold with the same type?
 *
 * Reads both values through the checked run-time accessors. The caller has
 * checked that both elements hold the field with this type, so no read throws.
 *
 * @param this_elem the element to compare
 * @param[out] answer_list receives error messages of nested comparisons
 * @param old_elem the element to compare against
 * @param nm the field number
 * @param type the field type both elements hold the field with
 * @return true if the values differ; strings compare with strcmp(), host names as sge_hostcmp() does,
 *         objects and lists element by element
 */
static bool
object_field_differs(const ocs::tom::ConstRawElement this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                     const ocs::tom::ConstRawElement old_elem, const int nm, const ocs::tom::FieldType type) {
   switch (type) {
      case ocs::tom::FieldType::Uint32:
         return this_elem.get<std::uint32_t>(nm) != old_elem.get<std::uint32_t>(nm);
      case ocs::tom::FieldType::Uint64:
         return this_elem.get<std::uint64_t>(nm) != old_elem.get<std::uint64_t>(nm);
      case ocs::tom::FieldType::Int32:
         return this_elem.get<std::int32_t>(nm) != old_elem.get<std::int32_t>(nm);
      case ocs::tom::FieldType::Int64:
         return this_elem.get<std::int64_t>(nm) != old_elem.get<std::int64_t>(nm);
      case ocs::tom::FieldType::Double:
         return this_elem.get<double>(nm) != old_elem.get<double>(nm);
      case ocs::tom::FieldType::Bool:
         return this_elem.get<bool>(nm) != old_elem.get<bool>(nm);
      case ocs::tom::FieldType::String:
         return object_strings_differ(this_elem.get<const char *>(nm), old_elem.get<const char *>(nm));
      case ocs::tom::FieldType::Host:
         return this_elem.get<ocs::tom::Host>(nm) != old_elem.get<ocs::tom::Host>(nm);
      case ocs::tom::FieldType::Ref:
         return this_elem.get<void *>(nm) != old_elem.get<void *>(nm);
      case ocs::tom::FieldType::Object:
         return object_has_differences(this_elem.get<ocs::tom::ConstRawElement>(nm), answer_list,
                                       old_elem.get<ocs::tom::ConstRawElement>(nm));
      case ocs::tom::FieldType::List:
         return object_list_has_differences(this_elem.get<ocs::tom::ConstRawList>(nm), answer_list,
                                            old_elem.get<ocs::tom::ConstRawList>(nm));
   }
   return true;
}

/**
 * @brief Do two elements differ in any attribute?
 *
 * Walks the fields @p this_elem holds and compares each with the same field of
 * @p old_elem. Two elements count as different rather than as an error when one
 * holds a field the other does not hold, or holds it with another type, as two
 * differently reduced elements do. Every field type is compared, the 64 bit
 * numbers included.
 *
 * @param this_elem the element to compare, maybe empty
 * @param[out] answer_list receives error messages
 * @param old_elem the element to compare against, maybe empty
 * @return true when the elements differ, or when only one of them is empty
 */
bool
object_has_differences(const ocs::tom::ConstRawElement this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       const ocs::tom::ConstRawElement old_elem) {
   DENTER(TOP_LAYER);

   if (!this_elem || !old_elem) {
      // one of both elements is not nullptr
      DRETURN(static_cast<bool>(this_elem) != static_cast<bool>(old_elem));
   }

   const lDescr *const this_descr = this_elem.descr();
   const lDescr *const old_descr = old_elem.descr();
   bool ret = false;
   int this_count = 0;

   for (const auto f : ocs_cull_present_fields(this_descr)) {
      const int nm = this_descr[f.pos].nm;
      const ocs::tom::FieldType type = ocs::tom::field_type_of(this_descr[f.pos].mt);
      const int old_pos = ocs::tom::field_pos(old_descr, nm);
      this_count++;

      // the same field with the same type, or the elements differ in their layout
      if (!old_elem.has(nm) || ocs::tom::field_type_of(old_descr[old_pos].mt) != type) {
         DPRINTF("Attribute " SFQ " cannot be compared, the other element does not hold it with the same type\n",
                 ocs::tom::field_name(nm));
         ret = true;
         break;
      }

      if (object_field_differs(this_elem, answer_list, old_elem, nm, type)) {
         DPRINTF("Attributes " SFQ " are not equivalent\n", ocs::tom::field_name(nm));
         ret = true;
      }
   }

   // every field of this_elem is held by old_elem: they differ if old_elem holds more
   if (!ret) {
      int old_count = 0;
      for ([[maybe_unused]] const auto f : ocs_cull_present_fields(old_descr)) {
         old_count++;
      }
      if (old_count != this_count) {
         DPRINTF("Descriptor size is not equivalent\n");
         ret = true;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Do two lists differ in any element?
 *
 * @param this_list the list to compare, maybe empty (nullptr)
 * @param[out] answer_list receives error messages
 * @param old_list the list to compare against, maybe empty (nullptr)
 * @return true when the lists differ in length or in any element; a nullptr list and an empty list do not differ
 *
 * @see #object_has_differences
 */
bool
object_list_has_differences(const ocs::tom::ConstRawList this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            const ocs::tom::ConstRawList old_list) {
   DENTER(BASIS_LAYER);

   if (this_list.size() != old_list.size()) {
      DRETURN(true);
   }

   // equal length; an empty handle iterates nothing
   auto old_it = old_list.begin();
   for (const auto this_elem : this_list) {
      if (object_has_differences(this_elem, answer_list, *old_it)) {
         DRETURN(true);
      }
      ++old_it;
   }

   DRETURN(false);
}

/**
 * @brief Verify cull list structure
 *
 * Verifies that a cull list, including all its objects and sublists,
 * is a valid object of a defined type.
 *
 * There are cases, where the type of a list cannot be verified, when the
 * type of a sublist is set to CULL_ANY_SUBTYPE in the cull list definition.
 * In this case, the list descriptor is not checked, but still all list
 * objects and there sublists are verified.
 *
 * @param lp list to verify
 * @param descr expected descriptor (or nullptr, see above)
 *
 * @return true, if the object is OK, else false
 *
 * @note MT-NOTE: object_list_verify_cull() is MT safe
 *
 * @see #object_verify_cull, `lCompListDescr()`
 */
bool
object_list_verify_cull(const lList *lp, const lDescr *descr) {
   bool ret = true;

   /* 
    * valid input parameters?
    * descr may be nullptr, if sublist is defined as CULL_ANY_SUBTYPE
    */
   if (lp == nullptr) {
      ret = false;
   }

   /* does list descriptor match expected descriptor? */
   if (ret) {
      if (descr != nullptr) {
         if (lCompListDescr(lp->descr, descr) != 0) {
            ret = false;
         }
      }
   }

   /* recursively check sublists and subobjects */
   if (ret) {
      const lListElem *ep;
      for_each_ep(ep, lp) {
         if (!object_verify_cull(ep, nullptr)) {
            ret = false;
            break;
         }
      }
   }

   return ret;
}

/**
 * @brief Verify cull object structure
 *
 * Verifies that a cull object, including all its sublists and subobjects,
 * is a valid object of a defined type.
 *
 * The type (descr) argument may be zero in two cases:
 *    - the descriptor has already been checked. This is the case when called
 *      from object_list_verify_cull().
 *    - we don't know the type of a subobject (if it is defined as
 *      CULL_ANY_SUBTYPE in the cull object definition
 *
 * @param ep the object to verify
 * @param descr expected object type, or nullptr (see above)
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: object_verify_cull() is MT safe
 *
 * @see #object_verify_cull, `lCompListDescr()`
 */
bool
object_verify_cull(const lListElem *ep, const lDescr *descr) {
   bool ret = true;

   /* 
    * valid input parameters?
    * descr may be nullptr, if subobject is defined as CULL_ANY_SUBTYPE
    */
   if (ep == nullptr) {
      ret = false;
   }

   /* does object descriptor match expected descriptor? */
   if (ret) {
      if (descr != nullptr) {
         if (lCompListDescr(ep->descr, descr) != 0) {
            ret = false;
         }
      }
   }

   /* check contents of subobject */
   if (ret) {
      for (const auto f : ocs_cull_present_fields(ep->descr)) {
         const int i = f.pos;
         int type = mt_get_type(ep->descr[i].mt);
         if (type == lListT) {
            const lList *lp = lGetList(ep, ep->descr[i].nm);
            if (lp != nullptr) {
               const lDescr *subdescr = object_get_subtype(ep->descr[i].nm);
               if (!object_list_verify_cull(lp, subdescr)) {
                  ret = false;
                  break;
               }
            }
         } else if (type == lObjectT) {
            lListElem *sub_ep = lGetObject(ep, ep->descr[i].nm);
            if (sub_ep != nullptr) {
               const lDescr *subdescr = object_get_subtype(ep->descr[i].nm);
               if (!object_verify_cull(sub_ep, subdescr)) {
                  ret = false;
                  break;
               }
            }
         }
      }
   }

   return ret;
}

/**
 * @brief Verify that a list received from a client is of the expected type, recursively (CS-3125)
 *
 * Unlike object_list_verify_cull(), which requires the full descriptor of
 * the type, a reduced descriptor of the type is accepted: request data is
 * often reduced (qalter, qconf -mattr, the bridge). Checked are the type of
 * the list, that every element has the layout of the list, and, through
 * ocs_object_verify_type(), the sublists and subobjects of every element.
 *
 * The check reads static information only (descriptors and the subtypes of
 * the fields), so it needs no lock.
 *
 * @param lp the list, or nullptr (nothing to verify)
 * @param descr the descriptor of the expected type, or nullptr when the type of the list is not fixed
 * @return true if the list and everything below it are of their expected types
 */
bool
ocs_object_list_verify_type(const lList *lp, const lDescr *descr) {
   if (lp == nullptr) {
      return true;
   }
   const lDescr *list_descr = lGetListDescr(lp);
   if (descr != nullptr && !ocs::tom::same_type(list_descr, descr)) {
      return false;
   }
   for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
      if (!ocs::tom::same_layout(lGetElemDescr(ep), list_descr) || !ocs_object_verify_type(ep, nullptr)) {
         return false;
      }
   }
   return true;
}

/**
 * @brief Verify that an element received from a client is of the expected type, recursively (CS-3125)
 *
 * The element's descriptor must describe the expected type, possibly
 * reduced; every sublist and subobject it holds must be of the subtype its
 * field declares (a field without a declared subtype is checked below its
 * own level only).
 *
 * @param ep the element, or nullptr (nothing to verify)
 * @param descr the descriptor of the expected type, or nullptr when the type is not fixed
 * @return true if the element and everything below it are of their expected types
 */
bool
ocs_object_verify_type(const lListElem *ep, const lDescr *descr) {
   if (ep == nullptr) {
      return true;
   }
   const lDescr *elem_descr = lGetElemDescr(ep);
   if (descr != nullptr && !ocs::tom::same_type(elem_descr, descr)) {
      return false;
   }
   for (const auto f : ocs_cull_present_fields(elem_descr)) {
      const int nm = elem_descr[f.pos].nm;
      const int type = mt_get_type(elem_descr[f.pos].mt);
      if (type == lListT) {
         if (!ocs_object_list_verify_type(lGetPosList(ep, f.pos), object_get_subtype(nm))) {
            return false;
         }
      } else if (type == lObjectT) {
         if (!ocs_object_verify_type(lGetPosObject(ep, f.pos), object_get_subtype(nm))) {
            return false;
         }
      }
   }
   return true;
}

namespace {
   /**
    * @brief The key of a unique attribute in the form the hash table of the list compares
    *
    * Host names are normalised as `cull_hash_key()` does it (`sge_hostcpy()`, upper case), so two names that
    * `sge_hostcmp()` takes for the same host give the same key; strings are taken as they are.
    *
    * @param value the attribute's value, not nullptr
    * @param is_host whether the attribute is a host name
    * @return the key
    */
   std::string
   comparable_key(const char *value, const bool is_host) {
      if (!is_host) {
         return value;
      }
      char host_key[CL_MAXHOSTNAMELEN + 1];
      sge_hostcpy(host_key, value);
      sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
      return host_key;
   }

   /**
    * @brief The path of a sub-list, its parent's path and the attribute holding it
    * @param path the parent's path, empty at the object itself
    * @param nm the list attribute
    * @return the path, attribute names separated by '/'
    */
   std::string
   sub_path(const std::string &path, const int nm) {
      const char *name = lNm2Str(nm);
      return path.empty() ? std::string(name) : path + "/" + name;
   }

   void find_in_list(const lList *lp, const std::string &path, std::vector<ocs::RepeatedKey> &found);

   /**
    * @brief Collect the repeated keys in the sub-lists and sub-objects of an object
    * @param ep the object, nullptr is accepted
    * @param path the path of the list holding the object, empty for the object itself
    * @param found receives every repeated key
    */
   void
   find_in_object(const lListElem *ep, const std::string &path, std::vector<ocs::RepeatedKey> &found) {
      if (ep == nullptr) {
         return;
      }
      for (const auto f : ocs_cull_present_fields(ep->descr)) {
         const int nm = ep->descr[f.pos].nm;
         const int type = mt_get_type(ep->descr[f.pos].mt);
         if (type == lListT) {
            find_in_list(lGetList(ep, nm), sub_path(path, nm), found);
         } else if (type == lObjectT) {
            find_in_object(lGetObject(ep, nm), sub_path(path, nm), found);
         }
      }
   }

   /**
    * @brief Collect the repeated keys of a list's unique attributes, then those of its elements
    * @param lp the list, nullptr is accepted
    * @param path the path of the list
    * @param found receives every repeated key; each repetition of a key is one entry
    */
   void
   find_in_list(const lList *lp, const std::string &path, std::vector<ocs::RepeatedKey> &found) {
      if (lp == nullptr || lGetNumberOfElem(lp) == 0) {
         return;
      }

      const lDescr *dp = lGetListDescr(lp);
      for (const auto f : ocs_cull_present_fields(dp)) {
         if (!mt_is_unique(dp[f.pos].mt)) {
            continue;
         }
         const int nm = dp[f.pos].nm;
         const int type = mt_get_type(dp[f.pos].mt);

         if (type == lStringT || type == lHostT) {
            std::unordered_set<std::string> seen;
            for_each_ep_lv(ep, lp) {
               const char *key = type == lStringT ? lGetString(ep, nm) : lGetHost(ep, nm);
               if (key != nullptr && !seen.insert(comparable_key(key, type == lHostT)).second) {
                  found.push_back({lp, nm, key, path});
               }
            }
         } else if (type == lUlongT) {
            std::unordered_set<uint32_t> seen;
            for_each_ep_lv(ep, lp) {
               if (const uint32_t key = lGetUlong(ep, nm); !seen.insert(key).second) {
                  found.push_back({lp, nm, std::to_string(key), path});
               }
            }
         } else if (type == lUlong64T) {
            std::unordered_set<uint64_t> seen;
            for_each_ep_lv(ep, lp) {
               if (const uint64_t key = lGetUlong64(ep, nm); !seen.insert(key).second) {
                  found.push_back({lp, nm, std::to_string(key), path});
               }
            }
         }
      }

      for_each_ep_lv(ep, lp) {
         find_in_object(ep, path, found);
      }
   }

   /**
    * @brief Add the error for a repeated key to an answer list
    * @param answer_list the answer list
    * @param repeated the repeated key
    * @param object_name the object named in the error
    */
   void
   add_repeated_key_error(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::RepeatedKey &repeated,
                          const char *object_name) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJ_DUPLICATEKEY_SSS,
                              repeated.key.c_str(), lNm2Str(repeated.nm), object_name != nullptr ? object_name : "");
   }
} // namespace

/**
 * @brief Find every key that a unique attribute of an object's sub-lists holds twice
 *
 * Walks every sub-list and sub-object of the object. In each list, every attribute flagged #CULL_UNIQUE must
 * hold a key at most once. String keys compare by exact string, host keys as the hash table of the list
 * compares them, normalised as `sge_hostcmp()` does, and ulong keys by value. Elements without a key are
 * skipped. The lists of a list are checked before the sub-lists of its elements.
 *
 * @param ep the object to check, nullptr is accepted
 * @param found receives one entry for each repetition of a key, with the list, the attribute, the key as the
 *        repeating element holds it and the path of attribute names from the object to the list
 *
 * @note MT-NOTE: ocs_object_find_repeated_keys() is MT safe
 *
 * @see #ocs_object_verify_unique_keys, #ocs_object_repair_repeated_keys
 */
void
ocs_object_find_repeated_keys(const lListElem *ep, std::vector<ocs::RepeatedKey> &found) {
   find_in_object(ep, "", found);
}

/**
 * @brief Check that no unique attribute of an object holds the same key twice
 *
 * Walks every sub-list and sub-object of the object as #ocs_object_find_repeated_keys does and reports the
 * first repeated key.
 *
 * Input from clients is checked with it after the normalisation that keeps the
 * last value of a repeated variable or request, so it rejects only what has no
 * defined meaning (CS-3053).
 *
 * @param ep the object to check, nullptr is accepted
 * @param answer_list receives an error naming the key, the attribute and the
 *        object on the first duplicate found
 * @param object_name the object named in the error, e.g. its type and name
 *
 * @return true if no unique attribute holds a key twice, false otherwise
 *
 * @note MT-NOTE: ocs_object_verify_unique_keys() is MT safe
 *
 * @see #ocs_object_list_verify_unique_keys
 */
bool
ocs_object_verify_unique_keys(const lListElem *ep, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              const char *object_name) {
   std::vector<ocs::RepeatedKey> found;
   find_in_object(ep, "", found);
   if (found.empty()) {
      return true;
   }
   add_repeated_key_error(answer_list, found.front(), object_name);
   return false;
}

/**
 * @brief Check that no unique attribute of a list holds the same key twice
 *
 * Checks the unique attributes of the list itself, then every element as
 * #ocs_object_verify_unique_keys does.
 *
 * @param lp the list to check, nullptr is accepted
 * @param answer_list receives an error naming the key, the attribute and the
 *        object on the first duplicate found
 * @param object_name the object named in the error
 *
 * @return true if no unique attribute holds a key twice, false otherwise
 *
 * @note MT-NOTE: ocs_object_list_verify_unique_keys() is MT safe
 */
bool
ocs_object_list_verify_unique_keys(const lList *lp, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   const char *object_name) {
   std::vector<ocs::RepeatedKey> found;
   find_in_list(lp, "", found);
   if (found.empty()) {
      return true;
   }
   add_repeated_key_error(answer_list, found.front(), object_name);
   return false;
}

/**
 * @brief Remove the elements of a list that repeat an earlier string key
 *
 * For lists that are sets, such as queue references or access lists, where a
 * name given twice adds nothing. The first element of each key stays, later
 * ones are removed (CS-3053).
 *
 * @param lp the list, nullptr is accepted
 * @param nm the string attribute holding the key
 *
 * @note MT-NOTE: ocs_object_list_remove_repeated_keys() is MT safe
 */
void
ocs_object_list_remove_repeated_keys(lList *lp, const int nm) {
   if (lp == nullptr) {
      return;
   }

   std::unordered_set<std::string> seen;
   lListElem *next = lFirstRW(lp);
   while (lListElem *ep = next) {
      next = lNextRW(ep);
      const char *key = lGetString(ep, nm);
      if (key != nullptr && !seen.insert(key).second) {
         lRemoveElem(lp, &ep);
      }
   }
}

namespace {
   /** @brief How a list with a repeated key is repaired */
   enum class Repair {
      None,      ///< not repaired; the repetition is an error
      KeepLast,  ///< a definition list: the last value of a name stays
      KeepFirst  ///< a set: the first element of a name stays
   };

   /**
    * @brief How the input paths of CS-3053 repair a repeated key in a list of this type
    * @param lp the list
    * @param[out] nm the key attribute the repair works on
    * @param[out] value_nm the value attribute of a definition list, unchanged for a set
    * @return the repair, Repair::None for a type that the input paths reject instead
    */
   Repair
   repair_of(const lList *lp, int &nm, int &value_nm) {
      const lDescr *dp = lGetListDescr(lp);
      if (lGetPosInDescr(dp, VA_variable) >= 0) {
         nm = VA_variable;
         value_nm = VA_value;
         return Repair::KeepLast;
      }
      if (lGetPosInDescr(dp, CE_name) >= 0) {
         nm = CE_name;
         value_nm = CE_stringval;
         return Repair::KeepLast;
      }
      if (lGetPosInDescr(dp, QR_name) >= 0) {
         nm = QR_name;
         return Repair::KeepFirst;
      }
      if (lGetPosInDescr(dp, UE_name) >= 0) {
         nm = UE_name;
         return Repair::KeepFirst;
      }
      return Repair::None;
   }
} // namespace

/**
 * @brief Repair the repeated keys of an object as the input paths do, and report those that cannot be
 *
 * For spooled objects written by an earlier version (N-H-18). A repeated key is repaired as the input paths of
 * CS-3053 repair it: a variable (`VA_Type`) and a complex value (`CE_Type`) keep their last value, a repeated
 * queue reference (`QR_Type`) or user set entry (`UE_Type`) is dropped and the first one stays. A repeated key
 * in any other list, or in another attribute of these types, is not repaired.
 *
 * @param ep the object, nullptr is accepted
 * @param[out] repairs receives one line per repaired repetition, `<path> "<key>"`, separated by ", "
 * @param[out] answer_list receives one error per repetition that is not repaired, naming the key, the
 *             attribute and the object with the path of the list
 * @param object_name the object named in the errors
 *
 * @return true if no repeated key is left, false otherwise
 *
 * @note MT-NOTE: ocs_object_repair_repeated_keys() is MT safe
 *
 * @see #ocs_object_find_repeated_keys
 */
bool
ocs_object_repair_repeated_keys(lListElem *ep, std::string &repairs, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const char *object_name) {
   std::vector<ocs::RepeatedKey> found;
   find_in_object(ep, "", found);

   // the text first, while every list is intact
   for (const auto &repeated : found) {
      if (int nm, value_nm; repair_of(repeated.list, nm, value_nm) != Repair::None && repeated.nm == nm) {
         if (!repairs.empty()) {
            repairs += ", ";
         }
         repairs += repeated.path + " \"" + repeated.key + "\"";
      }
   }

   // backwards, so a nested list is repaired before an element holding it may be removed; each list once
   std::unordered_set<const lList *> repaired;
   for (auto it = found.rbegin(); it != found.rend(); ++it) {
      int nm;
      int value_nm = NoName;
      const Repair repair = repair_of(it->list, nm, value_nm);
      if (repair == Repair::None || it->nm != nm || !repaired.insert(it->list).second) {
         continue;
      }
      // the list belongs to ep, which the caller holds for writing
      lList *lp = const_cast<lList *>(it->list);
      if (repair == Repair::KeepLast) {
         cull_compress_definition_list(lp, nm, value_nm, 0);
      } else {
         ocs_object_list_remove_repeated_keys(lp, nm);
      }
   }

   found.clear();
   find_in_object(ep, "", found);
   for (const auto &repeated : found) {
      const std::string where = std::string(object_name != nullptr ? object_name : "") + ", " + repeated.path;
      add_repeated_key_error(answer_list, repeated, where.c_str());
   }
   return found.empty();
}

/**
 * @brief Move the elements of one list into another, without ever holding a key twice
 *
 * For the input paths that collect the entries of several switches into one list (CS-3053). The rule of the
 * list's type decides about a repeated key: a variable (`VA_Type`) or a resource request (`CE_Type`) keeps its
 * last value, so the earlier element is removed and the new one appended; in any other list with a unique string
 * attribute the first element stays and the repeating one is dropped. A list without a unique string attribute
 * is appended as it is. The decision is taken before an element is added, so the target never holds a key twice,
 * not even for a moment, as the debug check of N-H-18 demands.
 *
 * @param[in,out] lpp the target list, created if nullptr
 * @param[in,out] src the elements to move; the list is freed and set to nullptr
 *
 * @note MT-NOTE: ocs_object_list_add_unique() is MT safe
 *
 * @see #ocs_object_list_remove_repeated_keys
 */
void
ocs_object_list_add_unique(lList **lpp, lList **src) {
   if (lpp == nullptr || src == nullptr || *src == nullptr) {
      return;
   }

   const lDescr *dp = lGetListDescr(*src);
   int nm = NoName;
   bool keep_last = false;
   if (lGetPosInDescr(dp, VA_variable) >= 0) {
      nm = VA_variable;
      keep_last = true;
   } else if (lGetPosInDescr(dp, CE_name) >= 0) {
      nm = CE_name;
      keep_last = true;
   } else {
      for (const auto f : ocs_cull_present_fields(dp)) {
         if (mt_is_unique(dp[f.pos].mt) && mt_get_type(dp[f.pos].mt) == lStringT) {
            nm = dp[f.pos].nm;
            break;
         }
      }
   }

   if (*lpp == nullptr) {
      *lpp = lCreateList(lGetListName(*src), dp);
   }
   if (nm == NoName) {
      lAddList(*lpp, src);
      return;
   }

   while (lListElem *ep = lFirstRW(*src)) {
      ep = lDechainElem(*src, ep);
      const char *key = lGetString(ep, nm);
      lListElem *held = key != nullptr ? lGetElemStrRW(*lpp, nm, key) : nullptr;
      if (held == nullptr) {
         lAppendElem(*lpp, ep);
      } else if (keep_last) {
         lRemoveElem(*lpp, &held);
         lAppendElem(*lpp, ep);
      } else {
         lFreeElem(&ep);
      }
   }
   lFreeList(src);
}

/**
 * @brief Verify ulong attribute not null
 *
 * Verify that a certain ulong attribute in an object is not 0
 *
 * @tparam F the 32 bit field to verify, of the element's object type
 * @param ep object to verify
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: object_verify_ulong_not_null() is MT safe
 *
 * @see #object_verify_string_not_null
 */
template <auto F>
bool
object_verify_ulong_not_null(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   static_assert(ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::Uint32, "a 32 bit field");
   bool ret = true;

   if (ocs::tom::get<F>(ep) == 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_ULONG_NOT_NULL_S,
                              ocs::tom::FieldTraits<F>::info.name);
      ret = false;
   }

   return ret;
}

/// @cond explicit instantiations for the fields in use
template bool object_verify_ulong_not_null<JB_job_number>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_not_null<JAT_task_number>(ocs::tom::ConstElement<ocs::tom::JAT> ep,
                                                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_not_null<JG_slots>(ocs::tom::ConstElement<ocs::tom::JG> ep,
                                                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_not_null<PETR_jobid>(ocs::tom::ConstElement<ocs::tom::PETR> ep,
                                                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_not_null<PETR_jataskid>(ocs::tom::ConstElement<ocs::tom::PETR> ep,
                                                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
/// @endcond

/**
 * @brief Verify ulong attribute null
 *
 * Verify that a certain ulong attribute in an object is 0
 *
 * @tparam F the 32 bit field to verify, of the element's object type
 * @param ep object to verify
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: object_verify_ulong_null() is MT safe
 *
 * @see #object_verify_string_not_null
 */
template <auto F>
bool
object_verify_ulong_null(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   static_assert(ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::Uint32, "a 32 bit field");
   bool ret = true;

   if (const uint32_t value = ocs::tom::get<F>(ep); value != 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_ULONG_NULL_SU,
                              ocs::tom::FieldTraits<F>::info.name, value);
      ret = false;
   }

   return ret;
}

/// @cond explicit instantiations for the fields in use
template bool object_verify_ulong_null<JB_job_number>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_null<JB_version>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_null<JB_script_size>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong_null<JB_override_tickets>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
/// @endcond

/**
 * @brief Verify that a certain ulong64 attribute in an object is 0
 *
 * @tparam F the 64 bit field to verify, of the element's object type
 * @param ep object to verify
 * @param[out] answer_list answer list to pass back error messages
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: object_verify_ulong64_null() is MT safe
 */
template <auto F>
bool
object_verify_ulong64_null(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   static_assert(ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::Uint64, "a 64 bit field");
   bool ret = true;

   if (const uint64_t value = ocs::tom::get<F>(ep); value != 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_ULONG64_NULL_SU,
                              ocs::tom::FieldTraits<F>::info.name, value);
      ret = false;
   }

   return ret;
}

/// @cond explicit instantiations for the fields in use
template bool object_verify_ulong64_null<JB_soft_wallclock_gmt>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_ulong64_null<JB_hard_wallclock_gmt>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
/// @endcond

/**
 * @brief Verify double attribute null
 *
 * Verify that a certain double attribute in an object is 0
 *
 * @tparam F the double field to verify, of the element's object type
 * @param ep object to verify
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: object_verify_double_null() is MT safe
 *
 * @see #object_verify_string_not_null
 */
template <auto F>
bool
object_verify_double_null(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   static_assert(ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::Double, "a double field");
   bool ret = true;

   if (const double value = ocs::tom::get<F>(ep); value != 0.0) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_DOUBLE_NULL_SD,
                              ocs::tom::FieldTraits<F>::info.name, value);
      ret = false;
   }

   return ret;
}

/// @cond explicit instantiations for the fields in use
template bool object_verify_double_null<JB_urg>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_double_null<JB_nurg>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                 ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_double_null<JB_rrcontr>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_double_null<JB_dlcontr>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_double_null<JB_wtcontr>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                    ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
/// @endcond

/**
 * @brief Verify string attribute not null
 *
 * Verifies that a string attribute of a certain object is not nullptr.
 *
 * @tparam F the string field to verify, of the element's object type
 * @param ep the object to verify
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: object_verify_string_not_null() is MT safe
 *
 * @see #object_verify_ulong_not_null
 */
template <auto F>
bool
object_verify_string_not_null(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   static_assert(ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::String, "a string field");
   bool ret = true;

   if (ocs::tom::get<F>(ep) == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_STRING_NOT_NULL_S,
                              ocs::tom::FieldTraits<F>::info.name);
      ret = false;
   }

   return ret;
}

/// @cond explicit instantiations for the fields in use
template bool object_verify_string_not_null<JB_job_name>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_string_not_null<JB_owner>(ocs::tom::ConstElement<ocs::tom::JB> ep,
                                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template bool object_verify_string_not_null<PETR_owner>(ocs::tom::ConstElement<ocs::tom::PETR> ep,
                                                        ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
/// @endcond

/**
 * @brief Verify string attribute expression syntax
 *
 * Verifies that the value of a complex entry of a string like type is an
 * expression in correct format; entries of other types pass.
 *
 * @param elem the complex entry to verify
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: object_verify_expression_syntax() is MT safe
 */
bool
object_verify_expression_syntax(const ocs::tom::ConstElement<ocs::tom::CE> elem,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(OBJECT_LAYER);
   bool ret = true;
   switch (const auto type = static_cast<ocs::CEntry::Type>(ocs::tom::get<CE_valtype>(elem))) {
      case ocs::CEntry::Type::STR:
      case ocs::CEntry::Type::CSTR:
      case ocs::CEntry::Type::RESTR:
      case ocs::CEntry::Type::HOST:
         if (sge_eval_expression(type, ocs::tom::get<CE_stringval>(elem), "*", answer_list) == -1) {
            ret = false;
         }
         break;
      default:;
   }
   DRETURN(ret);
}


/**
 * @brief Verify that a text is usable as an object name
 *
 * Two checks are applied:
 *
 * 1. the name must not start with a digit, and
 * 2. it must pass the general key check for the qsub character table.
 *
 * A detailed problem description is added to the answer list. A name that is
 * not set at all passes. The typed object_verify_name() reads the name from its
 * field and calls this.
 *
 * @param[out] answer_list receives the problem description
 * @param object_name the name, or nullptr
 * @param attribute_name the name of the attribute holding it, for the message of the key check
 *
 * @return 0 when the name is usable, otherwise `STATUS_EUNKNOWN`
 *
 * @note MT-NOTE: ocs_object_verify_name_value() is MT safe
 */
int
ocs_object_verify_name_value(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *object_name,
                             const char *attribute_name) {
   DENTER(TOP_LAYER);
   int ret = 0;

   if (object_name != nullptr) {
      if (isdigit(object_name[0])) {
         ERROR(MSG_OBJECT_INVALID_NAME_S, object_name);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      } else {
         if (verify_str_key(answer_list, object_name, MAX_VERIFY_STRING, attribute_name, QSUB_TABLE) != STATUS_OK) {
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   DRETURN(ret);
}


/**
 * @brief Verify validness of a jobs PE range request
 *
 * Verifies a jobs PE range is valid. Currently the following is done
 * - make PE range list normalized and ascending
 * - ensure PE range min/max not 0
 * - in case multiple PEs match the PE request in GEEE ensure
 *   the urgency slots setting is non-ambiguous
 *
 * @param alpp Returns answer list with error context.
 * @param pe_name PE request
 * @param pe_range PE range to be verified; it is sorted and compressed in place
 * @param object_descr object description for user messages
 *
 * @return STATUS_OK on success
 */
int
object_verify_pe_range(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *pe_name,
                       const ocs::tom::List<ocs::tom::RN> pe_range, const char *object_descr) {
   DENTER(TOP_LAYER);

   /* ensure jobs PE range list request is normalized and ascending */
   range_list_sort_uniq_compress(pe_range, nullptr, true);

   for (const auto range_elem : pe_range) {
      const unsigned long pe_range_min = ocs::tom::get<RN_min>(range_elem);
      const unsigned long pe_range_max = ocs::tom::get<RN_max>(range_elem);
      DPRINTF("pe max = %ld, pe min = %ld\n", pe_range_max, pe_range_min);
      if (pe_range_max == 0 || pe_range_min == 0) {
         ERROR(MSG_OBJECT_PERANGEMUSTBEGRZERO_S, object_descr);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* PE slot ranges used in conjunction with wildcards can cause number of slots
    finally being used for urgency value computation be ambiguous. We reject such
    jobs */
   if (range_list_get_number_of_ids(pe_range) > 1) {
      // the master list of the data store is a process-lifetime global; it holds parallel environments
      const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
      const ocs::tom::ConstElement<ocs::tom::PE> reference_pe(pe_list_find_matching(master_pe_list, pe_name));
      const int nslots = pe_urgency_slots(reference_pe, ocs::tom::get<PE_urgency_slots>(reference_pe), pe_range);
      for (const auto pe : master_pe_list) {
         if (pe_is_matching(pe, pe_name) &&
             nslots != pe_urgency_slots(pe, ocs::tom::get<PE_urgency_slots>(pe), pe_range)) {
            ERROR(MSG_OBJECT_WILD_RANGE_AMBIGUOUS_S, object_descr);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }

   DRETURN(STATUS_OK);
}

/**
 * @brief Remove multiple requests for one resource
 *
 * remove multiple requests for one resource
 * this can't be done fully in clients without having complex definitions
 * -l arch=linux -l a=sun4
 *
 * The list is walked from its end; of several requests for one resource the
 * last one stays. A request for `slots` is rejected.
 *
 * @param alpp Returns answer list with error context.
 * @param rl the resource requests, `CE_Type`; may be an empty handle
 * @param object_descr object description for user messages
 *
 * @return 0 on success, -1 for an explicit `slots` request
 */
int compress_ressources(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::List<ocs::tom::CE> rl,
                        const char *object_descr) {
   DENTER(TOP_LAYER);

   for (auto ep = rl.back(); ep; ep = ep.prev()) {
      const char *attr_name = ocs::tom::get<CE_name>(ep);

      /* ensure 'slots' is not requested explicitly */
      if (!strcmp(attr_name, "slots")) {
         ERROR(MSG_OBJECT_NODIRECTSLOTS_S, object_descr);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }

      /* remove all previous requests with the same name; the next one is fetched before an erase */
      auto prev = ep.prev();
      while (const auto rm_ep = prev) {
         prev = rm_ep.prev();
         if (!strcmp(ocs::tom::get<CE_name>(rm_ep), attr_name)) {
            DPRINTF("resource request -l " SFN "=" SFN " overrides previous -l " SFN "=" SFN "\n",
                    attr_name, ocs::tom::get<CE_stringval>(ep), attr_name, ocs::tom::get<CE_stringval>(rm_ep));
            rl.erase(rm_ep);
         }
      }
   }

   DRETURN(0);
}

/**
 * @brief Unpack and verify an object
 *
 * Unpacks the given packbuffer.
 * If unpacking was successful, verifies the object
 * against the given descriptor.
 *
 * @param answer_list answer list to report errors
 * @param pb the packbuffer containing the object
 * @param epp element pointer to pass back the unpacked object
 * @param descr the expected object type
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: object_unpack_elem_verify() is MT safe
 *
 * @see #object_verify_cull
 */
bool object_unpack_elem_verify(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, sge_pack_buffer *pb, lListElem **epp,
                               const lDescr *descr) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (pb == nullptr || epp == nullptr || descr == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_NULLELEMENTPASSEDTO_S,
                              "object_unpack_elem_verify");
      ret = false;
   }

   if (ret) {
      if (cull_unpack_elem(pb, epp, nullptr) != 0) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_COM_UNPACKOBJ_S,
                                 object_get_name(descr));
         ret = false;
      }
   }

   if (ret) {
      if (!object_verify_cull(*epp, descr)) {
         lFreeElem(epp);
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_OBJECT_STRUCTURE_ERROR);
         ret = false;
      }
   }

   DRETURN(ret);
}
