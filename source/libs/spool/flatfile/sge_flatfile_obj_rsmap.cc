/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "uti/sge_stdlib.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull_multitype.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/ocs_CEntry.h"

#include "msg_spoollib_flatfile.h"
#include "sge_flatfile_obj_rsmap.h"

/**
 * Return true if two RESL_properties lists carry identical characteristics —
 * same set of {CE_name, CE_stringval} pairs, order-independent. Empty (or
 * both nullptr) lists count as identical.
 */
static bool
properties_equal(const lList *a, const lList *b) {
   const int na = (a == nullptr) ? 0 : lGetNumberOfElem(a);
   const int nb = (b == nullptr) ? 0 : lGetNumberOfElem(b);
   if (na != nb) return false;
   if (na == 0) return true;

   const lListElem *ep;
   for_each_ep (ep, a) {
      const char *name = lGetString(ep, CE_name);
      const lListElem *match = lGetElemStr(b, CE_name, name);
      if (match == nullptr) return false;
      const char *va = lGetString(ep, CE_stringval);
      const char *vb = lGetString(match, CE_stringval);
      if (va == nullptr || vb == nullptr) {
         if (va != vb) return false;  // one nullptr, one not
      } else if (strcmp(va, vb) != 0) {
         return false;
      }
   }
   return true;
}

/**
 * Add an RSMAP id to a CE element, optionally attaching per-instance
 * properties.
 *
 * Duplicate-id semantics (see CS-1338): the same id may appear multiple
 * times in the input to model N-way sharing of one physical resource. In
 * that case every occurrence must carry identical characteristics (or all
 * be bare); the reader stores the property list once and increments
 * RESL_amount to record the multiplicity. A mismatch — including
 * bare-vs-annotated — is a syntax error, reported via `alpp` with the
 * enclosing `rsmap_name` for context.
 *
 * Ownership of `*properties` is transferred to the CE on success (either
 * attached to the RESL or, for a matching duplicate id, freed). On failure
 * the properties list is also freed. `*properties` is always nulled out
 * before return so the caller cannot double-free.
 *
 * @param centry      CE_Type element whose CE_resource_map_list is being built
 * @param id          the RSMAP id to add
 * @param properties  in/out list of CE_Type property elements to attach to
 *                    the RESL; may be nullptr, and *properties may be
 *                    nullptr (both mean "no properties")
 * @param alpp        answer list for conflict messages
 * @param rsmap_name  the enclosing RSMAP complex name, used only for error text
 * @return            true on success (id added or matched); false on
 *                    duplicate-id characteristic conflict
 */
static bool
store_resl(lListElem *centry, const char *id, lList **properties,
           lList **alpp, const char *rsmap_name) {
   lList *incoming = (properties != nullptr) ? *properties : nullptr;
   lListElem *resl = lGetSubStrRW(centry, RESL_value, id, CE_resource_map_list);
   if (resl == nullptr) {
      resl = lAddSubStr(centry, RESL_value, id, CE_resource_map_list, RESL_Type);
      if (resl == nullptr) {
         if (properties != nullptr) lFreeList(properties);
         return true;  // allocation failure is not this function's error to report
      }
      lAddUlong(resl, RESL_amount, 1);
      if (incoming != nullptr) {
         lSetList(resl, RESL_properties, incoming);
         *properties = nullptr;
      }
      return true;
   }

   // duplicate id: incoming characteristics (if any) must match stored ones
   const lList *stored = lGetList(resl, RESL_properties);
   if (!properties_equal(stored, incoming)) {
      answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_CHARACTERISTIC_CONFLICT_SS,
                              rsmap_name, id);
      if (properties != nullptr) lFreeList(properties);
      return false;
   }

   lAddUlong(resl, RESL_amount, 1);
   if (properties != nullptr) lFreeList(properties);  // stored copy wins
   return true;
}

/**
 * Split an RSMAP id token into its id and characteristics list. A token like
 *    gpu0[device=/dev/nvidia0,memory=80G]
 * yields id="gpu0" and a CE_Type list with two elements carrying CE_name and
 * CE_stringval verbatim; typing and centry-list resolution happen later
 * during validation. Tokens without a '[' are stored as-is with
 * *properties_out = nullptr.
 *
 * @param token             the raw id token grabbed from strtok, e.g.
 *                          "gpu0" or "gpu0[device=/dev/nvidia0,memory=80G]"
 * @param id_out            filled in with the id portion (before '[')
 * @param properties_out    out: newly-allocated CE_Type list holding
 *                          {CE_name, CE_stringval} pairs, or nullptr if the
 *                          token has no characteristics block
 * @param alpp              answer list for parse-error messages
 * @param rsmap_name        the enclosing RSMAP complex name, used only for
 *                          error text
 * @return                  true on success (possibly with no characteristics),
 *                          false on syntax error (answer_list is populated)
 */
static bool
parse_id_characteristics(const char *token, std::string &id_out,
                         lList **properties_out, lList **alpp,
                         const char *rsmap_name) {
   *properties_out = nullptr;

   const char *bracket = strchr(token, RSMAP_CHARACTERISTICS_OPEN);
   if (bracket == nullptr) {
      id_out.assign(token);
      return true;
   }

   // CS-1338: per-instance characteristics are a GCS-only feature. This is the
   // single point where they enter the system from a configuration, so
   // rejecting here keeps every downstream consumer (writer, validator,
   // scheduler booking, execd device isolation) unreachable in an OCS build
   // without needing its own guard.
#if !defined(WITH_EXTENSIONS)
   {
      std::string id_only(token, bracket - token);
      answer_list_add_sprintf(alpp, STATUS_ENOTAVAILABLE, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_CHARACTERISTIC_NOT_AVAILABLE_SS,
                              rsmap_name, id_only.c_str());
      return false;
   }
#endif

   const size_t token_len = strlen(token);
   if (token_len == 0 || token[token_len - 1] != RSMAP_CHARACTERISTICS_CLOSE) {
      answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_RSMAP_CHARACTERISTIC_UNCLOSED_SS,
                              rsmap_name, token);
      return false;
   }

   id_out.assign(token, bracket - token);
   const size_t chars_len = token_len - (bracket - token) - 2;
   std::string chars_buf(bracket + 1, chars_len);

   lList *props = nullptr;
   struct saved_vars_s *ctx = nullptr;
   for (char *ctok = sge_strtok_r(&chars_buf[0], RSMAP_CHARACTERISTIC_SEPARATOR_STR, &ctx);
        ctok != nullptr;
        ctok = sge_strtok_r(nullptr, RSMAP_CHARACTERISTIC_SEPARATOR_STR, &ctx)) {
      char *eq = strchr(ctok, '=');
      if (eq == nullptr) {
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_RSMAP_CHARACTERISTIC_NO_EQ_SSS,
                                 rsmap_name, id_out.c_str(), ctok);
         lFreeList(&props);
         sge_free_saved_vars(ctx);
         return false;
      }
      *eq = '\0';
      const char *name = ctok;
      const char *value = eq + 1;
      if (props == nullptr) {
         props = lCreateList("properties", CE_Type);
      }
      lListElem *cep = lCreateElem(CE_Type);
      lSetString(cep, CE_name, name);
      lSetString(cep, CE_stringval, value);
      lAppendElem(props, cep);
   }
   sge_free_saved_vars(ctx);

   *properties_out = props;
   return true;
}

/**
 * @brief parse an entry of complex_values
 *
 * @param ep        CE_Type element which will store the complex_value
 * @param nm        attribute to fill in (CE_stringval)
 * @param buf       string buffer with the complex value
 * @param alpp      for returning errors
 * @return          1 in case of success, else 0
 */
int
read_CE_stringval_host(lListElem *ep, int nm, const char *buf, lList **alpp) {
   int ret = 1;

   DENTER(TOP_LAYER);

   /*
    * The following code reads both ordinary complex_values (name=value)
    * and RSMAPs (name=amount(ids) like
    *    gpu=2
    *    gpu=2(A B)
    *    gpu=2(1-2)
    *    gpu=4(A B 1-2)
    * and ids with per-instance characteristics, e.g.
    *    gpu=2(gpu0[device=/dev/nvidia0,memory=80G] gpu1[device=/dev/nvidia1,memory=80G])
    * Characteristics are stored verbatim as CE_name/CE_stringval on the
    * RESL_properties list; the centry-list lookup and type-checking happen
    * later, on the qmaster side, in centry_check_rsmap / centry_elem_validate.
    * Characteristics on a range (e.g. 1-3[foo=bar]) are rejected.
    *
    * Any whitespace inside a [...] block is stripped up front. The flatfile
    * scanner turns a '\\' + <newline> line-continuation into a single space,
    * which lets an admin split a long characteristics block across lines:
    *   gpu=1(gpu0[device=/dev/nvidia0;\
    *         memory=80G])
    * The bracket-depth-aware preprocessing below preserves the useful spaces
    * between id-specs (outside brackets) and removes the line-continuation
    * residue (inside brackets) before the strtok-based tokenizer runs.
    */
   const char *rsmap_name = lGetString(ep, CE_name);
   std::string cleaned;
   cleaned.reserve(strlen(buf));
   {
      int depth = 0;
      for (const char *p = buf; *p != '\0'; ++p) {
         if (*p == RSMAP_CHARACTERISTICS_OPEN) {
            ++depth;
            cleaned += *p;
         } else if (*p == RSMAP_CHARACTERISTICS_CLOSE) {
            if (depth > 0) --depth;
            cleaned += *p;
         } else if (depth > 0 && isspace(static_cast<unsigned char>(*p))) {
            // strip whitespace inside [...] (line-continuation friendly)
            continue;
         } else {
            cleaned += *p;
         }
      }
   }

   struct saved_vars_s *context = nullptr;
   char *token;
   if ((token = sge_strtok_r(&cleaned[0], " (", &context))) {
      // first token is the amount
      lSetString(ep, nm, token);
      uint32_t amount = SGE_STRTOU_LONG32(token);

      // the following tokens are individual ids or ranges
      if ((token = sge_strtok_r(nullptr, " )", &context)) != nullptr) {
         uint32_t num_ids = 0;
         do {
            uint32_t range_start = 0;
            uint32_t range_end = 0;
            uint32_t range_step = 0;
            const bool has_characteristics =
                  (strchr(token, RSMAP_CHARACTERISTICS_OPEN) != nullptr);

            if (!has_characteristics &&
                range_parse_get_ids(token, 0, range_start, range_end, range_step)) {
               // bare range
               for (; range_start <= range_end; range_start += range_step) {
                  std::string id_str{std::to_string(range_start)};
                  store_resl(ep, id_str.c_str(), nullptr, alpp, rsmap_name);
                  num_ids++;
               }
            } else {
               // individual id, possibly with characteristics
               std::string id_str;
               lList *properties = nullptr;
               if (!parse_id_characteristics(token, id_str, &properties, alpp,
                                             rsmap_name)) {
                  ret = 0;
                  break;
               }
               // reject characteristics attached to a range spec like "1-3[foo=bar]"
               if (has_characteristics &&
                   range_parse_get_ids(id_str.c_str(), 0, range_start, range_end, range_step)) {
                  answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                          MSG_RSMAP_CHARACTERISTIC_ON_RANGE_SS,
                                          rsmap_name, token);
                  lFreeList(&properties);
                  ret = 0;
                  break;
               }
               if (!store_resl(ep, id_str.c_str(), &properties, alpp, rsmap_name)) {
                  ret = 0;
                  break;
               }
               num_ids++;
            }
         } while ((token = sge_strtok_r(nullptr, " )", &context)));

         // check if data is consistent
         if (ret == 1 && amount != num_ids) {
            answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_RSMAP_INCONSISTENTAMOUNT_SSUU, rsmap_name,
                                    buf, amount, num_ids);
            ret = 0;
         }
         if (ret == 0) {
            lSetList(ep, CE_resource_map_list, nullptr);
         }
      }
   }

   sge_free_saved_vars(context);
   DRETURN(ret);
}

static void
store_item(std::string &str_out, const std::string_view &str_new, const uint32_t amount) {
   for (uint32_t i = 0; i < amount; i++) {
      if (!str_out.empty()) {
         str_out += ' ';
      }
      str_out += str_new;
   }
}

static void
store_range(std::string &str_out, long &range_start, long &range_last, uint32_t amount, const long range_new) {
   // range ended - if it has at least 3 elements
   // then write it as range
   if (range_last - range_start > 1) {
      std::string str_range{};
      str_range += std::to_string(range_start);
      str_range += '-';
      str_range += std::to_string(range_last);
      store_item(str_out, str_range, amount);
   } else {
      // write it as individual entries
      for (long i = range_start; i <= range_last; ++i) {
         store_item(str_out, std::to_string(i), amount);
      }
   }
   range_start = range_last = range_new;
}

/**
 * @brief write an entry of complex_values
 *
 * @param ep        CE_Type element representing a complex_value
 * @param nm        attribute to read from (CE_stringval)
 * @param buf       string buffer we fill in with the data
 * @param alpp      for returning errors
 * @return          1 in case of success, else 0
 */
int
write_CE_stringval_host(const lListElem *ep, int nm, dstring *buffer, lList **alp) {
   DENTER(TOP_LAYER);

   std::string str_out{};

   // add the value of the complex_value to the output buffer
   const char *str_value = lGetString(ep, CE_stringval);
   if (str_value != nullptr) {
      str_out += str_value;
   } else {
      // should never happen - but you never know...
      double dbl_value = lGetDouble(ep, CE_doubleval);
      str_out += std::to_string(dbl_value);
   }
   sge_dstring_append(buffer, str_out.c_str());
   str_out.clear();

   if (static_cast<ocs::CEntry::Type>(lGetUlong(ep, CE_valtype)) == ocs::CEntry::Type::RSMAP) {
      /*
       * fill in the IDs from the CE_resource_map_list
       * we can have individual ids or a range or a combination of both
       *    gpu=2
       *    gpu=2(A B)
       *    gpu=2(1-2)
       *    gpu=4(A B 1-2)
       * and ids with per-instance characteristics, e.g.
       *    gpu=2(gpu0[device=/dev/nvidia0,memory=80G] gpu1[device=/dev/nvidia1,memory=80G])
       * we cannot use range functions from sgeobj lib as:
       *       - there is only the special case of ranges with step size 1
       *       - multiple occurrences of one entry are possible (0 0 0 1 1 1)
       * Range compaction only fires for consecutive numeric ids whose
       * RESL_properties list is empty; as soon as an id carries characteristics
       * the pending range is flushed and the characteristic-bearing id is
       * emitted individually. Characteristics themselves are sorted ascending
       * by CE_name (same convention as centry_list_sort) before emission; the
       * stored CULL order is not modified.
       */
      const lList *resource_map = lGetList(ep, CE_resource_map_list);
      if (resource_map != nullptr && lGetNumberOfElem(resource_map) > 0) {
         long range_start = -1, range_last = -1, range_current;
         uint32_t amount = 0;

         for_each_ep_lv (resource, resource_map) {
            str_value = lGetString(resource, RESL_value);
            amount = lGetUlong(resource, RESL_amount);
            const lList *props = lGetList(resource, RESL_properties);
            const bool has_props = (props != nullptr && lGetNumberOfElem(props) > 0);

            if (has_props) {
               // flush any pending range before emitting a characteristic-bearing id
               if (range_start != -1) {
                  store_range(str_out, range_start, range_last, amount, -1);
               }

               // build "id[k1=v1;k2=v2;...]" with characteristics sorted asc by CE_name
               std::vector<std::pair<std::string, std::string>> pairs;
               pairs.reserve(lGetNumberOfElem(props));
               const lListElem *prop;
               for_each_ep (prop, props) {
                  const char *pname = lGetString(prop, CE_name);
                  const char *pval = lGetString(prop, CE_stringval);
                  pairs.emplace_back(pname != nullptr ? pname : "",
                                     pval != nullptr ? pval : "");
               }
               std::sort(pairs.begin(), pairs.end(),
                         [](const auto &a, const auto &b) { return a.first < b.first; });

               std::string decorated{str_value != nullptr ? str_value : ""};
               decorated += RSMAP_CHARACTERISTICS_OPEN;
               for (size_t i = 0; i < pairs.size(); ++i) {
                  if (i > 0) decorated += RSMAP_CHARACTERISTIC_SEPARATOR;
                  decorated += pairs[i].first;
                  decorated += '=';
                  decorated += pairs[i].second;
               }
               decorated += RSMAP_CHARACTERISTICS_CLOSE;
               store_item(str_out, decorated, amount);
            } else if (sge_str_is_number(str_value)) {
               // can belong to a range
               range_current = strtol(str_value, nullptr, 10);
               if (range_start == -1) {
                  // a new potential range started
                  range_start = range_last = range_current;
               } else {
                  // additional number in range
                  if (range_current == range_last + 1) {
                     // we only support step = 1
                     // continue the range
                     range_last = range_current;
                  } else {
                     // store and start a new range
                     store_range(str_out, range_start, range_last, amount, range_current);
                  }
               }
            } else {
               // it is a name
               // if we have stored range values, then add the range to our output list
               if (range_start != -1) {
                  store_range(str_out, range_start, range_last, amount, -1);
               }

               // add the non number value to our output list
               store_item(str_out, str_value, amount);
            }
         }
         // we might end up having stored range values, add the range to our output list
         if (range_start != -1) {
            store_range(str_out, range_start, range_last, amount, -1);
         }

         // add the RSMAP entries to the output buffer
         sge_dstring_append_char(buffer, '(');
         sge_dstring_append(buffer, str_out.c_str());
         sge_dstring_append_char(buffer, ')');
      }
   }

   DRETURN(1);
}

